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

#ifndef SRC_FRAME_MODULES_PERSONALIZATION_MODULE_GXDM_GXDMWIDGET_H_
#define SRC_FRAME_MODULES_PERSONALIZATION_MODULE_GXDM_GXDMWIDGET_H_

#include "widgets/contentwidget.h"

class QDBusInterface;

namespace dcc {
namespace widgets {
class SwitchWidget;
}
namespace personalization {

class ThemeModel;

class GxdmWidget : public ContentWidget {
    Q_OBJECT

public:
    explicit GxdmWidget(QWidget *parent = nullptr);
    static bool isAvailable();

Q_SIGNALS:
    void requestFrameKeepAutoHide(const bool autoHide) const;
    void requestShowCursorThemes() const;

private Q_SLOTS:
    void onGreeterServerChanged(bool enabled);
    void onWallpaperChanged(const QString &path);
    void onLockWallpaperChanged(const QString &path);

private:
    void refresh();

private:
    QDBusInterface *m_displayManagerIface;
    widgets::SwitchWidget *m_x11GreeterSwitch;
};

// 欢迎界面鼠标指针主题的二级页面
class GxdmCursorThemeWidget : public ContentWidget {
    Q_OBJECT

public:
    explicit GxdmCursorThemeWidget(ThemeModel *cursorModel,
        QWidget *parent = nullptr);

private:
    QDBusInterface *m_displayManagerIface;
};

}  // namespace personalization
}  // namespace dcc

#endif  // SRC_FRAME_MODULES_PERSONALIZATION_MODULE_GXDM_GXDMWIDGET_H_
