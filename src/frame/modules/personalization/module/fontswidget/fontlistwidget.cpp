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

#include "fontlistwidget.h"
#include "../../model/fontmodel.h"

#include <QLineEdit>
#include <QListView>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QVBoxLayout>

using namespace dcc;
using namespace dcc::personalization;

namespace {
constexpr int FontDataRole = Qt::UserRole + 1;
constexpr int SearchRole = Qt::UserRole + 2;
constexpr int SelectedRole = Qt::UserRole + 3;

class FontDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(200, 36);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF rect(option.rect);
        const bool first = index.row() == 0;
        const bool last = index.row() == index.model()->rowCount() - 1;
        QPainterPath background;
        background.addRoundedRect(rect, 5, 5);
        if (!first)
            background.addRect(QRectF(rect.left(), rect.top(), rect.width(), 6));
        if (!last)
            background.addRect(QRectF(rect.left(), rect.bottom() - 6, rect.width(), 6));
        background.setFillRule(Qt::WindingFill);
        painter->setClipPath(background);
        painter->fillRect(rect, QColor(238, 238, 238, 51));
        if (option.state & QStyle::State_MouseOver)
            painter->fillRect(rect, QColor(255, 255, 255, 15));
        if (!last) {
            painter->setPen(QColor(0, 0, 0, 65));
            painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        }

        // Only visible rows resolve a preview font; selection uses the same
        // circular check as the existing settings rows, without a blue bar.
        QFont font(option.font);
        font.setFamily(index.data(FontDataRole).toJsonObject()["Id"].toString());
        painter->setFont(font);
        painter->setPen(Qt::white);
        const QRect textRect = option.rect.adjusted(20, 0, -38, 0);
        const QString text = QFontMetrics(font).elidedText(index.data().toString(), Qt::ElideRight, textRect.width());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
        if (index.data(SelectedRole).toBool()) {
            const QRect check(option.rect.right() - 26, option.rect.center().y() - 8, 16, 16);
            static const QIcon selected(QStringLiteral(":/widgets/themes/dark/icons/select.svg"));
            selected.paint(painter, check);
        }
        if ((option.state & QStyle::State_HasFocus) && (option.state & QStyle::State_KeyboardFocusChange)) {
            painter->setPen(QColor(255, 255, 255, 100));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(rect.adjusted(1, 1, -1, -1), 4, 4);
        }
        painter->restore();
    }
};
}

FontListWidget::FontListWidget(const QString &title, QWidget *parent)
    : ContentWidget(parent)
{
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search fonts"));
    m_search->setAccessibleName(tr("Search fonts"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedHeight(32);
    m_search->addAction(QIcon(QStringLiteral(":/widgets/themes/dark/icons/search_dark.png")), QLineEdit::LeadingPosition);
    m_search->setStyleSheet(QStringLiteral(
        "QLineEdit { color: white; background: rgba(238,238,238,25);"
        "border: 1px solid rgba(255,255,255,30); border-radius: 5px; padding: 0 8px; }"
        "QLineEdit:focus { border-color: rgba(255,255,255,90); }"));
    m_contentTopLayout->addSpacing(10);
    m_contentTopLayout->addWidget(m_search);
    m_contentTopLayout->addSpacing(10);

    m_items = new QStandardItemModel(this);
    m_filter = new QSortFilterProxyModel(this);
    m_filter->setSourceModel(m_items);
    m_filter->setFilterRole(SearchRole);
    m_filter->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_list = new QListView;
    m_list->setModel(m_filter);
    m_list->setItemDelegate(new FontDelegate(m_list));
    m_list->setUniformItemSizes(true);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setMouseTracking(true);
    m_list->setStyleSheet(QStringLiteral("QListView { background: transparent; border: none; }"));
    m_list->viewport()->setAutoFillBackground(false);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setAccessibleName(title);
    setTitle(title);
    setContent(m_list);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_filter->setFilterFixedString(text.trimmed());
    });
    const auto select = [this](const QModelIndex &index) {
        const QJsonObject font = index.data(FontDataRole).toJsonObject();
        if (!font["type"].toString().isEmpty())
            Q_EMIT requestSetDefault(font);
    };
    connect(m_list, &QListView::clicked, this, select);
    connect(m_list, &QListView::activated, this, select);
}

void FontListWidget::setModel(FontModel *const model)
{
    m_model = model;
    connect(model, &FontModel::listChanged, this, &FontListWidget::setList);
    connect(model, &FontModel::defaultFontChanged, this, &FontListWidget::setDefault);
    setList(model->getFontList());
}

void FontListWidget::setList(const QList<QJsonObject> &list)
{
    m_items->clear();
    bool found = false;
    for (const QJsonObject &font : list) {
        auto *item = new QStandardItem(font["Name"].toString());
        item->setData(font, FontDataRole);
        item->setData(font["Name"].toString() + ' ' + font["Id"].toString()
                      + ' ' + font["Aliases"].toString(), SearchRole);
        m_items->appendRow(item);
        found |= font["Id"].toString() == m_model->getFontName();
    }
    if (!found && !m_model->getFontName().isEmpty()) {
        const QString name = m_model->getFontName();
        auto *item = new QStandardItem(name + tr(" (Unsupported font)"));
        item->setData(QJsonObject{{"Id", name}}, FontDataRole);
        item->setData(name, SearchRole);
        m_items->appendRow(item);
    }
    setDefault(m_model->getFontName());
}

void FontListWidget::setDefault(const QString &name)
{
    for (int row = 0; row < m_items->rowCount(); ++row) {
        auto *item = m_items->item(row);
        item->setData(item->data(FontDataRole).toJsonObject()["Id"].toString() == name, SelectedRole);
    }
}
