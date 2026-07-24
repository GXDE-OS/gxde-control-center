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

#include <algorithm>
#include <cstring>

#include <QDBusMetaType>
#include <wayland-client.h>

#include "gxdescreen.h"
#include "kywc-output-v1-client-protocol.h"

namespace {

struct ModeData {
    GxdeScreen::Mode mode;
    kywc_output_mode_v1* proxy = nullptr;
};

struct OutputData {
    QString name;
    QList<ModeData*> modes;
    kywc_output_mode_v1* currentMode = nullptr;
    kywc_output_v1* proxy = nullptr;

    ~OutputData() {
        qDeleteAll(modes);
    }
};

struct ModeQuery {
    QString outputName;
    QList<OutputData*> outputs;
    kywc_output_manager_v1* manager = nullptr;
    bool collectOutputs = true;
    bool identified = false;

    ~ModeQuery() {
        qDeleteAll(outputs);
    }
};

void modeSize(void* data, kywc_output_mode_v1*, int32_t width,
              int32_t height) {
    ModeData* mode = static_cast<ModeData*>(data);
    mode->mode.width = width;
    mode->mode.height = height;
}

void modeRefresh(void* data, kywc_output_mode_v1*, int32_t refresh) {
    static_cast<ModeData*>(data)->mode.refresh = refresh;
}

void modePreferred(void* data, kywc_output_mode_v1*) {
    static_cast<ModeData*>(data)->mode.preferred = true;
}

void modeFinished(void*, kywc_output_mode_v1*) {
}

const kywc_output_mode_v1_listener modeListener = {
    modeSize,
    modeRefresh,
    modePreferred,
    modeFinished,
};

void outputName(void* data, kywc_output_v1*, const char* name) {
    static_cast<OutputData*>(data)->name = QString::fromUtf8(name);
}

void outputString(void*, kywc_output_v1*, const char*) {
}

void outputPhysicalSize(void*, kywc_output_v1*, int32_t, int32_t) {
}

void outputMode(void* data, kywc_output_v1*, kywc_output_mode_v1* proxy) {
    OutputData* output = static_cast<OutputData*>(data);
    ModeData* mode = new ModeData;
    mode->proxy = proxy;
    output->modes.append(mode);
    kywc_output_mode_v1_add_listener(proxy, &modeListener, mode);
}

void outputUint(void*, kywc_output_v1*, uint32_t) {
}

void outputInt(void*, kywc_output_v1*, int32_t) {
}

void outputCurrentMode(void* data, kywc_output_v1*,
                       kywc_output_mode_v1* mode) {
    static_cast<OutputData*>(data)->currentMode = mode;
}

void outputPosition(void*, kywc_output_v1*, int32_t, int32_t) {
}

void outputScale(void*, kywc_output_v1*, wl_fixed_t) {
}

void outputFinished(void*, kywc_output_v1*) {
}

const kywc_output_v1_listener outputListener = {
    outputName,
    outputString,
    outputString,
    outputString,
    outputString,
    outputPhysicalSize,
    outputMode,
    outputUint,
    outputInt,
    outputCurrentMode,
    outputPosition,
    outputInt,
    outputScale,
    outputUint,
    outputUint,
    outputUint,
    outputFinished,
};

void managerOutput(void* data, kywc_output_manager_v1*,
                   kywc_output_v1* proxy, const char*) {
    ModeQuery* query = static_cast<ModeQuery*>(data);
    OutputData* output = new OutputData;
    output->proxy = proxy;
    query->outputs.append(output);
    kywc_output_v1_add_listener(proxy, &outputListener, output);
}

void managerPrimary(void*, kywc_output_manager_v1*, kywc_output_v1*) {
}

void managerDone(void*, kywc_output_manager_v1*) {
}

void managerFinished(void*, kywc_output_manager_v1*) {
}

const kywc_output_manager_v1_listener managerListener = {
    managerOutput,
    managerPrimary,
    managerDone,
    managerFinished,
};

void registryGlobal(void* data, wl_registry* registry, uint32_t name,
                    const char* interface, uint32_t version) {
    ModeQuery* query = static_cast<ModeQuery*>(data);
    if (std::strcmp(interface, "gxde_identifier_v1") == 0) {
        query->identified = true;
        return;
    }

    if (std::strcmp(interface, kywc_output_manager_v1_interface.name) != 0)
        return;
    if (!query->collectOutputs)
        return;

    query->manager = static_cast<kywc_output_manager_v1*>(
        wl_registry_bind(registry, name,
                         &kywc_output_manager_v1_interface,
                         qMin(version, uint32_t(1))));
    kywc_output_manager_v1_add_listener(
        query->manager, &managerListener, query);
}

void registryGlobalRemove(void*, wl_registry*, uint32_t) {
}

const wl_registry_listener registryListener = {
    registryGlobal,
    registryGlobalRemove,
};

} // namespace

QDBusArgument& GxdeScreen::operator<<(QDBusArgument& argument,
                                      const Layout& layout) {
    argument.beginStructure();
    argument << layout.name << layout.x << layout.y;
    argument.endStructure();
    return argument;
}

const QDBusArgument& GxdeScreen::operator>>(const QDBusArgument& argument,
                                            Layout& layout) {
    argument.beginStructure();
    argument >> layout.name >> layout.x >> layout.y;
    argument.endStructure();
    return argument;
}

bool GxdeScreen::isAvailable() {
    wl_display* display = wl_display_connect(nullptr);
    if (!display)
        return false;

    ModeQuery query;
    query.collectOutputs = false;
    wl_registry* registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registryListener, &query);
    const bool roundtripSucceeded = wl_display_roundtrip(display) >= 0;
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return roundtripSucceeded && query.identified;
}

QList<GxdeScreen::Mode> GxdeScreen::outputModes(const QString& outputName) {
    QList<Mode> result;
    if (outputName.isEmpty() || !isAvailable())
        return result;

    wl_display* display = wl_display_connect(nullptr);
    if (!display)
        return result;

    ModeQuery query;
    query.outputName = outputName;
    wl_registry* registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registryListener, &query);

    if (wl_display_roundtrip(display) < 0 ||
            wl_display_roundtrip(display) < 0 ||
            !query.identified) {
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
        return result;
    }

    for (OutputData* output : query.outputs) {
        if (output->name != outputName)
            continue;

        for (ModeData* modeData : output->modes) {
            Mode item = modeData->mode;
            if (item.width < 1024 || item.height < 768)
                continue;
            item.current = output->currentMode == modeData->proxy;

            const auto duplicate = std::find_if(
                result.begin(), result.end(),
                [&item](const Mode& existing) {
                    return existing.width == item.width &&
                           existing.height == item.height &&
                           qRound(existing.refresh / 1000.0) ==
                               qRound(item.refresh / 1000.0);
                });
            if (duplicate == result.end()) {
                result.append(item);
            } else {
                duplicate->preferred =
                    duplicate->preferred || item.preferred;
                duplicate->current = duplicate->current || item.current;
            }
        }
        break;
    }

    for (OutputData* output : query.outputs) {
        for (ModeData* mode : output->modes)
            wl_proxy_destroy(
                reinterpret_cast<wl_proxy*>(mode->proxy));
        wl_proxy_destroy(reinterpret_cast<wl_proxy*>(output->proxy));
    }
    if (query.manager)
        wl_proxy_destroy(reinterpret_cast<wl_proxy*>(query.manager));
    wl_registry_destroy(registry);
    wl_display_disconnect(display);

    std::sort(result.begin(), result.end(),
              [](const Mode& left, const Mode& right) {
        if (left.preferred != right.preferred)
            return left.preferred;
        const qint64 leftArea = qint64(left.width) * left.height;
        const qint64 rightArea = qint64(right.width) * right.height;
        if (leftArea != rightArea)
            return leftArea > rightArea;
        if (left.width != right.width)
            return left.width > right.width;
        return left.refresh > right.refresh;
    });
    return result;
}

bool GxdeScreen::setLayout(const QList<Layout>& layout) {
    if (layout.isEmpty())
        return false;

    static const bool registered = [] {
        qDBusRegisterMetaType<Layout>();
        qDBusRegisterMetaType<QList<Layout>>();
        return true;
    }();
    Q_UNUSED(registered);

    return call(QStringLiteral("SetScreenLayout"),
                QVariantList() << QVariant::fromValue(layout));
}