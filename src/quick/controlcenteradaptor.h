/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
 * Copyright (C) 2026 shenmo <jifengshenmo@outlook.com>
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

#ifndef CONTROLCENTERADAPTOR_H
#define CONTROLCENTERADAPTOR_H

#include <QDBusAbstractAdaptor>
#include <QRect>

namespace dcc {
namespace quick {

class ControlCenter;

// The com.deepin.dde.ControlCenter interface. Same as the old
// DBusControlCenterService, so the dock and launcher need no change.
class ControlCenterAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.dde.ControlCenter")

    Q_PROPERTY(bool ShowInRight READ showInRight)
    Q_PROPERTY(QRect Rect READ rect NOTIFY rectChanged)

public:
    explicit ControlCenterAdaptor(ControlCenter *parent);

    bool showInRight() const;
    QRect rect() const;

public Q_SLOTS:
    void Hide();
    void HideImmediately();
    void Show();
    void ShowNav();
    void ShowNotify();
    void ShowImmediately();
    void ShowHome();
    void ShowModule(const QString &module);
    void ShowPage(const QString &module, const QString &page);
    // New in GXDE CC Ultimate: the standalone window.
    void ShowWindow();
    void ShowWindowPage(const QString &module, const QString &page);
    void SetAutoHide(const bool autoHide);
    void Toggle();
    void ToggleInLeft();
    bool isNetworkCanShowPassword();

Q_SIGNALS:
    void rectChanged(const QRect &rect);
    void destRectChanged(const QRect &rect);

private:
    ControlCenter *controlCenter() const;

    bool m_toggleProcessed;
};

} // namespace quick
} // namespace dcc

#endif // CONTROLCENTERADAPTOR_H
