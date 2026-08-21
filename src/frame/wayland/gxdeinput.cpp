/*
 * Copyright (C) 2026 GXDE OS Maintainers
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

#include "gxdeinput.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDebug>
#include <QVariantList>

namespace GxdeInput {

namespace {

QDBusMessage call(const QString &path, const QString &interface, const QString &method,
                  const QVariantList &args = QVariantList())
{
    if (!isAvailable())
        return QDBusMessage();

    QDBusMessage message = QDBusMessage::createMethodCall(Service, path, interface, method);
    message.setArguments(args);
    return QDBusConnection::sessionBus().call(message, QDBus::Block);
}

quint32 deviceTypeOf(const QString &name)
{
    const int pos = name.indexOf(':');
    if (pos <= 0)
        return DevicePointer;
    bool ok = false;
    const quint32 type = name.left(pos).toUInt(&ok);
    return ok ? type : DevicePointer;
}

} // namespace

bool isAvailable()
{
    QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
    if (!iface)
        return false;
    return iface->isServiceRegistered(Service);
}

QList<Device> devices()
{
    QList<Device> result;
    const QDBusMessage reply = call(Path, Interface, QStringLiteral("ListAllInputs"));
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return result;

    const QDBusArgument array = qvariant_cast<QDBusArgument>(reply.arguments().first());
    array.beginArray();
    while (!array.atEnd()) {
        QString name;
        quint32 prop = 0;
        array.beginStructure();
        array >> name >> prop;
        array.endStructure();

        Device device;
        device.name = name;
        device.type = deviceTypeOf(name);
        device.prop = prop;
        result.append(device);
    }
    array.endArray();
    return result;
}

QStringList keyboardDevices()
{
    QStringList result;
    const QList<Device> list = devices();
    for (const Device &device : list) {
        if (device.isKeyboard())
            result.append(device.name);
    }
    return result;
}

QStringList mouseDevices()
{
    QStringList result;
    const QList<Device> list = devices();
    for (const Device &device : list) {
        if (device.isMouse())
            result.append(device.name);
    }
    return result;
}

QStringList touchpadDevices()
{
    QStringList result;
    const QList<Device> list = devices();
    for (const Device &device : list) {
        if (device.isTouchpad())
            result.append(device.name);
    }
    return result;
}

QStringList tabletDevices() {
    QStringList result;
    const QList<Device> list = devices();
    for (const Device &device : list) {
        if (device.type == DeviceTabletTool ||
                device.type == DeviceTabletPad) {
            result.append(device.name);
        }
    }
    return result;
}

namespace {

// Get the "current" part of a "(bb)" reply (current, default).
bool getBool(const QString &method, const QStringList &names, bool *value)
{
    for (const QString &name : names) {
        const QDBusMessage reply = call(Path, Interface, method, QVariantList() << name);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 1)
            continue;
        *value = reply.arguments().first().toBool();
        return true;
    }
    return false;
}

bool setBool(const QString &method, const QStringList &names, bool enabled)
{
    bool ok = false;
    for (const QString &name : names) {
        const QDBusMessage reply =
            call(Path, Interface, method, QVariantList() << name << enabled);
        ok = reply.type() == QDBusMessage::ReplyMessage || ok;
    }
    return ok;
}

} // namespace

bool getNaturalScroll(const QStringList &names, bool *value)
{
    return getBool(QStringLiteral("GetNaturalScroll"), names, value);
}

bool getTapToClick(const QStringList &names, bool *value)
{
    return getBool(QStringLiteral("GetTapToClick"), names, value);
}

bool getLeftHanded(const QStringList &names, bool *value)
{
    return getBool(QStringLiteral("GetLeftHand"), names, value);
}

bool getDisableWhileTyping(const QStringList &names, bool *value)
{
    return getBool(QStringLiteral("GetDisableWhileTyping"), names, value);
}

bool setNaturalScroll(const QStringList &names, bool enabled)
{
    return setBool(QStringLiteral("EnableNaturalScroll"), names, enabled);
}

bool setTapToClick(const QStringList &names, bool enabled)
{
    return setBool(QStringLiteral("EnableTapToClick"), names, enabled);
}

bool setLeftHanded(const QStringList &names, bool enabled)
{
    return setBool(QStringLiteral("EnableLeftHand"), names, enabled);
}

bool setDisableWhileTyping(const QStringList &names, bool enabled)
{
    return setBool(QStringLiteral("SetDisableWhileTyping"), names, enabled);
}

bool getPointerSpeed(const QStringList &names, double *value)
{
    for (const QString &name : names) {
        const QDBusMessage reply = call(Path, Interface, QStringLiteral("GetPointerSpeed"),
                                        QVariantList() << name);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 1)
            continue;
        *value = reply.arguments().first().toDouble();
        return true;
    }
    return false;
}

bool setPointerSpeed(const QStringList &names, double speed)
{
    bool ok = false;
    for (const QString &name : names) {
        const QDBusMessage reply =
            call(Path, Interface, QStringLiteral("SetPointerSpeed"),
                 QVariantList() << name << speed);
        ok = reply.type() == QDBusMessage::ReplyMessage || ok;
    }
    return ok;
}

bool getAccelProfileAdaptive(const QStringList &names, bool *value)
{
    for (const QString &name : names) {
        const QDBusMessage reply = call(Path, Interface, QStringLiteral("GetAccelProfile"),
                                        QVariantList() << name);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 1)
            continue;
        *value = reply.arguments().first().toUInt() == 1; // LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE
        return true;
    }
    return false;
}

bool setAccelProfileAdaptive(const QStringList &names, bool adaptive)
{
    // 1 = adaptive, 0 = flat, see libinput accel profile enums.
    const quint32 profile = adaptive ? 1 : 0;
    bool ok = false;
    for (const QString &name : names) {
        const QDBusMessage reply =
            call(Path, Interface, QStringLiteral("SetAccelProfile"),
                 QVariantList() << name << profile);
        ok = reply.type() == QDBusMessage::ReplyMessage || ok;
    }
    return ok;
}

bool getDoubleClickTime(const QStringList &names, quint32 *value)
{
    for (const QString &name : names) {
        const QDBusMessage reply = call(Path, Interface, QStringLiteral("GetDoubleClickTime"),
                                        QVariantList() << name);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 1)
            continue;
        *value = reply.arguments().first().toUInt();
        return true;
    }
    return false;
}

bool setDoubleClickTime(const QStringList &names, quint32 time)
{
    bool ok = false;
    for (const QString &name : names) {
        const QDBusMessage reply =
            call(Path, Interface, QStringLiteral("SetDoubleClickTime"),
                 QVariantList() << name << time);
        ok = reply.type() == QDBusMessage::ReplyMessage || ok;
    }
    return ok;
}

bool setTouchpadSendEvents(const QStringList &names, quint32 mode)
{
    bool ok = false;
    for (const QString &name : names) {
        const QDBusMessage reply =
            call(Path, Interface, QStringLiteral("SetSendEventsMode"),
                 QVariantList() << name << mode);
        ok = reply.type() == QDBusMessage::ReplyMessage || ok;
    }
    return ok;
}

bool getTouchpadSendEvents(const QStringList &names, quint32 *mode)
{
    for (const QString &name : names) {
        const QDBusMessage reply = call(Path, Interface, QStringLiteral("GetSendEventsMode"),
                                        QVariantList() << name);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 1)
            continue;
        *mode = reply.arguments().first().toUInt();
        return true;
    }
    return false;
}

bool getRepeatInfo(const QStringList &names, int *rate, int *delay)
{
    for (const QString &name : names) {
        const QDBusMessage reply = call(Path, Interface, QStringLiteral("GetRepeatInfo"),
                                        QVariantList() << name);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 2)
            continue;
        *rate = reply.arguments().at(0).toInt();
        *delay = reply.arguments().at(1).toInt();
        return true;
    }
    return false;
}

bool setRepeatInfo(const QStringList &names, int rate, int delay)
{
    bool ok = false;
    for (const QString &name : names) {
        const QDBusMessage reply =
            call(Path, Interface, QStringLiteral("SetRepeatInfo"),
                 QVariantList() << name << rate << delay);
        ok = reply.type() == QDBusMessage::ReplyMessage || ok;
    }
    return ok;
}

bool getKeymap(const QStringList &names, Keymap *keymap)
{
    if (!keymap)
        return false;

    for (const QString &name : names) {
        const QDBusMessage reply = call(Path, Interface, QStringLiteral("GetKeymap"),
                                        QVariantList() << name);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 5)
            continue;
        keymap->rules = reply.arguments().at(0).toString();
        keymap->model = reply.arguments().at(1).toString();
        keymap->layout = reply.arguments().at(2).toString();
        keymap->variant = reply.arguments().at(3).toString();
        keymap->options = reply.arguments().at(4).toString();
        return true;
    }
    return false;
}

bool setKeymap(const QStringList &names, const Keymap &keymap)
{
    bool ok = false;
    for (const QString &name : names) {
        const QDBusMessage reply =
            call(Path, Interface, QStringLiteral("SetKeymap"),
                 QVariantList() << name << keymap.rules << keymap.model << keymap.layout
                                << keymap.variant << keymap.options);
        ok = reply.type() == QDBusMessage::ReplyMessage || ok;
    }
    return ok;
}

bool getKeymapGroup(const QStringList &names, quint32 *group)
{
    if (!group)
        return false;

    for (const QString &name : names) {
        const QDBusMessage reply = call(Path, Interface, QStringLiteral("GetKeymapGroup"),
                                        QVariantList() << name);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
            continue;
        *group = reply.arguments().first().toUInt();
        return true;
    }
    return false;
}

bool setKeymapGroup(const QStringList &names, quint32 group)
{
    bool ok = false;
    for (const QString &name : names) {
        const QDBusMessage reply = call(Path, Interface, QStringLiteral("SetKeymapGroup"),
                                        QVariantList() << name << group);
        ok = reply.type() == QDBusMessage::ReplyMessage || ok;
    }
    return ok;
}

QList<QPair<QString, QString>> listShortcuts()
{
    QList<QPair<QString, QString>> result;
    const QDBusMessage reply =
        call(ShortcutPath, ShortcutInterface, QStringLiteral("ListAllActions"));
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return result;

    const QDBusArgument array = qvariant_cast<QDBusArgument>(reply.arguments().first());
    array.beginArray();
    while (!array.atEnd()) {
        QString bindings;
        QString config;
        array.beginStructure();
        array >> bindings >> config;
        array.endStructure();
        result.append(qMakePair(bindings, config));
    }
    array.endArray();
    return result;
}

QList<KeyBinding> listKeyBindings()
{
    QList<KeyBinding> result;
    const QDBusMessage reply =
        call(ShortcutPath, ShortcutInterface, QStringLiteral("ListKeyBindings"));
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return result;

    const QDBusArgument array = qvariant_cast<QDBusArgument>(reply.arguments().first());
    array.beginArray();
    while (!array.atEnd()) {
        KeyBinding binding;
        array.beginStructure();
        array >> binding.bindings >> binding.description >> binding.type;
        array.endStructure();
        result.append(binding);
    }
    array.endArray();
    return result;
}

bool addShortcut(const QString &bindings, const QString &desc, const QString &actionJson,
                 const QString &bindingType)
{
    const QDBusMessage reply =
        call(ShortcutPath, ShortcutInterface, QStringLiteral("AddAction"),
             QVariantList() << QStringLiteral("keyboard") << bindings << desc << actionJson
                            << bindingType);
    return reply.type() == QDBusMessage::ReplyMessage;
}

bool controlShortcut(const QString &controlType, const QString &bindings)
{
    const QDBusMessage reply =
        call(ShortcutPath, ShortcutInterface, QStringLiteral("ControlAction"),
             QVariantList() << controlType << bindings);
    return reply.type() == QDBusMessage::ReplyMessage;
}

bool grabNextKey()
{
    const QDBusMessage reply =
        call(ShortcutPath, ShortcutInterface, QStringLiteral("GrabNextKey"));
    return reply.type() == QDBusMessage::ReplyMessage;
}

bool cancelGrab()
{
    const QDBusMessage reply =
        call(ShortcutPath, ShortcutInterface, QStringLiteral("CancelGrab"));
    return reply.type() == QDBusMessage::ReplyMessage;
}

} // namespace GxdeInput
