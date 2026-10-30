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

#include "controlcenteradaptor.h"
#include "controlcenter.h"

#include <QTimer>

namespace dcc {
namespace quick {

ControlCenterAdaptor::ControlCenterAdaptor(ControlCenter *parent)
    : QDBusAbstractAdaptor(parent)
    , m_toggleProcessed(true)
{
    connect(parent, &ControlCenter::rectChanged, this, &ControlCenterAdaptor::rectChanged, Qt::QueuedConnection);
    connect(parent, &ControlCenter::destRectChanged, this, &ControlCenterAdaptor::destRectChanged, Qt::QueuedConnection);
}

ControlCenter *ControlCenterAdaptor::controlCenter() const
{
    return static_cast<ControlCenter *>(parent());
}

bool ControlCenterAdaptor::showInRight() const
{
    return true;
}

QRect ControlCenterAdaptor::rect() const
{
    return controlCenter()->rect();
}

void ControlCenterAdaptor::Hide()
{
    controlCenter()->hide();
}

void ControlCenterAdaptor::HideImmediately()
{
    controlCenter()->hideImmediately();
}

void ControlCenterAdaptor::Show()
{
#ifdef DISABLE_MAIN_PAGE
    controlCenter()->showSettingsPage(QString(), QString());
#else
    controlCenter()->show();
#endif
}

void ControlCenterAdaptor::ShowImmediately()
{
    Show();
}

void ControlCenterAdaptor::ShowNotify()
{
    controlCenter()->backToNotify();
    Show();
}

void ControlCenterAdaptor::ShowNav()
{
    controlCenter()->backToNav();
    Show();
}

void ControlCenterAdaptor::ShowHome()
{
    controlCenter()->backToHome();
}

void ControlCenterAdaptor::ShowModule(const QString &module)
{
    ShowPage(module, QString());
}

void ControlCenterAdaptor::ShowPage(const QString &module, const QString &page)
{
    controlCenter()->showSettingsPage(module, page);
}

void ControlCenterAdaptor::ShowWindow()
{
    controlCenter()->showWindowHome();
}

void ControlCenterAdaptor::ShowWindowPage(const QString &module, const QString &page)
{
    controlCenter()->showWindow(module, page);
}

void ControlCenterAdaptor::SetAutoHide(const bool autoHide)
{
    controlCenter()->setDebugAutoHide(autoHide);
}

void ControlCenterAdaptor::Toggle()
{
    // Drop requests that come before the last toggle is done.
    if (!m_toggleProcessed)
        return;

    m_toggleProcessed = false;
    QTimer::singleShot(0, this, [this] {
        controlCenter()->toggle();
        m_toggleProcessed = true;
    });
}

void ControlCenterAdaptor::ToggleInLeft()
{
    Toggle();
}

bool ControlCenterAdaptor::isNetworkCanShowPassword()
{
    return false;
}

} // namespace quick
} // namespace dcc
