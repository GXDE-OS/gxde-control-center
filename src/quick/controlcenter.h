/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
 * Copyright (C) 2026 shenmo <jifengshenmo@outlook.com>
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

#ifndef CONTROLCENTER_H
#define CONTROLCENTER_H

#include <com_deepin_daemon_appearance.h>
#include <com_deepin_daemon_display.h>
#include <com_deepin_dde_launcher.h>

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QVariantAnimation>
#include <QtQml/qqmlregistration.h>

class QQuickWindow;
class QScreen;
class QTimer;

#ifdef HAS_LAYER_SHELL
namespace LayerShellQt {
class Window;
}
#endif

namespace Dtk {
namespace Widget {
class DRegionMonitor;
}
}

namespace dcc {
namespace quick {

// The panel window, like the window parts of the old Frame: it sits on the
// right of the screen, slides in and out, hides on outside clicks, follows
// the primary screen and sets the background opacity. Pages are in QML;
// this class only tells QML what to show.
class ControlCenter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ControlCenter is provided by the application")

    Q_PROPERTY(bool shown READ shown NOTIFY shownChanged)
    // Background color. Same as DBlurEffectWidget::DarkColor, with the
    // opacity from the appearance settings.
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(int frameWidth READ frameWidth CONSTANT)
    Q_PROPERTY(int navigationWidth READ navigationWidth CONSTANT)
    // Page animation time, like FrameWidget. Half as long without a compositor.
    Q_PROPERTY(int pageAnimationDuration READ pageAnimationDuration NOTIFY pageAnimationDurationChanged)
    // Fade pages while they slide (off with DISABLE_OPACITY_ANIMATION).
    Q_PROPERTY(bool pageOpacityAnimation READ pageOpacityAnimation CONSTANT)
    // False with DISABLE_MAIN_PAGE. All Settings then has no back button.
    Q_PROPERTY(bool mainPageEnabled READ mainPageEnabled CONSTANT)

public:
    explicit ControlCenter(QObject *parent = nullptr);
    ~ControlCenter() override;

    void setWindow(QQuickWindow *window);
    // The standalone window, from shenmo's GXDE CC Ultimate.
    void setSettingsWindow(QQuickWindow *window);

    bool shown() const;
    QColor backgroundColor() const;
    int frameWidth() const;
    int navigationWidth() const;
    int pageAnimationDuration() const;
    bool pageOpacityAnimation() const;
    bool mainPageEnabled() const;

    // Panel position for the D-Bus Rect property. Native pixels on X11.
    QRect rect() const;

public Q_SLOTS:
    void startup();
    void show();
    void hide();
    void toggle();
    void hideImmediately();
    void showSettingsPage(const QString &module, const QString &page, bool animation = false);
    void backToHome();
    void backToNotify();
    void backToNav();
    // A module wants the panel to stay, e.g. for the display confirm dialog.
    void setAutoHide(bool autoHide);
    // For debugging: turn off auto hide over D-Bus.
    void setDebugAutoHide(bool autoHide);
    // Screenshots take the focus for a moment. Do not hide on that.
    void startScreenCaptureGuard();
    void openManual();
    // Show the standalone window on its home page, or on a module page.
    void showWindowHome();
    void showWindow(const QString &module, const QString &page);

Q_SIGNALS:
    void shownChanged();
    void backgroundColorChanged();
    void pageAnimationDurationChanged();
    void rectChanged(const QRect &rect);
    void destRectChanged(const QRect &rect);

    // QML handles these.
    void settingsPageRequested(const QString &module, const QString &page, bool animation);
    void homeRequested();
    void notifyRequested();
    void navRequested();
    // The panel finished sliding in, or starts sliding out.
    void appeared();
    void disappeared();
    // Load All Settings early: after the panel shows, or at start with
    // DCC_PREPARE_INIT.
    void prepareRequested();
    // The panel was hidden for a while; free the modules. QML ignores it
    // when a module page is open.
    void releaseRequested();
    // The window QML shows this module and page. An empty module means home.
    void windowPageRequested(const QString &module, const QString &page);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool isWayland() const;
    bool slideEnabled() const;
    QScreen *targetScreen() const;
    QScreen *currentScreen() const;
    QRect waylandFrameRect() const;
    void onScreenRectChanged(const QRect &primaryRect);
    void updateFramePosition();
    void updateLayerShellMargins();
    void configureLayerShell();
    void applyBlur();
    void onMouseButtonReleased(const QPoint &point, int flag);
    bool checkOnBoard(const QPoint &point) const;
    void onDelayKillerTimeout();
    void finishHide();
    void updateBackgroundColor();

    QPointer<QQuickWindow> m_window;
    QPointer<QQuickWindow> m_settingsWindow;

    com::deepin::daemon::Display *m_displayInter;
    com::deepin::dde::Launcher *m_launcherInter;
    com::deepin::daemon::Appearance *m_appearanceInter;
    Dtk::Widget::DRegionMonitor *m_mouseAreaInter;

    QTimer *m_delayKillerTimer;
    QTimer *m_screenCaptureGuardTimer;
    // Animates the window geometry on X11 and the layer-shell margin on Wayland.
    QVariantAnimation *m_slideAnimation;

#ifdef HAS_LAYER_SHELL
    LayerShellQt::Window *m_layerShellWindow;
#endif

    QRect m_primaryRect;
    QString m_currentOutputName;
    int m_waylandSlideOffset;
    qreal m_opacity;
    QColor m_backgroundColor;
    bool m_compositing;
    bool m_shown;
    bool m_autoHide;
    bool m_debugAutoHide;
};

} // namespace quick
} // namespace dcc

#endif // CONTROLCENTER_H
