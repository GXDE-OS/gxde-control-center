/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     listenerri <listenerri@gmail.com>
 *
 * Maintainer: listenerri <listenerri@gmail.com>
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

#include "navdelegate.h"
#include "navmodel.h"

#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QScreen>
#include <QSvgRenderer>

NavDelegate::NavDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

void NavDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QString moduleName = index.data(Qt::WhatsThisRole).toString();
    if (!moduleName.isEmpty()) {
        bool isHover = index.data(NavModel::NavHoverRole).toBool();

        QRect rect = QRect(option.rect.left() + 5,
                           option.rect.top() + 5,
                           option.rect.width() - 10,
                           option.rect.height() - 10);

        QPainterPath path;
        path.addRoundedRect(rect, 5, 5);

        auto renderHints = painter->renderHints();
        painter->setRenderHint(QPainter::Antialiasing);

        // draw background
        if (isHover) {
            painter->fillPath(path, QColor(255, 255, 255, 25));
        } else {
            painter->fillPath(path, QColor(255, 255, 255, 7));
        }

        painter->setRenderHints(renderHints);

        // 图标直接按绘制设备分辨率渲染矢量，不用 qApp->devicePixelRatio() 预渲染
        // 位图：合成器上报分数缩放（wl_output.scale 取整 2，窗口实际 1.25）时，
        // 应用级 DPR 与绘制 DPR 不一致，预渲染位图被缩放后产生锯齿。
        QSvgRenderer renderer(QString(":/%1/themes/dark/icons/nav_%1.svg").arg(moduleName));

        // Keep and offset from the top left corner, base is 1080P
        const double Sh = qApp->primaryScreen()->geometry().height();
        double keepRatio = 1;
        if (Sh <= 1080) {
            keepRatio = Sh / 1080;
        }

        const QSize iconSize(24, 24);

        QPoint p(rect.x() + 20 * keepRatio, rect.y() + 26 * keepRatio);
        if (renderer.isValid()) {
            painter->setRenderHint(QPainter::Antialiasing);
            renderer.render(painter, QRectF(p, QSizeF(iconSize)));
            painter->setRenderHints(renderHints);
        }

        const QString &displayText = index.data(NavModel::NavDisplayRole).toString();

        QFontMetrics fontMetrics(displayText);

        if (rect.height() < static_cast<int>(iconSize.height() + fontMetrics.height() + 40 * keepRatio)) {
            p = QPoint(p.x() + iconSize.width() + 20 * keepRatio, p.y());
        }
        else {
            p = QPoint(p.x(), p.y() + iconSize.height() + 14 * keepRatio);
        }

        QTextOption option;
        option.setAlignment(Qt::AlignLeft | Qt::AlignTop);

        painter->drawText(QRect(p, QSize(rect.width() - 20 * keepRatio, rect.height())),
                          index.data(NavModel::NavDisplayRole).toString(),
                          option);
    }

    QStyledItemDelegate::paint(painter, option, index);
}
