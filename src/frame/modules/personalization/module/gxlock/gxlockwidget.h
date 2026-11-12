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

#ifndef SRC_FRAME_MODULES_PERSONALIZATION_MODULE_GXLOCK_GXLOCKWIDGET_H_
#define SRC_FRAME_MODULES_PERSONALIZATION_MODULE_GXLOCK_GXLOCKWIDGET_H_

#include "widgets/contentwidget.h"

class QDBusInterface;

namespace dcc {
namespace widgets {
class FileChooseWidget;
class OptionItem;
}
namespace personalization {

// Settings of gxde-lock, the in-session lock screen (top.gxde.Lock).
class GxlockWidget : public ContentWidget {
    Q_OBJECT

public:
    explicit GxlockWidget(QWidget *parent = nullptr);
    static bool isAvailable();
    static QDBusInterface *createInterface(QObject *parent = nullptr);

Q_SIGNALS:
    void requestFrameKeepAutoHide(const bool autoHide) const;
    void requestShowCursorThemes() const;

private Q_SLOTS:
    void onStyleChanged(const QString &style);

private:
    void setStyle(const QString &style);
    void refresh();

private:
    QDBusInterface *m_lockIface;
    widgets::OptionItem *m_classicStyle;
    widgets::OptionItem *m_modernStyle;
    widgets::FileChooseWidget *m_userWallpaper;
};

}  // namespace personalization
}  // namespace dcc

#endif  // SRC_FRAME_MODULES_PERSONALIZATION_MODULE_GXLOCK_GXLOCKWIDGET_H_
