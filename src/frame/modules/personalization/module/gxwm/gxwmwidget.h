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

#ifndef SRC_FRAME_MODULES_PERSONALIZATION_MODULE_GXWM_GXWMWIDGET_H_
#define SRC_FRAME_MODULES_PERSONALIZATION_MODULE_GXWM_GXWMWIDGET_H_

#include "widgets/contentwidget.h"

class QDBusInterface;

namespace dcc {
namespace widgets {
class OptionItem;
class SwitchWidget;
}
namespace personalization {

class GxwmWidget : public ContentWidget {
    Q_OBJECT

public:
    explicit GxwmWidget(QWidget *parent = nullptr);

Q_SIGNALS:
    void requestShowBlurSettings() const;

private Q_SLOTS:
    void onGtkButtonsChanged();
    void onMinimizeEffectChanged(int effect);
    void onForceRoundCornerChanged(bool enabled);
    void onExcludeLayerShellChanged(bool enabled);

private:
    void refresh();
    void setMinimizeEffectSelection(int effect);

private:
    QDBusInterface *m_windowBtnIface;
    QDBusInterface *m_windowCornerIface;
    QDBusInterface *m_viewIface;
    widgets::OptionItem *m_scaleOption;
    widgets::OptionItem *m_magicLampOption;
    widgets::SwitchWidget *m_minBtnSwitch;
    widgets::SwitchWidget *m_maxBtnSwitch;
    widgets::SwitchWidget *m_closeBtnSwitch;
    widgets::SwitchWidget *m_forceRoundCornerSwitch;
    widgets::SwitchWidget *m_excludeLayerShellSwitch;
};

}  // namespace personalization
}  // namespace dcc

#endif  // SRC_FRAME_MODULES_PERSONALIZATION_MODULE_GXWM_GXWMWIDGET_H_
