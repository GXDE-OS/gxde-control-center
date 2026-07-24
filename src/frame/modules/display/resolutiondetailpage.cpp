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

#include "resolutiondetailpage.h"
#include "widgets/settingsgroup.h"
#include "monitor.h"
#include "displaymodel.h"
#include "wayland/gxdescreen.h"

#include <limits>

using namespace dcc;
using namespace dcc::widgets;
using namespace dcc::display;

ResolutionDetailPage::ResolutionDetailPage(QWidget *parent)
    : ContentWidget(parent),

      m_resolutions(new SettingsGroup),
      m_currentItem(nullptr)
{
    setTitle(tr("Resolution/Rate"));

    TranslucentFrame *widget = new TranslucentFrame;
    QVBoxLayout *mainLayout = new QVBoxLayout(widget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(m_resolutions);

    setContent(widget);
}

void ResolutionDetailPage::setModel(DisplayModel *model)
{
    m_model = model;

    // delete old items
    for (auto item : m_options.keys())
    {
        m_resolutions->removeItem(item);
        item->deleteLater();
    }
    m_options.clear();
    m_currentItem = nullptr;

    if (!model || model->monitorList().isEmpty())
        return;

    const Monitor *mon = model->monitorList().first();
    const QList<GxdeScreen::Mode> gxdeModes =
        GxdeScreen::outputModes(mon->name());
    const ResolutionList legacyModes = mon->modeList();
    const Resolution currentMode = mon->currentMode();

    QList<ResolutionOption> modes;
    QList<bool> preferredModes;
    QList<bool> currentModes;
    if (!gxdeModes.isEmpty()) {
        for (const GxdeScreen::Mode &mode : gxdeModes) {
            ResolutionOption option;
            option.width = mode.width;
            option.height = mode.height;
            option.refresh = mode.refresh;

            int bestRefreshDelta = std::numeric_limits<int>::max();
            for (const Resolution &legacyMode : legacyModes) {
                if (legacyMode.width() != mode.width ||
                        legacyMode.height() != mode.height) {
                    continue;
                }
                const int refreshDelta =
                    qAbs(qRound(legacyMode.rate() * 1000.0) - mode.refresh);
                if (refreshDelta < bestRefreshDelta) {
                    bestRefreshDelta = refreshDelta;
                    option.mode = legacyMode.id();
                }
            }

            modes.append(option);
            preferredModes.append(mode.preferred);
            currentModes.append(mode.current);
        }
    } else {
        bool first = true;
        for (const Resolution &mode : legacyModes) {
            ResolutionOption option;
            option.mode = mode.id();
            option.width = mode.width();
            option.height = mode.height();
            option.refresh = qRound(mode.rate() * 1000.0);
            modes.append(option);
            preferredModes.append(first);
            currentModes.append(mode == currentMode);
            first = false;
        }
    }

    for (int index = 0; index < modes.size(); ++index)
    {
        const ResolutionOption &mode = modes.at(index);
        const QString res = QString::number(mode.width) + "×" +
            QString::number(mode.height) + "+" +
            QString::number(qRound(mode.refresh / 1000.0)) + "Hz";
        OptionItem *item = new OptionItem;
        item->setContentsMargins(20, 0, 10, 0);

        connect(item, &OptionItem::selectedChanged, this, &ResolutionDetailPage::onItemClicked);

        if (preferredModes.at(index))
            item->setTitle(res + tr(" (Recommended)"));
        else
            item->setTitle(res);

        if (currentModes.at(index))
            m_currentItem = item;

        m_options[item] = mode;
        m_resolutions->appendItem(item);
    }

    connect(mon, &Monitor::currentModeChanged, this, &ResolutionDetailPage::refreshCurrentResolution,
            static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));

    if (!m_currentItem)
        return;

    m_currentItem->blockSignals(true);
    m_currentItem->setSelected(true);
    m_currentItem->blockSignals(false);
}

void ResolutionDetailPage::onItemClicked()
{
    OptionItem *item = qobject_cast<OptionItem *>(sender());
    if (!item || !m_options.contains(item) || !m_model ||
            m_model->monitorList().isEmpty())
        return;

    if (item == m_currentItem)
        return;

    // NOTE: 800*600 is not support resolution;
    if (m_currentItem) {
        m_currentItem->setSelected(false);
    }

    item->setSelected(true);

    m_currentItem = item;

    const ResolutionOption option = m_options.value(item);
    Q_EMIT requestSetResolution(m_model->monitorList().first(), option.mode,
                                option.width, option.height, option.refresh);
    refreshCurrentResolution();
}

void ResolutionDetailPage::refreshCurrentResolution()
{
    if (!m_model || m_model->monitorList().isEmpty() ||
            m_options.isEmpty()) {
        return;
    }

    const Monitor *mon = m_model->monitorList().first();
    if (!mon) {
        return;
    }

    int width = 0;
    int height = 0;
    int refresh = 0;
    for (const GxdeScreen::Output &output : GxdeScreen::outputs()) {
        if (output.name == mon->name()) {
            width = output.width;
            height = output.height;
            refresh = output.refresh;
            break;
        }
    }

    OptionItem *currentItem = nullptr;
    int bestRefreshDelta = std::numeric_limits<int>::max();
    if (width > 0 && height > 0) {
        for (auto it = m_options.cbegin(); it != m_options.cend(); ++it) {
            const ResolutionOption &option = it.value();
            if (option.width != width || option.height != height)
                continue;
            const int refreshDelta = qAbs(option.refresh - refresh);
            if (refreshDelta < bestRefreshDelta) {
                bestRefreshDelta = refreshDelta;
                currentItem = it.key();
            }
        }
    } else {
        const int currentModeId = mon->currentMode().id();
        for (auto it = m_options.cbegin(); it != m_options.cend(); ++it) {
            if (it.value().mode == currentModeId) {
                currentItem = it.key();
                break;
            }
        }
    }

    if (m_currentItem && m_currentItem != currentItem) {
        m_currentItem->blockSignals(true);
        m_currentItem->setSelected(false);
        m_currentItem->blockSignals(false);
    }
    m_currentItem = currentItem;
    if (m_currentItem) {
        m_currentItem->blockSignals(true);
        m_currentItem->setSelected(true);
        m_currentItem->blockSignals(false);
    }
}
