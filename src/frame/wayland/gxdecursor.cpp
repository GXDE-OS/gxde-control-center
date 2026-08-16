/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstring>

#include <QtGlobal>
#include <wayland-client.h>

#include "gxdecursor.h"
#include "ukui-shell-client-protocol.h"

namespace {

struct CursorQuery {
    QString outputName;
    ukui_shell* shell = nullptr;
    bool done = false;
};

void shellCurrentOutput(void* data, ukui_shell*, const char* output,
        const char*) {
    CursorQuery* query = static_cast<CursorQuery*>(data);
    if (query->outputName.isEmpty() && output)
        query->outputName = QString::fromUtf8(output);
}

void shellDone(void* data, ukui_shell*) {
    static_cast<CursorQuery*>(data)->done = true;
}

const ukui_shell_listener shellListener = {
    shellCurrentOutput,
    shellDone,
};

void registryGlobal(void* data, wl_registry* registry, uint32_t name,
        const char* interface, uint32_t version) {
    CursorQuery* query = static_cast<CursorQuery*>(data);
    if (std::strcmp(interface, ukui_shell_interface.name) != 0)
        return;
    if (query->shell ||
            version < UKUI_SHELL_GET_CURRENT_OUTPUT_SINCE_VERSION)
        return;

    query->shell = static_cast<ukui_shell*>(
        wl_registry_bind(registry, name, &ukui_shell_interface,
            qMin(version, uint32_t(
                ukui_shell_interface.version))));
    ukui_shell_add_listener(query->shell, &shellListener, query);
}

void registryGlobalRemove(void*, wl_registry*, uint32_t) {
}

const wl_registry_listener registryListener = {
    registryGlobal,
    registryGlobalRemove,
};

}  // namespace

QString GxdeCursor::currentOutputName() {
    wl_display* display = wl_display_connect(nullptr);
    if (!display)
        return QString();

    CursorQuery query;
    wl_registry* registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registryListener, &query);

    if (wl_display_roundtrip(display) >= 0 && query.shell) {
        wl_display_roundtrip(display);
        if (!query.done) {
            ukui_shell_get_current_output(query.shell);
            wl_display_roundtrip(display);
        }
        wl_proxy_destroy(reinterpret_cast<wl_proxy*>(query.shell));
    }

    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return query.outputName;
}
