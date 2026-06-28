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

#include "bootwidget.h"

#include <QVBoxLayout>
#include <QScrollBar>
#include <QFile>
#include <iostream>
#include <QFileDialog>
#include <dfiledialog.h>

#include "widgets/settingsgroup.h"
#include "grubbackgrounditem.h"
#include "widgets/translucentframe.h"
#include "systeminfomodel.h"
#include "widgets/basiclistdelegate.h"


using namespace dcc::widgets;

namespace dcc{
namespace systeminfo{

BootWidget::BootWidget(QWidget *parent)
    : ContentWidget(parent)
{
    TranslucentFrame* widget = new TranslucentFrame;

    QVBoxLayout* layout = new QVBoxLayout;
    SettingsGroup *group = new SettingsGroup;

    m_background = new GrubBackgroundItem;

    QVBoxLayout *listLayout = new QVBoxLayout;
    listLayout->setSpacing(0);
    listLayout->setContentsMargins(0, 0, 0, 0);

    m_bootList = new QListWidget;
    m_bootList->setDragDropMode(QListWidget::DragDrop);
    m_bootList->setDefaultDropAction(Qt::MoveAction);
    m_bootList->setPalette(Qt::transparent);
    m_bootList->setWordWrap(true);
    m_bootList->setDragEnabled(false);
    m_bootList->verticalScrollBar()->setContextMenuPolicy(Qt::NoContextMenu);

    m_updatingLabel = new TipsLabel(tr("Updating..."));
    m_updatingLabel->setVisible(false);

    listLayout->addSpacing(10);
    listLayout->addWidget(m_bootList, 0, Qt::AlignHCenter);
    listLayout->addStretch();
    listLayout->addWidget(m_updatingLabel, 0, Qt::AlignHCenter);
    listLayout->addSpacing(10);
    m_background->setLayout(listLayout);

    m_boot = new SwitchWidget();
    m_boot->setTitle(tr("Startup Delay"));

    m_theme = new SwitchWidget();
    m_theme->setTitle(tr("Theme"));

    m_liveCDEnabled = new SwitchWidget();
    m_liveCDEnabled->setTitle(tr("Enable Live Mode"));

    // 检查是否开启 Live CD
    m_liveCDEnabled->setChecked(CheckLiveInstalled());

    TipsLabel *themeLbl = new TipsLabel(tr("Switch theme on to view it in boot menu"));
    themeLbl->setWordWrap(true);
    themeLbl->setContentsMargins(16, 0, 10, 0);

    group->appendItem(m_background);
    group->appendItem(m_boot);
    group->appendItem(m_theme);

    if (QFile::exists("/usr/bin/gxde-live-local")) {
        group->appendItem(m_liveCDEnabled);
    }

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addSpacing(10);
    layout->addWidget(group);
    layout->addSpacing(8);
    //layout->addWidget(label);
    layout->addWidget(themeLbl);
    layout->addStretch();

    widget->setLayout(layout);

    setContent(widget);
    setTitle(tr("Boot Menu"));

    connect(m_liveCDEnabled, &SwitchWidget::checkedChanged, this, &BootWidget::EnabledLiveCD);
    connect(m_theme, SIGNAL(checkedChanged(bool)), this, SIGNAL(enableTheme(bool)));
    connect(m_boot, SIGNAL(checkedChanged(bool)), this, SIGNAL(bootdelay(bool)));
    connect(m_bootList, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),this, SLOT(onCurrentItem(QListWidgetItem*,QListWidgetItem*)));
    connect(m_background, &GrubBackgroundItem::requestEnableTheme, this, &BootWidget::enableTheme);
    //connect(m_background, &GrubBackgroundItem::requestSetBackground, this, &BootWidget::requestSetBackground);  // 禁用设置壁纸
}

bool BootWidget::CheckLiveInstalled() {
    QProcess process;
    process.start("gxde-live-local", QStringList() << "check");
    process.waitForStarted();
    process.waitForFinished();
    int code = process.exitCode();
    process.close();
    return !code;
}

void BootWidget::EnabledLiveCD(bool value)
{
    // 先设置控件为禁用
    m_liveCDEnabled->setDisabled(true);

    QProcess *process = new QProcess();
    if (value) {
        // 浏览文件
        QString path = DFileDialog::getOpenFileName(this,
                                     "选择系统安装镜像",
                                     QDir::homePath(),
                                     "GXDE 系统安装镜像(*.iso);;所有文件(*.*)");
        // 判断是否有选择文件
        if (path == "" || !QFile::exists(path)) {
            delete process;
            m_liveCDEnabled->setChecked(CheckLiveInstalled());
            m_liveCDEnabled->setEnabled(true);
            return;
        }

        process->start("pkexec", QStringList() << "gxde-live-local"
                       << "set" << path);
    }
    else {
        process->start("pkexec", QStringList() << "gxde-live-local" << "del");
    }

    process->waitForStarted();
    connect(process,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [this, process](int exitCode, QProcess::ExitStatus exitStatus){
        qDebug() << "Output:";
        qDebug() << process->readAllStandardOutput();
        qDebug() << "Error";
        qDebug() << process->readAllStandardError();
        // 设置控件为启用
        m_liveCDEnabled->setChecked(CheckLiveInstalled());
        m_liveCDEnabled->setEnabled(true);
        // 显示信息
        if (exitCode) {
            system(("notify-send -i dialog-error '控制中心' 'Live 环境配置失败！\n返回值："
                    + QString::number(exitCode) + "'").toUtf8());
        }
        else {
            system("notify-send -i dialog-ok '控制中心' 'Live 环境配置完成！\n重启后即可在 grub 寻找入口'");
        }
        // 销毁对象
        process->close();
        delete process;
    });
}


void BootWidget::setDefaultEntry(const QString &value)
{
    m_defaultEntry = value;

    blockSignals(true);
    for (int i = 0; i < m_bootList->count(); i++) {
        QListWidgetItem * item = m_bootList->item(i);
        if (item->text() == value) {
            m_bootList->setCurrentRow(i);
        }
    }
    blockSignals(false);
}

#ifndef DCC_DISABLE_GRUB
void BootWidget::setModel(SystemInfoModel *model)
{
    connect(model, &SystemInfoModel::bootDelayChanged, m_boot, &SwitchWidget::setChecked);
    connect(model, &SystemInfoModel::themeEnabledChanged, m_theme, &SwitchWidget::setChecked);
    connect(model, &SystemInfoModel::defaultEntryChanged, this, &BootWidget::setDefaultEntry);
    connect(model, &SystemInfoModel::updatingChanged, m_updatingLabel, &SmallLabel::setVisible);
    connect(model, &SystemInfoModel::entryListsChanged, this, &BootWidget::setEntryList);
    connect(model, &SystemInfoModel::themeEnabledChanged, m_background, &GrubBackgroundItem::setThemeEnable);
    connect(model, &SystemInfoModel::backgroundChanged, m_background, &GrubBackgroundItem::updateBackground);

    m_boot->setChecked(model->bootDelay());
    m_theme->setChecked(model->themeEnabled());
    m_updatingLabel->setVisible(model->updating());
    m_background->setThemeEnable(model->themeEnabled());

    setEntryList(model->entryLists());
    setDefaultEntry(model->defaultEntry());

    m_background->updateBackground(model->background());
}
#endif

void BootWidget::setEntryList(const QStringList &list)
{
    int currentIndex = 0;

    for(int i = 0; i<list.count(); i++)
    {
        const QString entry = list.at(i);

        QListWidgetItem* item = new QListWidgetItem(entry);
        item->setBackground(Qt::transparent);
        item->setSizeHint(QSize(200, 30));
        onCurrentItem(nullptr, item);
        m_bootList->addItem(item);

        if (m_defaultEntry == entry) {
            currentIndex = i;
        }
    }

    m_bootList->setCurrentRow(currentIndex);
}

void BootWidget::onCurrentItem(QListWidgetItem *cur, QListWidgetItem *pre)
{
    if ( pre ) {
        QPixmap pix(16, 16);
        pix.fill(Qt::transparent);
        pre->setIcon(pix);
    }

    if ( cur ) {
        cur->setIcon(loadPixmap(":/systeminfo/themes/common/icons/select.svg"));

        // m_defaultEntry is empty means the data is being initialized.
        if (!m_defaultEntry.isEmpty()) {
            Q_EMIT defaultEntry(cur->text());
        }
    }
}

}
}
