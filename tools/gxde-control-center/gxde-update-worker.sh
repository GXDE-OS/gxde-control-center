#!/bin/bash

export DEBIAN_FRONTEND=noninteractive
export LANGUAGE=en_US

UPDATE_LOG=/tmp/gxde-control-center-update-log.txt
UPDATE_STATUS=/tmp/gxde-control-center-update-status.txt
UPGRADE_LOG=/tmp/gxde-control-center-upgrade-log.txt
UPGRADE_STATUS=/tmp/gxde-control-center-upgrade-status.txt

if command -v aptss >/dev/null 2>&1; then
    APT_CMD=aptss
else
    APT_CMD=/usr/bin/apt
fi

APTSS_APT_CONF=/opt/durapps/spark-store/bin/apt-fast-conf/aptss-apt.conf

apt_get_install_from_cache() {
    if [ -f "$APTSS_APT_CONF" ]; then
        env LANGUAGE=en_US DEBIAN_FRONTEND=noninteractive /usr/bin/apt-get -c "$APTSS_APT_CONF" install -y --no-download --only-upgrade "$@"
    else
        env LANGUAGE=en_US DEBIAN_FRONTEND=noninteractive /usr/bin/apt-get install -y --no-download --only-upgrade "$@"
    fi
}

run_as_root() {
    if [ "$(id -u)" != "0" ]; then
        exec pkexec "$0" "$@"
    fi
}

write_error_status() {
    local log_file="$1"
    local status_file="$2"

    if [ -f "$log_file" ]; then
        awk '/^E:|Package manager quit with exit code\./ { print }' "$log_file" > "$status_file"
    else
        : > "$status_file"
    fi

    chmod 666 "$status_file" 2>/dev/null || true
}

fail_with_log() {
    local stage="$1"
    local ret="$2"

    echo "gxde-update-worker: ${stage} failed with exit code ${ret}" >&2
    exit "$ret"
}

size_to_bytes() {
    local value="$1"
    local unit="$2"

    awk -v value="$value" -v unit="$unit" '
        BEGIN {
            factor = 1
            if (unit ~ /^kB$/) factor = 1000
            else if (unit ~ /^MB$/) factor = 1000 * 1000
            else if (unit ~ /^GB$/) factor = 1000 * 1000 * 1000
            else if (unit ~ /^KiB$/) factor = 1024
            else if (unit ~ /^MiB$/) factor = 1024 * 1024
            else if (unit ~ /^GiB$/) factor = 1024 * 1024 * 1024
            printf "%.0f\n", value * factor
        }'
}

validate_packages() {
    local package

    for package in "$@"; do
        if ! printf '%s\n' "$package" | grep -Eq '^[a-z0-9][a-z0-9+.-]+(:[a-z0-9]+)?$'; then
            echo "Invalid package name: $package" >&2
            exit 2
        fi
    done
}

download_to_cache() {
    env LANGUAGE=en_US DEBIAN_FRONTEND=noninteractive ${APT_CMD} install -d -y --only-upgrade "$@" 2>&1 | tr '\r' '\n' | while IFS= read -r line; do
        printf '%s\n' "$line"
        speed=$(printf '%s\n' "$line" | sed -n 's/.*DL:\([^ ]*\).*/\1/p')
        progress=$(printf '%s\n' "$line" | sed -n 's/.*(\([0-9][0-9]*\)%).*/\1/p')

        if [ -n "$speed" ]; then
            echo "# Downloading updates... ${speed}"
        fi

        if [ -n "$progress" ]; then
            echo "$((progress * 80 / 100))"
        fi
    done
    return "${PIPESTATUS[0]}"
}

install_from_cache() {
    echo "# Installing updates from cache..."
    echo 85
    apt_get_install_from_cache "$@"
}

case "$1" in
    update)
        run_as_root "$@"
        env LANGUAGE=en_US ${APT_CMD} update 2>&1 | tee "$UPDATE_LOG"
        write_error_status "$UPDATE_LOG" "$UPDATE_STATUS"

        if [ -s "$UPDATE_STATUS" ]; then
            exit 1
        fi
        ;;
    upgradable-list)
        run_as_root "$@"
        output=$(env LANGUAGE=en_US ${APT_CMD} list --upgradable 2>/dev/null | awk 'NR>1')

        IFS_OLD="$IFS"
        IFS=$'\n'
        for line in $output; do
            PKG_NAME=$(printf '%s\n' "$line" | awk -F '/' '{print $1}')
            PKG_NEW_VER=$(printf '%s\n' "$line" | awk -F ' ' '{print $2}')
            PKG_CUR_VER=$(printf '%s\n' "$line" | awk -F ' ' '{print $6}' | awk -F ']' '{print $1}')
            PKG_STA=$(dpkg-query -W -f='${db:Status-Want}' "$PKG_NAME" 2>/dev/null || true)

            if [ -n "$PKG_NAME" ] && [ "$PKG_STA" != "hold" ]; then
                printf '%s\t%s\t%s\n' "$PKG_NAME" "$PKG_NEW_VER" "$PKG_CUR_VER"
            fi
        done
        IFS="$IFS_OLD"
        ;;
    download-size)
        run_as_root "$@"
        shift
        validate_packages "$@"

        if [ "$#" -eq 0 ]; then
            echo 0
            exit 0
        fi

        output=$(env LANGUAGE=en_US DEBIAN_FRONTEND=noninteractive ${APT_CMD} install --simulate --only-upgrade "$@" 2>/dev/null || true)
        need_line=$(printf '%s\n' "$output" | awk '/^Need to get / { line = $0 } END { print line }')

        if [ -z "$need_line" ]; then
            echo 0
            exit 0
        fi

        amount=$(printf '%s\n' "$need_line" | awk '{ print $4 }')
        unit=$(printf '%s\n' "$need_line" | awk '{ print $5 }')
        size_to_bytes "$amount" "$unit"
        ;;
    upgrade)
        run_as_root "$@"
        shift
        validate_packages "$@"

        if [ "$#" -gt 0 ]; then
            {
                download_to_cache "$@"
                download_ret="$?"
                if [ "$download_ret" -ne 0 ]; then
                    fail_with_log "download" "$download_ret"
                fi

                install_from_cache "$@"
                install_ret="$?"
                echo 100
                if [ "$install_ret" -ne 0 ]; then
                    fail_with_log "install" "$install_ret"
                fi

                exit 0
            } 2>&1 | tee "$UPGRADE_LOG"
        else
            env LANGUAGE=en_US DEBIAN_FRONTEND=noninteractive ${APT_CMD} upgrade -y 2>&1 | tee "$UPGRADE_LOG"
        fi

        ret=${PIPESTATUS[0]}
        write_error_status "$UPGRADE_LOG" "$UPGRADE_STATUS"

        if [ "$ret" -ne 0 ] || [ -s "$UPGRADE_STATUS" ]; then
            echo "gxde-update-worker: upgrade failed, ret=${ret}, status=$(cat "$UPGRADE_STATUS" 2>/dev/null)" >&2
            if [ -f "$UPGRADE_LOG" ]; then
                echo "gxde-update-worker: full log from ${UPGRADE_LOG}:" >&2
                sed 's/^/  /' "$UPGRADE_LOG" >&2
            fi
            exit 1
        fi
        ;;
    clean-log)
        rm -f "$UPDATE_LOG" "$UPDATE_STATUS" "$UPGRADE_LOG" "$UPGRADE_STATUS"
        ;;
    *)
        echo "Usage: $0 {update|upgradable-list|download-size [package ...]|upgrade [package ...]|clean-log}" >&2
        exit 2
        ;;
esac
