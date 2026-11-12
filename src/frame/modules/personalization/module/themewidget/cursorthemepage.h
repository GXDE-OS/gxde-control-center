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

#ifndef SRC_FRAME_MODULES_PERSONALIZATION_MODULE_THEMEWIDGET_CURSORTHEMEPAGE_H_
#define SRC_FRAME_MODULES_PERSONALIZATION_MODULE_THEMEWIDGET_CURSORTHEMEPAGE_H_

#include "widgets/contentwidget.h"

class QDBusInterface;

namespace dcc {
namespace personalization {

class ThemeModel;

// Cursor theme list that sends the picked theme to a D-Bus service with
// SetCursor(s). Used for the welcome screen (GXDM) and the lock screen
// (gxde-lock).
class CursorThemePage : public ContentWidget {
    Q_OBJECT

public:
    CursorThemePage(const QString &title, QDBusInterface *iface,
        ThemeModel *cursorModel, QWidget *parent = nullptr);
};

}  // namespace personalization
}  // namespace dcc

#endif  // SRC_FRAME_MODULES_PERSONALIZATION_MODULE_THEMEWIDGET_CURSORTHEMEPAGE_H_
