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

#include "quickcontrolpanel.h"
#include "basicsettingspage.h"
#include "quickswitchbutton.h"
#include "vpn/vpncontrolpage.h"
#include "modules/display/displaycontrolpage.h"
#include "wifi/wifipage.h"

#include "modules/display/displaymodel.h"
#include "modules/display/displayworker.h"

#ifndef DISABLE_BLUETOOTH
#include "quick_control/bluetooth/bluetoothlist.h"
#include "modules/bluetooth/bluetoothmodel.h"
#include "modules/bluetooth/bluetoothworker.h"
#include "modules/bluetooth/adapter.h"
#endif

#include <QVBoxLayout>
#include <QProcess>
#include <QProcessEnvironment>
#include <networkmodel.h>
#include <networkworker.h>

using namespace dcc;
using dde::network::NetworkModel;
using dde::network::NetworkWorker;
using dde::network::NetworkDevice;
using dcc::display::DisplayModel;
using dcc::display::DisplayWorker;

#ifndef DISABLE_BLUETOOTH
using dcc::bluetooth::BluetoothModel;
using dcc::bluetooth::BluetoothWorker;
#endif

// 修复在x11下从信息页快捷中心打开的软件无特效的问题
void QuickControlPanel::startAppWithEffect(const QString &program, const QStringList &arguments)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    
    env.insert("QT_QPA_PLATFORM", "dxcb;xcb");

    QProcess::startDetached(program, arguments, QString(), env);
}

QuickControlPanel::QuickControlPanel(QWidget *parent)
    : QWidget(parent),

      m_itemStack(new QStackedLayout)
{
    // 判断是否在 Chroot 下运行
    QDBusMessage checkChrootDBus = QDBusMessage::createMethodCall(CHROOTCHECKDESTINATION,
                                                                  CHROOTCHECKPATH,
                                                                  CHROOTCHECKINTERFACE,
                                                                  "IsInChroot");
    QDBusMessage res = QDBusConnection::sessionBus().call(checkChrootDBus);
    m_isInChroot = res.arguments().at(0).toBool();

    QHBoxLayout *btnsLayout = new QHBoxLayout;
    QGridLayout *controlBtnLayout = new QGridLayout;
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(m_itemStack);
    mainLayout->addLayout(controlBtnLayout);
    mainLayout->addLayout(btnsLayout);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(mainLayout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_networkModel = new NetworkModel(this);
    m_networkWorker = new NetworkWorker(m_networkModel, this);

    m_displayModel = new DisplayModel(this);
    m_displayWorker = new DisplayWorker(m_displayModel, this);

    WifiPage *wifiPage = new WifiPage(m_networkModel);

#ifndef DISABLE_BLUETOOTH
    m_bluetoothWorker = &BluetoothWorker::Instance();
    m_bluetoothModel = m_bluetoothWorker->model();
    m_bluetoothWorker->activate();

    BluetoothList *bluetoothList = new BluetoothList(m_bluetoothModel);
#endif

    dcc::display::DisplayControlPage *displayPage = new dcc::display::DisplayControlPage(/*m_displayModel*/);

    VpnControlPage *vpnPage = new VpnControlPage(m_networkModel);

    m_basicSettingsPage = new BasicSettingsPage;

    m_itemStack->addWidget(m_basicSettingsPage);
#ifndef DISABLE_BLUETOOTH
    m_itemStack->addWidget(bluetoothList);
#endif
    m_itemStack->addWidget(vpnPage);
    m_itemStack->addWidget(wifiPage);
    m_itemStack->addWidget(displayPage);

#ifndef DISABLE_BLUETOOTH
    m_btSwitch = new QuickSwitchButton(1, "bluetooth");
#endif
    m_vpnSwitch = new QuickSwitchButton(2, "VPN");
    m_wifiSwitch = new QuickSwitchButton(3, "wifi");
    m_screenShotBtn = new DImageButton(":/frame/quick_control/icons/dark/screenshot.svg",
                                       ":/frame/quick_control/icons/dark/screenshot.svg",
                                       ":/frame/quick_control/icons/dark/screenshot.svg");
    m_screenRecordBtn = new DImageButton(":/frame/quick_control/icons/dark/status-screen-record.svg",
                                       ":/frame/quick_control/icons/dark/status-screen-record.svg",
                                       ":/frame/quick_control/icons/dark/status-screen-record.svg");
    m_systemMonitorBtn = new DImageButton(":/frame/quick_control/icons/dark/deepin-system-monitor.svg",
                                          ":/frame/quick_control/icons/dark/deepin-system-monitor.svg",
                                          ":/frame/quick_control/icons/dark/deepin-system-monitor.svg");
    m_grandSearchBtn = new DImageButton(":/frame/quick_control/icons/dark/grand-search-light.svg",
                                          ":/frame/quick_control/icons/dark/grand-search-light.svg",
                                          ":/frame/quick_control/icons/dark/grand-search-light.svg");
    m_powerBtn = new DImageButton(":/frame/quick_control/icons/dark/power.svg",
                                  ":/frame/quick_control/icons/dark/power.svg",
                                  ":/frame/quick_control/icons/dark/power.svg");
    m_ocrBtn = new DImageButton(":/frame/quick_control/icons/dark/ocr.svg",
                                ":/frame/quick_control/icons/dark/ocr.svg",
                                ":/frame/quick_control/icons/dark/ocr.svg");
    m_scrollBtn = new DImageButton(":/frame/quick_control/icons/dark/scrollShot.svg",
                                ":/frame/quick_control/icons/dark/scrollShot.svg",
                                ":/frame/quick_control/icons/dark/scrollShot.svg");

    m_detailSwitch = new QuickSwitchButton(0, "all_settings");
    QuickSwitchButton *displaySwitch = new QuickSwitchButton(4, "display");
    m_displaySwitch = displaySwitch;

#ifndef DISABLE_BLUETOOTH
    m_btSwitch->setObjectName("QuickSwitchBluetooth");
    m_btSwitch->setAccessibleName("QuickSwitchBluetooth");
#endif
    m_vpnSwitch->setObjectName("QuickSwitchVPN");
    m_vpnSwitch->setAccessibleName("QuickSwitchVPN");
    m_wifiSwitch->setObjectName("QuickSwitchWiFi");
    m_wifiSwitch->setAccessibleName("QuickSwitchWiFi");

    displaySwitch->setObjectName("QuickSwitchDisplay");
    displaySwitch->setAccessibleName("QuickSwitchDisplay");
    displaySwitch->setCheckable(false);
    displaySwitch->setChecked(true);

    m_detailSwitch->setObjectName("QuickSwitchAllSettings");
    m_detailSwitch->setAccessibleName("QuickSwitchAllSettings");
    m_detailSwitch->setCheckable(false);
    m_detailSwitch->setBackgroundVisible(false);
    m_detailSwitch->setVisible(1);

    m_switchs.append(m_detailSwitch);
#ifndef DISABLE_BLUETOOTH
    m_switchs.append(m_btSwitch);
#endif
    m_switchs.append(m_vpnSwitch);
    m_switchs.append(m_wifiSwitch);
    m_switchs.append(displaySwitch);
    //m_switchs.append(m_screenShotBtn);

/*#ifndef DISABLE_BLUETOOTH
    btnsLayout->addWidget(m_btSwitch);
#endif
    btnsLayout->addWidget(m_vpnSwitch);
    btnsLayout->addWidget(m_wifiSwitch);
    btnsLayout->addWidget(displaySwitch);*/

    //btnsLayout->addWidget(m_detailSwitch);
    btnsLayout->setContentsMargins(0, 0, 0, 0);

    controlBtnLayout->setContentsMargins(5, 5, 5, 5);
    controlBtnLayout->addWidget(m_btSwitch, 0, 0);
    controlBtnLayout->addWidget(m_vpnSwitch, 0, 1);
    controlBtnLayout->addWidget(m_wifiSwitch, 0, 2);
    controlBtnLayout->addWidget(displaySwitch, 0, 3);
    controlBtnLayout->addWidget(m_screenShotBtn, 2, 0);
    controlBtnLayout->addWidget(m_screenRecordBtn, 2, 1);
    controlBtnLayout->addWidget(m_ocrBtn, 2, 2);
    controlBtnLayout->addWidget(m_scrollBtn, 2, 3);
    controlBtnLayout->addWidget(new QLabel(), 3, 0);
    controlBtnLayout->addWidget(m_systemMonitorBtn, 4, 0);
    controlBtnLayout->addWidget(m_grandSearchBtn, 4, 1);
    controlBtnLayout->addWidget(m_detailSwitch, 5, 0);
    controlBtnLayout->addWidget(m_powerBtn, 5, 3);

    if (m_isInChroot) {
        m_powerBtn->setVisible(false);
    }

#ifndef DISABLE_BLUETOOTH
    //connect(m_btSwitch, &QuickSwitchButton::hovered, m_itemStack, &QStackedLayout::setCurrentIndex);
#endif
    //connect(m_vpnSwitch, &QuickSwitchButton::hovered, m_itemStack, &QStackedLayout::setCurrentIndex);
    //connect(m_wifiSwitch, &QuickSwitchButton::hovered, m_itemStack, &QStackedLayout::setCurrentIndex);
    connect(m_screenShotBtn, &DImageButton::clicked, this, [](){
        // QProcess::startDetached("deepin-screen-recorder --shot");
        startAppWithEffect("deepin-screen-recorder", {"--shot"});
    });
    connect(m_screenRecordBtn, &DImageButton::clicked, this, [](){
        // QProcess::startDetached("deepin-screen-recorder --record");
        startAppWithEffect("deepin-screen-recorder", {"--record"});
    });
    connect(m_systemMonitorBtn, &DImageButton::clicked, this, [](){
        // QProcess::startDetached("deepin-system-monitor");
        startAppWithEffect("deepin-system-monitor", {});
    });
    connect(m_grandSearchBtn, &DImageButton::clicked, this, [](){
        // QProcess::startDetached("dde-grand-search");
        startAppWithEffect("dde-grand-search", {});
    });
    connect(m_powerBtn, &DImageButton::clicked, this, [](){
        // QProcess::startDetached("dde-shutdown");
        startAppWithEffect("dde-shutdown", {});
    });
    connect(m_ocrBtn, &DImageButton::clicked, this, [](){
        // QProcess::startDetached("deepin-screen-recorder --ocr");
        startAppWithEffect("deepin-screen-recorder", {"--ocr"});
    });
    connect(m_scrollBtn, &DImageButton::clicked, this, [](){
        // QProcess::startDetached("deepin-screen-recorder --scroll");
        startAppWithEffect("deepin-screen-recorder", {"--scroll"});
    });


    connect(m_detailSwitch, &QuickSwitchButton::hovered, m_itemStack, &QStackedLayout::setCurrentIndex);
    connect(m_detailSwitch, &QuickSwitchButton::clicked, this, &QuickControlPanel::requestDetailConfig);

    connect(m_networkModel, &NetworkModel::vpnEnabledChanged, m_vpnSwitch, &QuickSwitchButton::setChecked);
    connect(m_networkModel, &NetworkModel::connectionListChanged, this, &QuickControlPanel::onNetworkConnectionListChanged);
    connect(m_vpnSwitch, &QuickSwitchButton::checkedChanged, m_networkWorker, &NetworkWorker::setVpnEnable);
    connect(vpnPage, &VpnControlPage::mouseLeaveView, this, [=] { m_itemStack->setCurrentIndex(0); });
    connect(vpnPage, &VpnControlPage::requestActivateConnection, m_networkWorker, &NetworkWorker::activateConnection);
    connect(vpnPage, &VpnControlPage::requestDisconnect, m_networkWorker, &NetworkWorker::deactiveConnection);
    //connect(m_wifiSwitch, &QuickSwitchButton::hovered, m_networkWorker, &NetworkWorker::requestWirelessScan);


#ifndef DISABLE_BLUETOOTH
    connect(m_btSwitch, &QuickSwitchButton::checkedChanged, this, &QuickControlPanel::onBluetoothButtonClicked);
    connect(m_bluetoothWorker, &BluetoothWorker::deviceEnableChanged, this, &QuickControlPanel::onBluetoothDeviceEnableChanged);
    connect(m_bluetoothModel, &BluetoothModel::adpaterListChanged, this, &QuickControlPanel::onBluetoothDeviceListChanged);
#endif

    connect(m_networkModel, &NetworkModel::deviceEnableChanged, this, &QuickControlPanel::onNetworkDeviceEnableChanged);
    connect(m_networkModel, &NetworkModel::deviceListChanged, this, &QuickControlPanel::onNetworkDeviceListChanged);
    connect(m_wifiSwitch, &QuickSwitchButton::checkedChanged, this, &QuickControlPanel::onWirelessButtonClicked);
    connect(wifiPage, &WifiPage::mouseLeaveView, this, [=] { m_itemStack->setCurrentIndex(0); });
    connect(wifiPage, &WifiPage::requestDeviceApList, m_networkWorker, &NetworkWorker::queryAccessPoints);
    connect(wifiPage, &WifiPage::requestActivateAccessPoint, m_networkWorker, &NetworkWorker::activateAccessPoint);
    connect(wifiPage, &WifiPage::requestDeactivateConnection, m_networkWorker, &NetworkWorker::deactiveConnection);
    connect(wifiPage, &WifiPage::requestPage, this, &QuickControlPanel::requestPage);

    connect(m_displayModel, &DisplayModel::monitorListChanged, [=] { displaySwitch->setEnabled(m_displayModel->monitorList().size() > 1); reconnectDisplaySwitch(); });
    //connect(displayPage, &dcc::display::DisplayControlPage::mouseLeaveView, this, [=] { m_itemStack->setCurrentIndex(0); });
    connect(displayPage, &dcc::display::DisplayControlPage::requestOnlyMonitor, [=](const QString &name) { m_displayWorker->switchMode(SINGLE_MODE, name); m_displayWorker->saveChanges(); });
    connect(displayPage, &dcc::display::DisplayControlPage::requestDuplicateMode, [=] { m_displayWorker->switchMode(MERGE_MODE); m_displayWorker->saveChanges(); });
    connect(displayPage, &dcc::display::DisplayControlPage::requestExtendMode, [=] { m_displayWorker->switchMode(EXTEND_MODE); m_displayWorker->saveChanges(); });
    connect(displayPage, &dcc::display::DisplayControlPage::requestConfig, m_displayWorker, &DisplayWorker::switchConfig);
    connect(displayPage, &dcc::display::DisplayControlPage::requestCustom, [=] { Q_EMIT requestPage("display", QString(), false); });


#ifndef DISABLE_BLUETOOTH
    connect(bluetoothList, &BluetoothList::mouseLeaveView, this, [=] { m_itemStack->setCurrentIndex(0); });
    //connect(bluetoothList, &BluetoothList::requestConnect, m_bluetoothWorker, &bluetooth::BluetoothWorker::connectDevice);
    //connect(bluetoothList, &BluetoothList::requestDisConnect, m_bluetoothWorker, &bluetooth::BluetoothWorker::disconnectDevice);
    connect(bluetoothList, &BluetoothList::requestDetailPage, this, &QuickControlPanel::requestPage);
    connect(bluetoothList, &BluetoothList::requestAdapterDiscoverable, m_bluetoothWorker, &bluetooth::BluetoothWorker::setAdapterDiscoverable);
#endif

    connect(m_itemStack, &QStackedLayout::currentChanged, this, &QuickControlPanel::onIndexChanged);

    displaySwitch->setEnabled(m_displayModel->monitorList().size() > 1);
    reconnectDisplaySwitch();

    m_vpnSwitch->setChecked(m_networkModel->vpnEnabled());
    onNetworkDeviceEnableChanged();
    onNetworkDeviceListChanged();

#ifndef DISABLE_BLUETOOTH
    onNetworkConnectionListChanged();
    onBluetoothDeviceEnableChanged();
    onBluetoothDeviceListChanged();
#endif
}

void QuickControlPanel::reconnectDisplaySwitch()
{
    if (m_displayModel->monitorList().size() > 1) {
        m_displaySwitch->setChecked(1);
        connect(m_displaySwitch, &QuickSwitchButton::hovered, m_itemStack, &QStackedLayout::setCurrentIndex);
        return;
    }
    m_displaySwitch->setChecked(0);
    disconnect(m_displaySwitch, &QuickSwitchButton::hovered, 0, 0);
}

void QuickControlPanel::setAllSettingsVisible(const bool visible)
{
    m_detailSwitch->setVisible(visible);
}

void QuickControlPanel::setMPRISEnable(const bool enable)
{
    m_basicSettingsPage->setMPRISEnable(enable);
}

void QuickControlPanel::setMPRISPictureEnable(const bool enable)
{
    m_basicSettingsPage->setMPRISPictureEnable(enable);
}

void QuickControlPanel::appear()
{
#ifndef DISABLE_BLUETOOTH
    m_bluetoothWorker->blockDBusSignals(false);
#endif
    m_networkWorker->active();
}

void QuickControlPanel::disappear()
{
#ifndef DISABLE_BLUETOOTH
    m_bluetoothWorker->blockDBusSignals(true);
#endif
    m_networkWorker->deactive();
}

void QuickControlPanel::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);

    m_itemStack->setCurrentIndex(0);
}

void QuickControlPanel::onNetworkDeviceEnableChanged()
{
    for (auto *dev : m_networkModel->devices())
    {
        if (dev->type() == NetworkDevice::Wireless && dev->enabled())
        {
            m_wifiSwitch->setChecked(true);
            return;
        }
    }

    m_wifiSwitch->setChecked(false);
}

void QuickControlPanel::onNetworkDeviceListChanged()
{
    for (auto *dev : m_networkModel->devices())
    {
        if (dev->type() == NetworkDevice::Wireless)
        {
            //m_wifiSwitch->setVisible(true);
            m_wifiSwitch->setEnabled(true);
            return;
        }
    }

    m_wifiSwitch->setEnabled(false);
    m_itemStack->setCurrentIndex(0);
}

void QuickControlPanel::onNetworkConnectionListChanged()
{
    const bool state = m_networkModel->vpns().size();
    m_vpnSwitch->setEnabled(state);

    if (!state)
        m_itemStack->setCurrentIndex(0);
}

void QuickControlPanel::onWirelessButtonClicked()
{
    const bool enable = m_wifiSwitch->checked();

    for (auto *dev : m_networkModel->devices())
    {
        if (dev->type() == NetworkDevice::Wireless)
            m_networkWorker->setDeviceEnable(dev->path(), enable);
    }
}

#ifndef DISABLE_BLUETOOTH
void QuickControlPanel::onBluetoothDeviceEnableChanged()
{
    QTimer::singleShot(100, this,[=]{
        for (const Adapter *adapter : m_bluetoothModel->adapters()) {
            if (adapter->powered()) {
                m_btSwitch->setChecked(true);
                return;
            }
        }

        m_btSwitch->setChecked(false);
    });
}

void QuickControlPanel::onBluetoothButtonClicked(const bool checked)
{
    for (const Adapter *adapter : m_bluetoothModel->adapters()) {
        m_bluetoothWorker->setAdapterPowered(adapter, checked);
    }
}

void QuickControlPanel::onBluetoothDeviceListChanged()
{
    for (const Adapter *adapter : m_bluetoothModel->adapters()) {
        Q_UNUSED(adapter);
        m_btSwitch->setEnabled(true);
        return;
    }

    m_btSwitch->setEnabled(false);
    m_itemStack->setCurrentIndex(0);
}
#endif

void QuickControlPanel::onIndexChanged(const int index)
{
    for (int i(0); i != m_switchs.size(); ++i)
        m_switchs[i]->setSelected(i == index);
}
