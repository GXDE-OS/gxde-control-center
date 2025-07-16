/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
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

#include "personalizationmodel.h"
#include "model/thememodel.h"
#include "model/fontmodel.h"
#include "model/fontsizemodel.h"

#include <QFile>
#include <QDir>
#include <QProcessEnvironment>
#include <QDBusMessage>
#include <QDBusConnection>

#define PERSIONALIZATION_DESTINATION "com.gxde.daemon.personalization"
#define PERSIONALIZATION_PATH "/com/gxde/daemon/personalization"
#define PERSIONALIZATION_INTERFACE "com.gxde.daemon.personalization"

using namespace dcc;
using namespace dcc::personalization;

PersonalizationModel::PersonalizationModel(QObject *parent)
    : QObject(parent)
    , m_opacity(std::pair<int, double>(2, 0.4f))
{
    m_windowModel    = new ThemeModel(this);
    m_iconModel      = new ThemeModel(this);
    m_mouseModel     = new ThemeModel(this);
    m_standFontModel = new FontModel(this);
    m_monoFontModel  = new FontModel(this);
    m_fontSizeModel  = new FontSizeModel(this);
    m_is3DWm = true;
}

PersonalizationModel::~PersonalizationModel()
{

}

void PersonalizationModel::setIs3DWm(const bool is3d)
{
    if (is3d != m_is3DWm) {
        m_is3DWm = is3d;
        Q_EMIT wmChanged(is3d);
    }
}

void PersonalizationModel::setTopPanelGlobalMenu(const bool value)
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(PERSIONALIZATION_DESTINATION,
                                                       PERSIONALIZATION_PATH,
                                                       PERSIONALIZATION_INTERFACE,
                                                       "SetTopPanelGlobalMenu");
    dbus << value;
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
}

void PersonalizationModel::setTopPanel(const bool isTopPanel)
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(PERSIONALIZATION_DESTINATION,
                                                       PERSIONALIZATION_PATH,
                                                       PERSIONALIZATION_INTERFACE,
                                                       "SetTopPanel");
    dbus << isTopPanel;
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
}

void PersonalizationModel::setBottomPanel(const bool value)
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(PERSIONALIZATION_DESTINATION,
                                                       PERSIONALIZATION_PATH,
                                                       PERSIONALIZATION_INTERFACE,
                                                       "SetBottomPanel");
    dbus << value;
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
}

void PersonalizationModel::set20Launcher(const bool value)
{
    QDir dir(QDir::homePath() + "/.config/gxde/gxde-launcher/");
    if (!dir.exists()) {
        dir.mkpath(QDir::homePath() + "/.config/gxde/gxde-launcher/");
    }
    if(value){
       QFile file(QDir::homePath() + "/.config/gxde/gxde-launcher/use_20_launcher");
       file.open(QFile::WriteOnly);
       file.write("1");
       file.close();
    }
    else {
        QFile::remove(QDir::homePath() + "/.config/gxde/gxde-launcher/use_20_launcher");
    }
    system("killall dde-launcher-x11 -9");
    system("killall dde-launcher-wayland -9");
}

void PersonalizationModel::setSizeMode(const bool value)
{
    QDir dir(QDir::homePath() + "/.config/GXDE/dtk/");
    if (!dir.exists()) {
        dir.mkpath(QDir::homePath() + "/.config/GXDE/dtk/");
    }
    if(value){
       QFile file(QDir::homePath() + "/.config/GXDE/dtk/SIZEMODE");
       file.open(QFile::WriteOnly);
       file.write("1");
       file.close();
    }
    else {
        QFile::remove(QDir::homePath() + "/.config/GXDE/dtk/SIZEMODE");
    }
}

void PersonalizationModel::setHideDDEDock(const bool value)
{
    QDir dir(QDir::homePath() + "/.config/GXDE/gxde-dock/");
    if (!dir.exists()) {
        dir.mkpath(QDir::homePath() + "/.config/GXDE/gxde-dock/");
    }
    if(value){
       QFile file(QDir::homePath() + "/.config/GXDE/gxde-dock/dock-hide");
       file.open(QFile::WriteOnly);
       file.write("1");
       file.close();
    }
    else {
        QFile::remove(QDir::homePath() + "/.config/GXDE/gxde-dock/dock-hide");
    }
    system("killall gxde-dock -9");
}



bool PersonalizationModel::is3DWm() const
{
    return m_is3DWm;
}

bool PersonalizationModel::isOpenTopPanelGlobalMenu() const
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(PERSIONALIZATION_DESTINATION,
                                                       PERSIONALIZATION_PATH,
                                                       PERSIONALIZATION_INTERFACE,
                                                       "IsOpenTopPanelGlobalMenu");
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
    return res.arguments().at(0).toBool();
}

bool PersonalizationModel::isOpenTopPanel() const
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(PERSIONALIZATION_DESTINATION,
                                                       PERSIONALIZATION_PATH,
                                                       PERSIONALIZATION_INTERFACE,
                                                       "IsOpenTopPanel");
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
    return res.arguments().at(0).toBool();
}

bool PersonalizationModel::isOpenBottomPanel() const
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(PERSIONALIZATION_DESTINATION,
                                                       PERSIONALIZATION_PATH,
                                                       PERSIONALIZATION_INTERFACE,
                                                       "IsOpenBottomPanel");
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
    qDebug() << res.arguments().at(0).toString();
    return res.arguments()[0].toBool();
}

bool PersonalizationModel::isInstallTopPanel() const
{
    return QFile::exists("/usr/bin/gxde-top-panel");
}

bool PersonalizationModel::isInstallVideoWallpaper() const
{
    return QFile::exists("/usr/bin/fantascene-dynamic-wallpaper");
}

bool PersonalizationModel::isInstallBottomPanel() const
{
    return QFile::exists("/usr/bin/plank");
}

bool PersonalizationModel::isInstall20Launcher() const
{
    return QFile::exists("/usr/bin/dde-launcher-wayland");
}

bool PersonalizationModel::isUse20Launcher() const
{
    return QFile::exists(QDir::homePath() + "/.config/gxde/gxde-launcher/use_20_launcher");
}

bool PersonalizationModel::isSizeMode() const
{
    return QFile::exists(QDir::homePath() + "/.config/GXDE/dtk/SIZEMODE");
}

bool PersonalizationModel::isHideDDEDock() const
{
    return QFile::exists(QDir::homePath() + "/.config/GXDE/gxde-dock/dock-hide");
}

bool PersonalizationModel::isDockUseMacMode() const
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(PERSONALIZATION_DESTINATION,
                                                       PERSONALIZATION_PATH,
                                                       PERSONALIZATION_INTERFACE,
                                                       "IsDockUseMacMode");
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
    qDebug() << res.arguments().at(0).toString();
    return res.arguments()[0].toBool();
}

bool PersonalizationModel::isWayland() const
{
    QString XDG_SESSION_TYPE = QProcessEnvironment::systemEnvironment().value("XDG_SESSION_TYPE");
    QString WAYLAND_DISPLAY = QProcessEnvironment::systemEnvironment().value("WAYLAND_DISPLAY");
    return XDG_SESSION_TYPE == "wayland" || WAYLAND_DISPLAY != "";
}

int PersonalizationModel::windowRadius()
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(PERSONALIZATION_DESTINATION,
                                                       PERSONALIZATION_PATH,
                                                       PERSONALIZATION_INTERFACE,
                                                       "Radius");
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
    if (res.arguments().count() == 0) {
        return 8;
    }
    return res.arguments().at(0).toInt();
}

void PersonalizationModel::setWindowRadius(const int radius)
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(PERSONALIZATION_DESTINATION,
                                                       PERSONALIZATION_PATH,
                                                       PERSONALIZATION_INTERFACE,
                                                       "SetRadius");
    dbus << radius;
    QDBusConnection::sessionBus().call(dbus);
}

void PersonalizationModel::setOpacity(std::pair<int, double> opacity)
{
    if (m_opacity == opacity) return;

    m_opacity = opacity;

    Q_EMIT onOpacityChanged(opacity);
}

void PersonalizationModel::setDockUseMacMode(bool option)
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(PERSONALIZATION_DESTINATION,
                                                       PERSONALIZATION_PATH,
                                                       PERSONALIZATION_INTERFACE,
                                                       "SetDockUseMacMode");
    dbus << option;
    QDBusConnection::sessionBus().call(dbus);
}
