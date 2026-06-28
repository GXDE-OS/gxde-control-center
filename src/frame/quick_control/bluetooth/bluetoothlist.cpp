/*
 * Copyright (C) 2016 ~ 2018 Deepin Technology Co., Ltd.
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

#include "bluetoothlist.h"
#include "bluetoothdelegate.h"
#include "modules/bluetooth/bluetoothmodel.h"
#include "modules/bluetooth/device.h"
#include "modules/bluetooth/adapter.h"

#include <QVBoxLayout>
#include <QEvent>

BluetoothList::BluetoothList(BluetoothModel *model, QWidget *parent)
    : QWidget(parent),
      m_model(new BluetoothListModel(model)),
      m_btModel(model)
{
    BasicListView *listView = new BasicListView;
    listView->setModel(m_model);
    listView->installEventFilter(this);

    BluetoothDelegate *delegate = new BluetoothDelegate;
    listView->setItemDelegate(delegate);

    QVBoxLayout *centralLayout = new QVBoxLayout;
    centralLayout->addStretch();
    centralLayout->addWidget(listView);
    centralLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(centralLayout);

    connect(listView, &BasicListView::entered, m_model, &BluetoothListModel::setCurrentHovered);
    connect(listView, &BasicListView::clicked, this, &BluetoothList::onItemClicked);
}

bool BluetoothList::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);

    if (event->type() == QEvent::Leave)
        Q_EMIT mouseLeaveView();

    return false;
}

void BluetoothList::onItemClicked(const QModelIndex &index) const
{
    if (index.data(BluetoothListModel::ItemIsPowerOffRole).toBool())
        return;

    if (index.data(BluetoothListModel::ItemIsHeaderRole).toBool()) {
        m_model->refreshData();
        Q_EMIT requestAdapterDiscoverable(index.data(BluetoothListModel::ItemAdapterRole).toString());
        return;
    }

    if (index.data(BluetoothListModel::ItemIsSettingRole).toBool()) {
        if (index.data(BluetoothListModel::ItemCountRole).toInt() != 1)
            Q_EMIT requestDetailPage("bluetooth", "", false);
        else
            Q_EMIT requestDetailPage("bluetooth", "bluetooth", false);
        return;
    }

    const bool connected = index.data(BluetoothListModel::ItemConnectedRole).toBool();
    const BluetoothItemInfo info = index.data(BluetoothListModel::ItemDeviceRole).value<BluetoothItemInfo>();

    /*if (connected)
        Q_EMIT requestDisConnect(info.device);
    else
        Q_EMIT requestConnect(info.device);*/
}
