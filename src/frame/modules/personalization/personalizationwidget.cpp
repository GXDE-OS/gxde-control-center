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

#include "personalizationwidget.h"
#include "widgets/contentwidget.h"
#include "widgets/dccslider.h"
#include "widgets/nextpagewidget.h"
#include "personalizationmodel.h"
#include "widgets/settingsgroup.h"
#include "widgets/switchwidget.h"
#include "widgets/titledslideritem.h"
#include "dwindowmanagerhelper.h"
#include "dapplication.h"
#include "wayland/gxdescreen.h"

#include <QDebug>
#include <QPushButton>
#include <QFile>

DTK_USE_NAMESPACE

using namespace dcc;
using namespace dcc::personalization;
using namespace dcc::widgets;

PersonalizationWidget::PersonalizationWidget()
    : ModuleWidget()
    , m_userGroup(new SettingsGroup)
    , m_transparentSlider(new TitledSliderItem(tr("Transparency")))
    , m_radiusSlider(new TitledSliderItem(tr("Radius (Logout to apply)")))
    , m_trGrp(new SettingsGroup)
{
    setObjectName("Personalization");

    m_trGrp->appendItem(m_transparentSlider);
    m_trGrp->appendItem(m_radiusSlider);

    m_transparentSlider->setObjectName("Transparency");
    m_radiusSlider->setObjectName("Radius");

    DCCSlider *slider = m_transparentSlider->slider();
    slider->setRange(1, 6);
    slider->setType(DCCSlider::Vernier);
    slider->setTickPosition(QSlider::TicksBelow);
    slider->setTickInterval(1);
    slider->setPageStep(1);

    DCCSlider *radiusSlider = m_radiusSlider->slider();
    radiusSlider->setRange(0, 18);
    radiusSlider->setType(DCCSlider::Vernier);
    radiusSlider->setTickPosition(QSlider::TicksBelow);
    radiusSlider->setTickInterval(1);
    radiusSlider->setPageStep(1);
    /*QStringList numberList;
    for (int i = 0; i <= 18; ++i) {
        numberList << QString::number(i);
    }
    m_radiusSlider->setAnnotations(numberList);*/

    m_centralLayout->addWidget(m_trGrp);

    m_centralLayout->addWidget(m_userGroup);
    NextPageWidget *theme = new NextPageWidget;
    NextPageWidget *font  = new NextPageWidget;
    NextPageWidget *videoWallpaper  = new NextPageWidget;

    m_wmSwitch = new SwitchWidget(tr("Window Effect"));

    m_showTopPanel = new SwitchWidget(tr("Top Panel"));

    m_showTopPanelGlobalMenu = new SwitchWidget(tr("Top Panel Global Menu"));

    m_showBottomPanel = new SwitchWidget(tr("Bottom Panel"));

    m_use20Launcher = new SwitchWidget(tr("Use deepin 20 style launcher"));

    m_hideDDEDock = new SwitchWidget(tr("Hide DDE Dock"));
    m_dockUseMacMode = new SwitchWidget(tr("Dock's Mac Mode"));

    m_sizeMode = new SwitchWidget(tr("Enable Compact mode on DTK5 Apps (Relogin to take effect)"));

    theme->setTitle(tr("Theme"));
    font->setTitle(tr("Font"));
    videoWallpaper->setTitle(tr("Video Wallpaper"));

    m_userGroup->appendItem(theme);
    m_userGroup->appendItem(font);
    // 判断系统有没有安装动态壁纸
    if(m_model->isInstallVideoWallpaper()) {
        m_userGroup->appendItem(videoWallpaper);
    }
    // 仅 Wayland 会话且 WM 广播 gxde-identifier-v1 时显示
    NextPageWidget *gxwm = new NextPageWidget;
    gxwm->setTitle(tr("窗口管理器"));
    gxwm->setHidden(!(DApplication::isWayland() && GxdeScreen::isAvailable()));
    m_userGroup->appendItem(gxwm);
    m_userGroup->appendItem(m_wmSwitch);
    // 判断指定程序是否存在，如果不存在则不显示
    if(m_model->isInstallTopPanel()) {
        m_userGroup->appendItem(m_showTopPanel);
        m_userGroup->appendItem(m_showTopPanelGlobalMenu);
    }
    if(m_model->isInstallBottomPanel()) {
        m_userGroup->appendItem(m_showBottomPanel);
    }
    if(m_model->isInstall20Launcher()) {
        m_userGroup->appendItem(m_use20Launcher);
    }
    // 如果是 Wayland，则禁用以下内容
    if(DApplication::isWayland()) {
        m_showBottomPanel->setHidden(true);
        m_wmSwitch->setHidden(true);
        m_use20Launcher->setHidden(true);
    }
    m_userGroup->appendItem(m_hideDDEDock);
    m_userGroup->appendItem(m_dockUseMacMode);
    m_userGroup->appendItem(m_sizeMode);

    setTitle(tr("Personalization"));
    connect(theme, &NextPageWidget::clicked, this,
            &PersonalizationWidget::showThemeWidget);
    connect(font, &NextPageWidget::clicked, this,
            &PersonalizationWidget::showFontsWidget);
    connect(videoWallpaper, &NextPageWidget::clicked, this,
            &PersonalizationWidget::showVideoWallpaperWidget);
    connect(gxwm, &NextPageWidget::clicked, this,
            &PersonalizationWidget::showGxwmWidget);
    connect(m_wmSwitch, &SwitchWidget::checkedChanged, this,
            &PersonalizationWidget::requestSwitchWM);
    connect(m_wmSwitch, &SwitchWidget::checkedChanged, this, [=] {
        // reset switch state
        m_wmSwitch->setChecked(m_model->is3DWm());
    });
    //
    connect(m_showTopPanel, &SwitchWidget::checkedChanged, this,
            &PersonalizationWidget::requestSetTopPanel);
    connect(m_showTopPanel, &SwitchWidget::checkedChanged, this, [=] {
        // reset top panel state
        m_showTopPanel->setChecked(m_model->isOpenTopPanel());
        m_showTopPanelGlobalMenu->setHidden(!m_model->isOpenTopPanel());
        if (!m_model->isOpenTopPanel()) {
            m_showTopPanelGlobalMenu->setChecked(false);
        }
    });

    connect(m_showTopPanelGlobalMenu, &SwitchWidget::checkedChanged, this,
            &PersonalizationWidget::requestSetTopPanelGlobalMenu);
    connect(m_showTopPanelGlobalMenu, &SwitchWidget::checkedChanged, this, [=] {
        // reset top panel state
        m_showTopPanelGlobalMenu->setChecked(m_model->isOpenTopPanelGlobalMenu());
    });

    connect(m_sizeMode, &SwitchWidget::checkedChanged, this,
            &PersonalizationWidget::requestSetSizeMode);
    connect(m_sizeMode, &SwitchWidget::checkedChanged, this, [=] {
        // reset top panel state
        m_sizeMode->setChecked(m_model->isSizeMode());
    });

    connect(m_showBottomPanel, &SwitchWidget::checkedChanged, this,
            &PersonalizationWidget::requestSetBottomPanel);
    connect(m_showBottomPanel, &SwitchWidget::checkedChanged, this, [=] {
        // reset top panel state
        m_showBottomPanel->setChecked(m_model->isOpenBottomPanel());
    });

    connect(m_use20Launcher, &SwitchWidget::checkedChanged, this,
            &PersonalizationWidget::requestSet20Launcher);
    connect(m_use20Launcher, &SwitchWidget::checkedChanged, this, [=] {
       // reset state
        m_use20Launcher->setChecked(m_model->isUse20Launcher());
    });

    connect(m_hideDDEDock, &SwitchWidget::checkedChanged, this,
            &PersonalizationWidget::requestSetHideDDEDock);
    connect(m_hideDDEDock, &SwitchWidget::checkedChanged, this, [=] {
       // reset state
        m_hideDDEDock->setChecked(m_model->isHideDDEDock());
    });

    connect(m_dockUseMacMode, &SwitchWidget::checkedChanged, this,
            &PersonalizationWidget::requestSetDockUseMacMode);
    connect(m_dockUseMacMode, &SwitchWidget::checkedChanged, this,
            [this](){
        if (m_dockUseMacMode->checked()) {
            m_showTopPanel->setChecked(true);
            m_showTopPanelGlobalMenu->setChecked(true);
            // 手动发送信号避免勾选后未执行相关操作
            // 注：只发送全局菜单的事件避免重复执行开关顶栏的函数开启多个顶栏
            Q_EMIT requestSetTopPanel(true);
        }
    });
    connect(m_dockUseMacMode, &SwitchWidget::checkedChanged, this, [=] {
       // reset state
        m_dockUseMacMode->setChecked(m_model->isDockUseMacMode());
    });

    connect(m_transparentSlider->slider(), &DCCSlider::valueChanged, this,
            &PersonalizationWidget::requestSetOpacity);

    connect(m_radiusSlider->slider(), &DCCSlider::valueChanged, this,
            &PersonalizationWidget::requestSetRadius);

    connect(DWindowManagerHelper::instance(), &DWindowManagerHelper::hasBlurWindowChanged, this,
            &PersonalizationWidget::onBlurWindowChanged);

    onBlurWindowChanged();
    initRadiusSlider();
}

void PersonalizationWidget::setModel(PersonalizationModel *const model)
{
    m_model = model;

    connect(model, &PersonalizationModel::wmChanged, m_wmSwitch,
            &SwitchWidget::setChecked);

    m_wmSwitch->setChecked(model->is3DWm());
    m_showTopPanel->setChecked(model->isOpenTopPanel());
    m_showTopPanelGlobalMenu->setChecked(model->isOpenTopPanelGlobalMenu());
    m_showBottomPanel->setChecked(model->isOpenBottomPanel());
    m_use20Launcher->setChecked(model->isUse20Launcher());
    m_hideDDEDock->setChecked(model->isHideDDEDock());
    m_dockUseMacMode->setChecked(model->isDockUseMacMode());
    m_sizeMode->setChecked(model->isSizeMode());

    connect(model, &PersonalizationModel::onOpacityChanged, this,
            &PersonalizationWidget::onOpacityChanged);

    onOpacityChanged(model->opacity());
}

void PersonalizationWidget::onOpacityChanged(std::pair<int, double> value)
{
    m_transparentSlider->slider()->blockSignals(true);
    m_transparentSlider->slider()->setValue(value.first);
    m_transparentSlider->slider()->blockSignals(false);
}

void PersonalizationWidget::onBlurWindowChanged()
{
    if (DApplication::isWayland()) {
        m_trGrp->setVisible(true);
    } else {
        m_trGrp->setVisible(DWindowManagerHelper::instance()->hasBlurWindow());
    }
}

void PersonalizationWidget::initRadiusSlider()
{
    m_radiusSlider->slider()->setValue(PersonalizationModel::windowRadius());
    m_radiusSlider->setValueLiteral(QString::number(PersonalizationModel::windowRadius()));
}

void PersonalizationWidget::requestSetRadius(int value)
{
    PersonalizationModel::setWindowRadius(value);
    initRadiusSlider();
}
