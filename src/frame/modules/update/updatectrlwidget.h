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

#ifndef UPDATECTRLWIDGET_H
#define UPDATECTRLWIDGET_H

#include "widgets/contentwidget.h"
#include "widgets/settingsgroup.h"
#include "summaryitem.h"
#include "downloadprogressbar.h"
#include "resultitem.h"
#include "widgets/labels/tipslabel.h"

#include <types/appupdateinfolist.h>

#include <QSettings>
#include <QSet>

class QCheckBox;
class QPushButton;

using namespace dcc;
using namespace dcc::widgets;

namespace dcc{
namespace widgets {
class PlainTextItem;
}
namespace update{

class UpdateModel;
class DownloadInfo;
class LoadingItem;
class UpdateItem;

class UpdateCtrlWidget : public ContentWidget
{
    Q_OBJECT

public:
    explicit UpdateCtrlWidget(UpdateModel *model, QWidget *parent = 0);
    ~UpdateCtrlWidget();

    void setModel(UpdateModel *model);

Q_SIGNALS:
    void requestDownloadUpdates(const QStringList &packages);
    void requestPauseDownload();
    void requestResumeDownload();
    void requestInstallUpdates();
    void requestRefreshUpdates();

private Q_SLOTS:
    void onProgressBarClicked();
    void loadAppList(const QList<AppUpdateInfo> &infos);

private:
    void setStatus(const UpdatesStatus &status);
    void setDownloadInfo(DownloadInfo *downloadInfo);
    void setProgressValue(const double value);
    void setLowBattery(const bool &lowBattery);
    void setRestartRequired(const bool &restartRequired);
    void setUpdateProgress(const double value);
    void setUpgradeMessage(const QString &message);
    void setFailureMessage(const QString &message);
    QStringList selectedPackages() const;
    void updateSelectedPackages();
    void setAllPackagesSelected(bool selected);

private:
    UpdateModel *m_model;
    UpdatesStatus m_status;

    SettingsGroup* m_checkGroup;
    LoadingItem* m_checkUpdateItem;
    QPushButton *m_refreshButton;
    SettingsGroup *m_resultGroup;
    ResultItem *m_resultItem;
    SettingsGroup *m_failureLogGroup;
    dcc::widgets::PlainTextItem *m_failureLogItem;
    DownloadProgressBar* m_progress;
    SettingsGroup* m_summaryGroup;
    SettingsGroup* m_selectAllGroup;
    SettingsGroup* m_upgradeWarningGroup;
    SettingsItem *m_selectAllItem;
    QCheckBox *m_selectAll;
    SummaryItem* m_summary;
    SummaryItem* m_upgradeWarning;
    TipsLabel *m_powerTip;
    TipsLabel *m_reminderTip;
    TipsLabel *m_noNetworkTip;

    QSettings *m_qsettings;
    QList<UpdateItem *> m_updateItems;
    QSet<QString> m_selectedPackages;
};

}
}
#endif // UPDATECTRLWIDGET_H
