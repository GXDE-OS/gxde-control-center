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

#include "updatectrlwidget.h"
#include "updateitem.h"
#include "widgets/translucentframe.h"
#include "widgets/plantextitem.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QFontDatabase>

#include "updatemodel.h"
#include "loadingitem.h"
#include "widgets/labels/normallabel.h"

#define UpgradeWarningSize 500

namespace dcc{
namespace update{

UpdateCtrlWidget::UpdateCtrlWidget(UpdateModel *model, QWidget *parent)
    : ContentWidget(parent),
      m_model(nullptr),
      m_status(UpdatesStatus::Updated),
      m_checkGroup(new SettingsGroup),
      m_checkUpdateItem(new LoadingItem),
      m_refreshButton(new QPushButton(tr("Refresh"))),
      m_resultGroup(new SettingsGroup),
      m_resultItem(new ResultItem),
      m_failureLogGroup(new SettingsGroup),
      m_failureLogItem(new PlainTextItem),
      m_progress(new DownloadProgressBar),
      m_summaryGroup(new SettingsGroup),
      m_selectAllGroup(new SettingsGroup),
      m_upgradeWarningGroup(new SettingsGroup),
      m_selectAllItem(new SettingsItem),
      m_selectAll(new QCheckBox(tr("Select all"))),
      m_summary(new SummaryItem),
      m_upgradeWarning(new SummaryItem),
      m_powerTip(new TipsLabel),
      m_reminderTip(new TipsLabel(tr("Please restart to use the system and applications properly after updated"))),
      m_noNetworkTip(new TipsLabel(tr("Network disconnected, please retry after connected"))),
      m_qsettings(new QSettings(this))
{
    setTitle(tr("Update"));

    TranslucentFrame* widget = new TranslucentFrame();
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setMargin(0);
    layout->setSpacing(10);

    m_checkGroup->setVisible(false);
    m_checkGroup->appendItem(m_checkUpdateItem);

    QHBoxLayout *refreshLayout = new QHBoxLayout;
    refreshLayout->setMargin(0);
    refreshLayout->addStretch();
    refreshLayout->addWidget(m_refreshButton);

    m_resultGroup->setVisible(false);
    m_resultGroup->appendItem(m_resultItem);

    m_failureLogGroup->setVisible(false);
    m_failureLogItem->plainEdit()->setReadOnly(true);
    m_failureLogItem->plainEdit()->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_failureLogItem->plainEdit()->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_failureLogItem->plainEdit()->setMinimumHeight(180);
    m_failureLogItem->plainEdit()->setMaximumHeight(320);
    m_failureLogGroup->appendItem(m_failureLogItem);

    m_progress->setVisible(false);

    m_summaryGroup->setVisible(false);
    m_summaryGroup->appendItem(m_summary);

    m_selectAllGroup->setVisible(false);
    m_selectAll->setChecked(true);
    QHBoxLayout *selectAllLayout = new QHBoxLayout;
    selectAllLayout->setMargin(10);
    selectAllLayout->addWidget(m_selectAll);
    m_selectAllItem->setLayout(selectAllLayout);
    m_selectAllGroup->appendItem(m_selectAllItem);

    m_powerTip->setWordWrap(true);
    m_powerTip->setAlignment(Qt::AlignHCenter);
    m_powerTip->setVisible(false);

    m_reminderTip->setWordWrap(true);
    m_reminderTip->setAlignment(Qt::AlignHCenter);
    m_reminderTip->setVisible(false);

    m_noNetworkTip->setWordWrap(true);
    m_noNetworkTip->setAlignment(Qt::AlignHCenter);
    m_noNetworkTip->setVisible(false);

    m_upgradeWarning->setTitle(tr("This update may take a long time, please do not shut down or reboot during the process"));
    m_upgradeWarning->setContentsMargins(20, 0, 20, 0);
    m_upgradeWarningGroup->setVisible(false);
    m_upgradeWarningGroup->appendItem(m_upgradeWarning);

    layout->addSpacing(10);
    layout->addWidget(m_checkGroup);
    layout->addWidget(m_resultGroup);
    layout->addWidget(m_failureLogGroup);
    layout->addWidget(m_progress);
    layout->addLayout(refreshLayout);
    layout->addWidget(m_upgradeWarningGroup);
    layout->addWidget(m_selectAllGroup);
    layout->addWidget(m_summaryGroup);
    layout->addWidget(m_powerTip);
    layout->addWidget(m_reminderTip);
    layout->addWidget(m_noNetworkTip);
    layout->addStretch();

    widget->setLayout(layout);
    setContent(widget);

    setModel(model);

    connect(m_progress, &DownloadProgressBar::clicked, this, &UpdateCtrlWidget::onProgressBarClicked);
    connect(m_selectAll, &QCheckBox::toggled, this, &UpdateCtrlWidget::setAllPackagesSelected);
    connect(m_refreshButton, &QPushButton::clicked, this, &UpdateCtrlWidget::requestRefreshUpdates);
}

UpdateCtrlWidget::~UpdateCtrlWidget()
{

}

void UpdateCtrlWidget::loadAppList(const QList<AppUpdateInfo>& infos)
{
    qDebug() << infos.count();
    m_updateItems.clear();
    m_selectedPackages.clear();
    QLayoutItem *item;
    while((item = m_summaryGroup->layout()->takeAt(1)) != NULL) {
        item->widget()->deleteLater();
        delete item;
    }

    for(const AppUpdateInfo& info : infos)
    {
        UpdateItem* item = new UpdateItem();
        item->setAppInfo(info);
        connect(item, &UpdateItem::selectionChanged, this, &UpdateCtrlWidget::updateSelectedPackages);

        m_summaryGroup->appendItem(item);
        m_updateItems << item;
    }

    updateSelectedPackages();
}

void UpdateCtrlWidget::onProgressBarClicked()
{
    switch (m_status) {
    case UpdatesStatus::UpdatesAvailable:
        Q_EMIT requestDownloadUpdates(selectedPackages());
        break;
    case UpdatesStatus::Downloading:
        Q_EMIT requestPauseDownload();
        break;
    case UpdatesStatus::DownloadPaused:
        Q_EMIT requestResumeDownload();
        break;
    case UpdatesStatus::Downloaded:
        Q_EMIT requestInstallUpdates();
        break;
    default:
        qWarning() << "unhandled status " << m_status;
        break;
    }
}

void UpdateCtrlWidget::setStatus(const UpdatesStatus &status)
{
    m_status = status;

    m_noNetworkTip->setVisible(false);
    m_resultGroup->setVisible(false);
    m_failureLogGroup->setVisible(false);
    m_progress->setVisible(false);
    m_selectAllGroup->setVisible(false);
    m_summaryGroup->setVisible(false);
    m_upgradeWarningGroup->setVisible(false);
    m_reminderTip->setVisible(false);
    m_checkGroup->setVisible(false);
    m_checkUpdateItem->setVisible(false);
    m_checkUpdateItem->setProgressBarVisible(false);
    m_refreshButton->setDisabled(status == UpdatesStatus::Checking || status == UpdatesStatus::Downloading || status == UpdatesStatus::Installing);

    switch (status) {
    case UpdatesStatus::Checking:
        m_checkGroup->setVisible(true);
        m_checkUpdateItem->setVisible(true);
        m_checkUpdateItem->setProgressBarVisible(true);
        m_checkUpdateItem->setMessage(tr("Checking for updates, please wait..."));
        break;
    case UpdatesStatus::UpdatesAvailable:
        m_progress->setVisible(true);
        m_selectAllGroup->setVisible(true);
        m_summaryGroup->setVisible(true);
        m_progress->setMessage(tr("Download and install selected updates"));
        setDownloadInfo(m_model->downloadInfo());
        m_progress->setValue(0);
        setLowBattery(m_model->lowBattery());
        updateSelectedPackages();
        break;
    case UpdatesStatus::Downloading:
        m_progress->setVisible(true);
        m_selectAllGroup->setVisible(true);
        m_summaryGroup->setVisible(true);
        m_progress->setValue(m_progress->minimum());
        m_progress->setMessage(tr("%1% downloaded (Click to pause)").arg(m_progress->value()));
        break;
    case UpdatesStatus::DownloadPaused:
        m_progress->setVisible(true);
        m_selectAllGroup->setVisible(true);
        m_summaryGroup->setVisible(true);
        m_progress->setMessage(tr("%1% downloaded (Click to continue)").arg(m_progress->value()));
        break;
    case UpdatesStatus::Downloaded:
        m_progress->setVisible(true);
        m_selectAllGroup->setVisible(true);
        m_summaryGroup->setVisible(true);
        m_progress->setValue(m_progress->maximum());
        m_progress->setMessage(tr("Install updates"));
        setDownloadInfo(m_model->downloadInfo());
        setLowBattery(m_model->lowBattery());
        break;
    case UpdatesStatus::Updated:
        m_checkGroup->setVisible(true);
        m_checkUpdateItem->setVisible(true);
        m_checkUpdateItem->setMessage(tr("Your system is up to date"));
        break;
    case UpdatesStatus::Installing:
        m_progress->setVisible(true);
        m_summaryGroup->setVisible(true);
        m_progress->setMessage(m_model->upgradeMessage().isEmpty() ? tr("Updating, please wait...") : m_model->upgradeMessage());
        break;
    case UpdatesStatus::UpdateSucceeded:
        m_resultItem->setSuccess(true);
        m_resultGroup->setVisible(true);
        m_reminderTip->setVisible(m_model->restartRequired());
        break;
    case UpdatesStatus::UpdateFailed:
        m_resultGroup->setVisible(true);
        m_resultItem->setSuccess(false);
        setFailureMessage(m_model->failureMessage());
        break;
    case UpdatesStatus::NeedRestart:
        m_checkGroup->setVisible(true);
        m_checkUpdateItem->setVisible(true);
        m_checkUpdateItem->setMessage(tr("The newest system installed, restart to take effect"));
        break;
    case UpdatesStatus::NoNetwork:
        m_resultGroup->setVisible(true);
        m_resultItem->setSuccess(false);
        m_noNetworkTip->setVisible(true);
        break;
    case UpdatesStatus::NoSpace:
        m_resultGroup->setVisible(true);
        m_resultItem->setSuccess(false);
        m_resultItem->setMessage(tr("Insufficient disk space, unable to update system."));
        break;
    case UpdatesStatus::DeependenciesBrokenError:
        m_resultGroup->setVisible(true);
        m_resultItem->setSuccess(false);
        m_resultItem->setMessage(tr("Dependency error, failed to detect the updates"));
        break;
    default:
        qWarning() << "unknown status!!!";
    }
}

void UpdateCtrlWidget::setDownloadInfo(DownloadInfo *downloadInfo)
{
    if (!downloadInfo)
        return;

    const QList<AppUpdateInfo> &apps = downloadInfo->appInfos();
    const qlonglong downloadSize = downloadInfo->downloadSize();

    int appCount = apps.length();
    for (const AppUpdateInfo &info : apps) {
        if (info.m_packageId == "dde") {
            appCount--;
        }
    }

    m_summary->setTitle(tr("%n application update(s) detected", "", appCount));

    for (const AppUpdateInfo &info : apps) {
        if (info.m_packageId == "dde") {
            if (!appCount) {
                m_summary->setTitle(tr("New system edition detected"));
            } else {
                m_summary->setTitle(tr("New system edition and %n application update(s) detected", "", appCount));
            }
            break;
        }
    }

    m_summary->setDetails(downloadSize > 0 ? QString(tr("Download size: %1").arg(formatCap(downloadSize))) : QString());

    if (downloadSize) {
        if ((downloadSize / 1024) / 1024 >= m_qsettings->value("upgrade_waring_size", UpgradeWarningSize).toInt())
            m_upgradeWarningGroup->setVisible(true);
    }

    loadAppList(apps);
}

void UpdateCtrlWidget::setProgressValue(const double value)
{
    m_progress->setValue(value * 100);

    if (m_status == UpdatesStatus::Downloading) {
        m_progress->setMessage(tr("%1% downloaded (Click to pause)").arg(qFloor(value * 100)));
    }
}

void UpdateCtrlWidget::setUpgradeMessage(const QString &message)
{
    if (m_status == UpdatesStatus::Installing && !message.isEmpty()) {
        m_progress->setMessage(message);
    }
}

void UpdateCtrlWidget::setFailureMessage(const QString &message)
{
    m_failureLogItem->plainEdit()->setPlainText(message);
    m_failureLogGroup->setVisible(m_status == UpdatesStatus::UpdateFailed && !message.isEmpty());
}

QStringList UpdateCtrlWidget::selectedPackages() const
{
    return m_selectedPackages.values();
}

void UpdateCtrlWidget::updateSelectedPackages()
{
    m_selectedPackages.clear();

    for (UpdateItem *item : m_updateItems) {
        if (item && item->isSelected()) {
            m_selectedPackages.insert(item->packageId());
        }
    }

    const bool allSelected = !m_updateItems.isEmpty() && m_selectedPackages.count() == m_updateItems.count();
    const bool partiallySelected = !m_selectedPackages.isEmpty() && !allSelected;
    m_selectAll->blockSignals(true);
    m_selectAll->setTristate(partiallySelected);
    m_selectAll->setCheckState(allSelected ? Qt::Checked : (partiallySelected ? Qt::PartiallyChecked : Qt::Unchecked));
    m_selectAll->setTristate(false);
    m_selectAll->blockSignals(false);

    m_progress->setDisabled(m_selectedPackages.isEmpty() || m_model->lowBattery());
}

void UpdateCtrlWidget::setAllPackagesSelected(bool selected)
{
    for (UpdateItem *item : m_updateItems) {
        if (item) {
            item->setSelected(selected);
        }
    }

    updateSelectedPackages();
}

void UpdateCtrlWidget::setLowBattery(const bool &lowBattery)
{
    if (m_status == UpdatesStatus::Downloaded || m_status == UpdatesStatus::UpdatesAvailable) {
        if(lowBattery) {
            m_powerTip->setText(tr("Your battery is lower than 50%, please plug in to continue"));
        } else {
            m_powerTip->setText(tr("Please ensure sufficient power to restart, and don't power off or unplug your machine"));
        }

        m_progress->setDisabled(lowBattery || m_selectedPackages.isEmpty());
        m_powerTip->setVisible(lowBattery);
    }
}

void UpdateCtrlWidget::setRestartRequired(const bool &restartRequired)
{
    if (m_status == UpdatesStatus::UpdateSucceeded) {
        m_reminderTip->setVisible(restartRequired);
    }
}

void UpdateCtrlWidget::setUpdateProgress(const double value)
{
    m_checkUpdateItem->setProgressValue(value * 100);
}

void UpdateCtrlWidget::setModel(UpdateModel *model)
{
    m_model = model;

    connect(m_model, &UpdateModel::statusChanged, this, &UpdateCtrlWidget::setStatus);
    connect(m_model, &UpdateModel::lowBatteryChanged, this, &UpdateCtrlWidget::setLowBattery);
    connect(m_model, &UpdateModel::restartRequiredChanged, this, &UpdateCtrlWidget::setRestartRequired);
    connect(m_model, &UpdateModel::downloadInfoChanged, this, &UpdateCtrlWidget::setDownloadInfo);
    connect(m_model, &UpdateModel::upgradeProgressChanged, this, &UpdateCtrlWidget::setProgressValue);
    connect(m_model, &UpdateModel::upgradeMessageChanged, this, &UpdateCtrlWidget::setUpgradeMessage);
    connect(m_model, &UpdateModel::failureMessageChanged, this, &UpdateCtrlWidget::setFailureMessage);
    connect(m_model, &UpdateModel::updateProgressChanged, this, &UpdateCtrlWidget::setUpdateProgress);

    setUpdateProgress(m_model->updateProgress());
    setProgressValue(m_model->upgradeProgress());
    setUpgradeMessage(m_model->upgradeMessage());
    setFailureMessage(m_model->failureMessage());
    setStatus(m_model->status());
    setLowBattery(m_model->lowBattery());
    setRestartRequired(m_model->restartRequired());
    setDownloadInfo(m_model->downloadInfo());
}

}
}
