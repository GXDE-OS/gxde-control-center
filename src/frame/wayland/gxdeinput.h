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
 * ----------------------------------------------------------------------------
 * Wayland (gxde-wlcom) input device & shortcut bridge.
 *
 * The control center normally talks to com.deepin.daemon.InputDevices /
 * com.deepin.daemon.Keybinding, which are X11 based and disabled under
 * Wayland. Under Wayland we talk to gxde-wlcom directly through its own
 * D-Bus interfaces (com.kylin.Wlcom.Input / com.kylin.Wlcom.InputAction).
 * ----------------------------------------------------------------------------
 */

#ifndef GXDEINPUT_H
#define GXDEINPUT_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>

namespace GxdeInput {

static const QString Service = QStringLiteral("com.kylin.Wlcom");
static const QString Path = QStringLiteral("/com/kylin/Wlcom/Input");
static const QString Interface = QStringLiteral("com.kylin.Wlcom.Input");

static const QString ShortcutPath = QStringLiteral("/com/kylin/Wlcom/InputAction");
static const QString ShortcutInterface = QStringLiteral("com.kylin.Wlcom.InputAction");

// wlr_input_device_type, keep in sync with gxde-wlcom (wlr/types/wlr_input_device.h)
enum DeviceType {
    DeviceKeyboard = 0,
    DevicePointer = 1,
    DeviceTouch = 2,
    DeviceTabletTool = 3,
    DeviceTabletPad = 4,
    DeviceSwitch = 5,
};

// The prop bitfield mirrors gxde-wlcom's struct input_prop union,
// see include/input/input.h in the gxde-wlcom source tree.
struct Device
{
    QString name; // "type:vendor:product:name"
    quint32 type = 0;
    quint32 prop = 0;

    bool isKeyboard() const { return type == DeviceKeyboard; }
    // A touchpad is a pointer device that supports tapping.
    bool isTouchpad() const { return type == DevicePointer && tapFingerCount() > 0; }
    bool isMouse() const { return type == DevicePointer && tapFingerCount() == 0; }

    quint32 tapFingerCount() const { return prop & 0x7; }
    bool hasNaturalScroll() const { return (prop >> 20) & 0x1; }
    bool hasPointerAccel() const { return (prop >> 22) & 0x1; }
    bool hasLeftHanded() const { return (prop >> 19) & 0x1; }
    bool hasDwt() const { return (prop >> 17) & 0x1; }
    bool hasAccelProfiles() const { return (prop >> 9) & 0x7; }
    bool hasSendEvents() const { return (prop >> 12) & 0x7; }
};

// True when running under a gxde-wlcom Wayland session.
bool isAvailable();

// Enumerate input devices from wlcom.
QList<Device> devices();
QStringList keyboardDevices();
QStringList mouseDevices();
QStringList touchpadDevices();
QStringList tabletDevices();

// --- per device value accessors (operate on the first matching device) ---
bool getNaturalScroll(const QStringList &names, bool *value);
bool getTapToClick(const QStringList &names, bool *value);
bool getLeftHanded(const QStringList &names, bool *value);
bool getDisableWhileTyping(const QStringList &names, bool *value);
bool getPointerSpeed(const QStringList &names, double *value);
bool getAccelProfileAdaptive(const QStringList &names, bool *value);
bool getDoubleClickTime(const QStringList &names, quint32 *value);
bool getTouchpadSendEvents(const QStringList &names, quint32 *mode);

// --- apply a value to all matching devices ---
bool setNaturalScroll(const QStringList &names, bool enabled);
bool setTapToClick(const QStringList &names, bool enabled);
bool setLeftHanded(const QStringList &names, bool enabled);
bool setDisableWhileTyping(const QStringList &names, bool enabled);
bool setPointerSpeed(const QStringList &names, double speed);
bool setAccelProfileAdaptive(const QStringList &names, bool adaptive);
bool setDoubleClickTime(const QStringList &names, quint32 time);
// disableTpad: 1 = disable touchpad, 2 = disable on external mouse, 0 = enabled.
bool setTouchpadSendEvents(const QStringList &names, quint32 mode);

// --- keyboard repeat ---
bool getRepeatInfo(const QStringList &names, int *rate, int *delay);
bool setRepeatInfo(const QStringList &names, int rate, int delay);

// --- shortcuts (com.kylin.Wlcom.InputAction) ---
// Each pair is (bindings, action-config-json); see gxde-wlcom data/config.json.
QList<QPair<QString, QString>> listShortcuts();
// actionData is the comma separated "bus string" accepted by gxde-wlcom's
// AddAction, e.g. "command,/usr/bin/xterm" or "dbus,session,svc,path,iface,method".
bool addShortcut(const QString &bindings, const QString &desc, const QString &actionData,
                 const QString &bindingType);
bool controlShortcut(const QString &controlType, const QString &bindings);

// Grab the next key pressed on the seat; the result is delivered through the
// "KeyEvent(bool pressed, string shortcut)" signal of this interface.
bool grabNextKey();
bool cancelGrab();

} // namespace GxdeInput

#endif // GXDEINPUT_H
