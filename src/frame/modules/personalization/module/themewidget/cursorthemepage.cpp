/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
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

#include "cursorthemepage.h"

#include "theme.h"
#include "widgets/translucentframe.h"

#include <QDBusInterface>
#include <QDBusMessage>
#include <QJsonObject>
#include <QVBoxLayout>

using namespace dcc::widgets;
using namespace dcc::personalization;

CursorThemePage::CursorThemePage(const QString &title, QDBusInterface *iface,
        ThemeModel *cursorModel, QWidget *parent)
        : ContentWidget(parent)
{
    // The page owns the interface from now on.
    iface->setParent(this);

    auto *cursorTheme = new Theme(title);
    if (cursorModel) {
        cursorTheme->setModel(cursorModel);
    }

    auto *frame = new TranslucentFrame;
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(cursorTheme);
    layout->addStretch();

    setTitle(title);
    setContent(frame);

    connect(cursorTheme, &Theme::requestSetDefault, this,
        [iface, cursorTheme](const QJsonObject &value) {
            const QString id = value[QStringLiteral("Id")].toString();
            if (!iface->isValid()) {
                return;
            }
            const QDBusMessage reply = iface->call(QDBus::Block,
                QStringLiteral("SetCursor"), id);
            if (reply.type() == QDBusMessage::ReplyMessage
                && !reply.arguments().isEmpty()
                && reply.arguments().first().toBool()) {
                cursorTheme->setDefault(id);
            }
        });
}
