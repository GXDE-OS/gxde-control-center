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
            env LANGUAGE=en_US DEBIAN_FRONTEND=noninteractive ${APT_CMD} install -y --only-upgrade "$@" 2>&1 | tee "$UPGRADE_LOG"
        else
            env LANGUAGE=en_US DEBIAN_FRONTEND=noninteractive ${APT_CMD} upgrade -y 2>&1 | tee "$UPGRADE_LOG"
        fi

        ret=${PIPESTATUS[0]}
        write_error_status "$UPGRADE_LOG" "$UPGRADE_STATUS"

        if [ "$ret" -ne 0 ] || [ -s "$UPGRADE_STATUS" ]; then
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
