/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     listenerri <190771752ri@gmail.com>
 *
 * Maintainer: listenerri <190771752ri@gmail.com>
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

#include "notifymodel.h"

#include <QFont>
#include <QFontMetrics>

NotifyModel::NotifyModel(QObject *parent) : QAbstractListModel(parent)
{
    m_dbus = new Notification("com.deepin.dde.Notification", "/com/deepin/dde/Notification", QDBusConnection::sessionBus(), this);
    m_dbus->setSync(false);

    m_removeIndex = QModelIndex();
    m_hoverIndex = QModelIndex();

    m_currentXOffset = 0;
    m_maxXOffset = 0;
    m_animTimerID = 0;
    m_isClearAll = false;

    QDBusPendingReply<QString> notify = m_dbus->GetAllRecords();
    QDBusPendingCallWatcher *notifyWatcher = new QDBusPendingCallWatcher(notify, this);
    connect(notifyWatcher, &QDBusPendingCallWatcher::finished, this, &NotifyModel::onNotifyGetAllFinished);

    connect(m_dbus, &Notification::RecordAdded, this, &NotifyModel::addNotify);
}

int NotifyModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    // FIXME: Need to use an extra piece of data to update the hover
    const int size = m_dataJsonArray.size();

    return size + 1;
}

QVariant NotifyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() && m_dataJsonArray.size() <= 0 && index.row() > m_dataJsonArray.size()) {
        return QVariant();
    }

    QJsonObject notifyObject = m_dataJsonArray.at(m_dataJsonArray.size() - 1 - index.row()).toObject();

    switch (role) {
    case NotifyIdRole:
        return notifyObject.value("id").toString();
        break;
    case NotifyNameRole:
        return notifyObject.value("name").toString();
        break;
    case NotifySummaryRole:
        return notifyObject.value("summary").toString();
        break;
    case NotifyBodyRole:
        return notifyObject.value("body").toString();
        break;
    case NotifyIconRole:
        return notifyObject.value("icon").toString();
        break;
    case NotifyTimeRole:
        return notifyObject.value("time").toString();
        break;
    case Qt::SizeHintRole:
        if (index.row() == m_dataJsonArray.size()) {
            return QSize(0, 10);
        }
        if (m_expandedRows.contains(index.row())) {
            // 展开时根据正文内容计算所需高度
            QJsonObject notifyObject = m_dataJsonArray.at(m_dataJsonArray.size() - 1 - index.row()).toObject();
            const QString &strBody = notifyObject.value("body").toString();
            const QString &strSum = notifyObject.value("summary").toString();
            const QString &text = strBody.isEmpty() ? strSum : strBody;
            QFont font;
            font.setPixelSize(13);
            QFontMetrics fm(font);
            const int lineHeight = fm.lineSpacing();
            const int bodyWidth = 300; // 与绘制区域估算一致
            const int textHeight = fm.boundingRect(QRect(0, 0, bodyWidth, 1),
                                                   Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, text).height();
            const int height = 10 /*summary top*/ + 16 /*summary height*/
                               + 5 /*spacing*/ + textHeight + 5 /*bottom*/ + 16 /*hint*/;
            return QSize(0, qMax(80, height));
        }
        return QSize(0, 80);
        break;
    case NotifyRemoveRole: {
        if (m_isClearAll || m_removeIndex == index) {
            return true;
        }
        return false;
        break;
    }
    case NotifyXOffsetRole:
        return m_currentXOffset;
        break;
    case NotifyHoverRole:
        return m_hoverIndex == index;
        break;
    case NotifyExpandRole:
        return m_expandedRows.contains(index.row());
        break;
    case NotifyFoldableRole: {
        // 仅在展开时、或正文超出可见区域时判定为可折叠
        return index.row() < m_dataJsonArray.size() && isBodyFoldable(index.row());
        break;
    }
    default:
        break;
    }

    return QVariant();
}

Qt::ItemFlags NotifyModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
    }
    return QAbstractListModel::flags(index);
}

void NotifyModel::clearAllNotify()
{
    m_dbus->ClearRecords();
    beginResetModel();
    m_dataJsonArray = {};
    m_expandedRows.clear();
    endResetModel();
    Q_EMIT notifyClearStateChanged(true);
}

void NotifyModel::onNotifyGetAllFinished(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<QString> reply = *w;

    QJsonDocument jsonDocument = QJsonDocument::fromJson(reply.value().toLocal8Bit().data());

    beginResetModel();
    m_dataJsonArray = jsonDocument.array();
    endResetModel();

    if (m_dataJsonArray.isEmpty()) {
        Q_EMIT notifyClearStateChanged(true);
    }

    w->deleteLater();
}

void NotifyModel::addNotify(const QString &s)
{
    if (m_dataJsonArray.isEmpty()) {
        Q_EMIT notifyClearStateChanged(false);
    }

    m_dataJsonArray.append(QJsonDocument::fromJson(s.toLocal8Bit().data()).object());
    const QModelIndex mindex = index(0);
    Q_EMIT dataChanged(mindex, mindex);
}

void NotifyModel::showClearAllAnim(int maxXOffset)
{
    m_isClearAll = true;
    m_maxXOffset = maxXOffset;
    if (m_animTimerID == 0) {
        m_animTimerID = startTimer(10);
    }
}

void NotifyModel::showRemoveAnim(const QModelIndex &removeIndex, int maxXOffset)
{
    m_removeIndex = removeIndex;
    m_maxXOffset = maxXOffset;
    if (m_animTimerID == 0) {
        m_animTimerID = startTimer(10);
    }
}

void NotifyModel::setHoverIndex(const QModelIndex &index)
{
    m_hoverIndex = index;
    Q_EMIT dataChanged(m_hoverIndex, m_hoverIndex);
}

void NotifyModel::toggleExpand(const QModelIndex &index)
{
    if (!index.isValid() || index.row() >= m_dataJsonArray.size()) {
        return;
    }

    if (!isBodyFoldable(index.row())) {
        return;
    }

    if (m_expandedRows.contains(index.row())) {
        m_expandedRows.remove(index.row());
    } else {
        m_expandedRows.insert(index.row());
    }

    Q_EMIT layoutChanged();
    Q_EMIT dataChanged(index, index);
}

bool NotifyModel::isBodyFoldable(int row) const
{
    if (row < 0 || row >= m_dataJsonArray.size()) {
        return false;
    }

    QJsonObject notifyObject = m_dataJsonArray.at(m_dataJsonArray.size() - 1 - row).toObject();
    const QString &strBody = notifyObject.value("body").toString();
    const QString &strSum = notifyObject.value("summary").toString();
    const QString &text = strBody.isEmpty() ? strSum : strBody;

    if (text.isEmpty()) {
        return false;
    }

    // 估算正文在固定 80px 高度内能否完整显示
    // 与 NotifyDelegate 的绘制区域保持一致：左侧图标 48 + 间距，右侧留白
    const int bodyHeight = 80 - 10 /*summary top*/ - 16 /*summary height*/
                           - 5 /*spacing*/ - 5 /*bottom*/;
    QFont font;
    font.setPixelSize(13);
    QFontMetrics fm(font);
    const int lineHeight = fm.lineSpacing();
    if (lineHeight <= 0) {
        return false;
    }
    const int maxLines = bodyHeight / lineHeight;
    const int textLines = fm.boundingRect(QRect(0, 0, 300, 1),
                                          Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, text).height() / lineHeight;

    return textLines > maxLines;
}

void NotifyModel::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event)

    if (m_currentXOffset < m_maxXOffset) {
        m_currentXOffset += 20;
        if (m_isClearAll) {
            Q_EMIT layoutChanged();
        } else {
            Q_EMIT dataChanged(m_removeIndex, m_removeIndex);
        }
        return;
    }

    if (m_animTimerID != 0) {
        killTimer(m_animTimerID);
    }

    if (m_isClearAll) {
        Q_EMIT clearAllAnimFinished();
    } else {
        Q_EMIT removeAnimFinished(m_removeIndex);
    }
    m_isClearAll = false;
    m_animTimerID = 0;
    m_currentXOffset = 0;
    m_removeIndex = QModelIndex();
}

void NotifyModel::removeNotify(const QModelIndex &index)
{
    QJsonObject notifyObject = m_dataJsonArray.at(m_dataJsonArray.size() - 1 - index.row()).toObject();
    m_dbus->RemoveRecord(notifyObject.value("id").toString());
    m_dataJsonArray.removeAt(m_dataJsonArray.size() - 1 - index.row());
    Q_EMIT dataChanged(index, index);

    if (m_dataJsonArray.size() == 0) {
        Q_EMIT notifyClearStateChanged(true);
    }
}
