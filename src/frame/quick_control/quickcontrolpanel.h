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

#ifndef QUICKCONTROLPANEL_H
#define QUICKCONTROLPANEL_H

#include <QWidget>
#include <QStackedLayout>
#include <QPushButton>
#include "dimagebutton.h"

#define CHROOTCHECKDESTINATION "com.gxde.daemon.system.info"
#define CHROOTCHECKPATH "/com/gxde/daemon/system/info"
#define CHROOTCHECKINTERFACE "com.gxde.daemon.system.info"

DWIDGET_USE_NAMESPACE

namespace dcc {

class QuickSwitchButton;
class BasicSettingsPage;

namespace display {

class DisplayModel;
class DisplayWorker;

}

#ifndef DISABLE_BLUETOOTH
namespace bluetooth {

class BluetoothModel;
class BluetoothWorker;

}
#endif
}

namespace dde {
namespace network {
class NetworkModel;
class NetworkWorker;
}
}

class QuickControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit QuickControlPanel(QWidget *parent = 0);

    void setAllSettingsVisible(const bool visible);
    void setMPRISEnable(const bool enable);
    void setMPRISPictureEnable(const bool enable);

    void appear();
    void disappear();

Q_SIGNALS:
    void requestDetailConfig() const;
    void requestPage(const QString &module, const QString &page, bool animation);
    void screenCaptureStarted() const;

protected:
    void leaveEvent(QEvent *e);

private Q_SLOTS:
    void onNetworkDeviceEnableChanged();
    void onNetworkDeviceListChanged();
    void onNetworkConnectionListChanged();
    void onWirelessButtonClicked();
#ifndef DISABLE_BLUETOOTH
    void onBluetoothDeviceEnableChanged();
    void onBluetoothButtonClicked(const bool checked);
    void onBluetoothDeviceListChanged();
#endif
    void onIndexChanged(const int index);

    void reconnectDisplaySwitch();

private:
    // 添加声明
    void startAppWithEffect(const QString &program, const QStringList &arguments);

    QStackedLayout *m_itemStack;
    dde::network::NetworkModel *m_networkModel;
    dde::network::NetworkWorker *m_networkWorker;
    dcc::display::DisplayModel *m_displayModel;
    dcc::display::DisplayWorker *m_displayWorker;

#ifndef DISABLE_BLUETOOTH
    dcc::bluetooth::BluetoothModel *m_bluetoothModel;
    dcc::bluetooth::BluetoothWorker *m_bluetoothWorker;
#endif

    dcc::BasicSettingsPage *m_basicSettingsPage;

    dcc::QuickSwitchButton *m_wifiSwitch;
#ifndef DISABLE_BLUETOOTH
    dcc::QuickSwitchButton *m_btSwitch;
#endif
    dcc::QuickSwitchButton *m_vpnSwitch;
    dcc::QuickSwitchButton *m_detailSwitch;
    dcc::QuickSwitchButton *m_displaySwitch;

    bool m_isInChroot;

    DImageButton *m_screenShotBtn;
    DImageButton *m_screenRecordBtn;
    DImageButton *m_systemMonitorBtn;
    DImageButton *m_grandSearchBtn;
    DImageButton *m_powerBtn;
    DImageButton *m_ocrBtn;
    DImageButton *m_scrollBtn;

    QList<dcc::QuickSwitchButton *> m_switchs;
};

#endif // QUICKCONTROLPANEL_H
