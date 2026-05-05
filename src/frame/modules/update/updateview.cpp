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

#include "updateview.h"
#include "dapplication.h"

#include <QStyle>
#include <QDebug>
#include <ddialog.h>
#include <QFile>
#include <QProcess>
#include <unistd.h>
#include <QDBusMessage>
#include <QDBusConnection>

using namespace dcc::widgets;

namespace dcc{
namespace update{

UpdateView::UpdateView()
    : ModuleWidget()
{
    setObjectName("Update");
    m_updateGroup = new SettingsGroup;
    m_updateItem = new NextPageWidget;
    m_updateItem->setTitle(tr("Update"));
    m_updateGroup->appendItem(m_updateItem);

//    m_settingsItem = new NextPageWidget;
//    m_settingsItem->setTitle(tr("Update Settings"));
//    m_settingsGroup = new SettingsGroup;
//    m_settingsGroup->appendItem(m_settingsItem);


    m_addTestingSource = new QPushButton;
    m_addTestingSource->setText(tr("Internal Testing Group Settings"));


    m_disabledUpgradeNotifications = new SwitchWidget(tr("Disabled Upgrade Notifications"));
    m_updateGroup->appendItem(m_disabledUpgradeNotifications);

    m_centralLayout->addWidget(m_updateGroup);

    if (QFile::exists("/usr/share/gxde-control-center/join-testing-group.sh")) {
        // 需要保证脚本存在才会显示按钮
        m_centralLayout->addWidget(m_addTestingSource);
    }

    connect(m_addTestingSource, &QPushButton::clicked, this, &UpdateView::ShowTesingDialog);
    connect(m_updateItem, &NextPageWidget::clicked,this, &UpdateView::ExecUpgrader);
    connect(m_disabledUpgradeNotifications, &SwitchWidget::checkedChanged, this, &UpdateView::DisabledUpgradeNotifications);


    setTitle(tr("Update"));

    //connect(m_settingsItem, &NextPageWidget::clicked, this, &UpdateView::pushMirrors);
}

void UpdateView::init()
{
    QDBusMessage disabledUpgradeNotificationsStatus = QDBusMessage::createMethodCall("com.gxde.daemon.system.update",
                                                                                     "/com/gxde/daemon/system/update",
                                                                                     "com.gxde.daemon.system.update",
                                                                                     "IsDisabledUpgradeNotifications");
    auto result = QDBusConnection::sessionBus().call(disabledUpgradeNotificationsStatus).arguments();
    if (result.count() >= 1) {
        m_disabledUpgradeNotifications->setChecked(result.at(0).toBool());
    }
    else {
        m_disabledUpgradeNotifications->setChecked(0);
    }
}

void UpdateView::ExecUpgrader()
{
    Q_EMIT pushUpdate();
}

void UpdateView::ShowTesingDialog()
{
    QProcess process;
    process.start("bash", QStringList() << "/usr/share/gxde-control-center/join-testing-group.sh");
    process.waitForStarted();
    process.waitForFinished(-1);
}

void UpdateView::DisabledUpgradeNotifications()
{
    QDBusMessage disabledUpgradeNotificationsStatus = QDBusMessage::createMethodCall("com.gxde.daemon.system.update",
                                                                                     "/com/gxde/daemon/system/update",
                                                                                     "com.gxde.daemon.system.update",
                                                                                     "DisabledUpgradeNotifications");
    disabledUpgradeNotificationsStatus << m_disabledUpgradeNotifications->checked();
    QDBusConnection::sessionBus().call(disabledUpgradeNotificationsStatus);
    init();
}

}
}
