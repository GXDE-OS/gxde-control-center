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

#include "homewidgets.h"

#include "notify/notifywidget.h"
#include "quick_control/quickcontrolpanel.h"
#include "updatenotifier.h"

namespace dcc {
namespace quick {

HomeWidgets::HomeWidgets(QObject *parent)
    : QObject(parent)
    , m_notifyWidget(new NotifyWidget)
    , m_quickControlPanel(new QuickControlPanel)
#ifndef DISABLE_SYS_UPDATE
    , m_updateNotifier(new UpdateNotifier)
#endif
{
    m_quickControlPanel->setHidden(true);

    connect(m_quickControlPanel, &QuickControlPanel::requestDetailConfig, this, &HomeWidgets::allSettingsRequested);
    connect(m_quickControlPanel, &QuickControlPanel::requestPage, this, &HomeWidgets::settingsPageRequested);
    connect(m_quickControlPanel, &QuickControlPanel::screenCaptureStarted, this, &HomeWidgets::screenCaptureStarted);

    if (m_updateNotifier) {
        m_updateNotifier->setObjectName("UpdateNotifier");
        connect(m_updateNotifier, &UpdateNotifier::clicked, this, [this] {
            Q_EMIT settingsPageRequested("update", "available-updates", false);
        });
    }
}

HomeWidgets::~HomeWidgets()
{
    delete m_notifyWidget.data();
    delete m_quickControlPanel.data();
    delete m_updateNotifier.data();
}

QObject *HomeWidgets::notifyWidget() const
{
    return m_notifyWidget.data();
}

QObject *HomeWidgets::quickControlPanel() const
{
    return m_quickControlPanel.data();
}

QObject *HomeWidgets::updateNotifier() const
{
    return m_updateNotifier.data();
}

void HomeWidgets::setQuickControlPanelVisible(bool visible)
{
    if (m_quickControlPanel)
        m_quickControlPanel->setVisible(visible);
}

void HomeWidgets::appear()
{
    if (m_quickControlPanel)
        m_quickControlPanel->appear();
}

void HomeWidgets::disappear()
{
    if (m_quickControlPanel)
        m_quickControlPanel->disappear();
}

} // namespace quick
} // namespace dcc
