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

#include "controlcenter.h"

#include "wayland/gxdecursor.h"
#include "wayland/gxdescreen.h"
#include "wayland/waylandhelper.h"

#include <dregionmonitor.h>

#include <bernese/window_effects.h>
#include <bernese/window_geometry.h>
#include <bernese/window_stack.h>

#include <QDBusServiceWatcher>
#include <QGSettings>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QProcess>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

#ifdef HAS_LAYER_SHELL
#include <LayerShellQt/window.h>
#endif

DWIDGET_USE_NAMESPACE

namespace dcc {
namespace quick {

namespace {

const int FrameWidth = 408;
const int NavigationWidth = 56;
const int PageAnimationDuration = 300;
// Default QPropertyAnimation time. The old X11 slide used it.
const int X11SlideDuration = 250;
const int WaylandSlideDuration = 300;

const int ButtonLeft = 1;
const int ButtonRight = 3;

// Mask alpha DBlurEffectWidget uses with a compositor but no blur.
const int UnblurredMaskAlpha = 204;
// Window radius DBlurEffectWidget sets on Wayland when blurRectXRadius is 0.
const int WaylandWindowRadius = 5;

} // namespace

ControlCenter::ControlCenter(QObject *parent)
    : QObject(parent)
    , m_displayInter(new com::deepin::daemon::Display("com.deepin.daemon.Display", "/com/deepin/daemon/Display", QDBusConnection::sessionBus(), this))
    , m_launcherInter(new com::deepin::dde::Launcher("com.deepin.dde.Launcher", "/com/deepin/dde/Launcher", QDBusConnection::sessionBus(), this))
    , m_appearanceInter(new com::deepin::daemon::Appearance("com.deepin.daemon.Appearance", "/com/deepin/daemon/Appearance", QDBusConnection::sessionBus(), this))
    , m_mouseAreaInter(new DRegionMonitor(this))
    , m_delayKillerTimer(new QTimer(this))
    , m_screenCaptureGuardTimer(new QTimer(this))
    , m_slideAnimation(new QVariantAnimation(this))
#ifdef HAS_LAYER_SHELL
    , m_layerShellWindow(nullptr)
#endif
    , m_primaryRect(m_displayInter->primaryRect())
    , m_waylandSlideOffset(0)
    , m_opacity(0.4)
    , m_compositing(true)
    , m_shown(false)
    , m_autoHide(true)
    , m_debugAutoHide(true)
{
    m_mouseAreaInter->setCoordinateType(DRegionMonitor::Original);
    m_appearanceInter->setSync(false, false);
    m_displayInter->setSync(false);

    m_delayKillerTimer->setSingleShot(true);
    m_delayKillerTimer->setInterval(60 * 1000);

    // A screenshot takes the keyboard focus for a moment. Do not hide then.
    m_screenCaptureGuardTimer->setSingleShot(true);
    m_screenCaptureGuardTimer->setInterval(3000);

    auto *screenRecorderWatcher = new QDBusServiceWatcher(
        QStringLiteral("com.deepin.ScreenRecorder"), QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForRegistration, this);
    connect(screenRecorderWatcher, &QDBusServiceWatcher::serviceRegistered, this, [this] {
        if (m_window && m_window->isVisible())
            startScreenCaptureGuard();
    });

    m_slideAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_slideAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        if (!m_window)
            return;

        if (isWayland()) {
            m_waylandSlideOffset = value.toInt();
            updateLayerShellMargins();
            // set_margin needs a commit, and Qt draws no frame when nothing
            // changed. Ask for a frame every step.
            m_window->update();
        } else {
            m_window->setGeometry(value.toRect());
        }
    });
    connect(m_slideAnimation, &QVariantAnimation::finished, this, [this] {
        if (!m_shown)
            finishHide();
        else if (!isWayland() && m_window)
            Q_EMIT rectChanged(rect());
    });

    // The old appearance service may be missing on Wayland. Then D-Bus gives
    // 0, and the panel must not become fully clear.
    auto setOpacity = [this](double opacity) {
        const qreal effective = (opacity > 0.05 && opacity <= 1.0) ? opacity : 0.4;
        if (qFuzzyCompare(effective, m_opacity))
            return;
        m_opacity = effective;
        updateBackgroundColor();
    };

    connect(m_delayKillerTimer, &QTimer::timeout, this, &ControlCenter::onDelayKillerTimeout);
    connect(m_displayInter, &com::deepin::daemon::Display::PrimaryRectChanged, this, &ControlCenter::onScreenRectChanged);
    connect(m_launcherInter, &com::deepin::dde::Launcher::Shown, this, &ControlCenter::hideImmediately);
    connect(m_appearanceInter, &com::deepin::daemon::Appearance::OpacityChanged, this, setOpacity);

    setOpacity(m_appearanceInter->opacity());
    updateBackgroundColor();
}

ControlCenter::~ControlCenter()
{
}

void ControlCenter::setWindow(QQuickWindow *window)
{
    m_window = window;
    if (!m_window)
        return;

    if (isWayland()) {
        m_window->setFlags(Qt::Window | Qt::FramelessWindowHint);
    } else {
        m_window->setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint
                           | Qt::WindowStaysOnTopHint);
    }

    m_window->setMaximumWidth(FrameWidth);
    m_window->installEventFilter(this);

    onScreenRectChanged(m_primaryRect);

#ifdef DCC_PREPARE_INIT
    QMetaObject::invokeMethod(this, &ControlCenter::prepareRequested, Qt::QueuedConnection);
#endif
}

bool ControlCenter::shown() const
{
    return m_shown;
}

QColor ControlCenter::backgroundColor() const
{
    return m_backgroundColor;
}

int ControlCenter::frameWidth() const
{
    return FrameWidth;
}

int ControlCenter::navigationWidth() const
{
    return NavigationWidth;
}

int ControlCenter::pageAnimationDuration() const
{
    return m_compositing ? PageAnimationDuration : PageAnimationDuration / 2;
}

bool ControlCenter::pageOpacityAnimation() const
{
#ifdef DISABLE_OPACITY_ANIMATION
    return false;
#else
    return true;
#endif
}

bool ControlCenter::mainPageEnabled() const
{
#ifdef DISABLE_MAIN_PAGE
    return false;
#else
    return true;
#endif
}

QRect ControlCenter::rect() const
{
    if (isWayland())
        return waylandFrameRect();

    if (!m_window)
        return QRect();

    return bernese::window_utils::NativeWindowGeometry(m_window);
}

void ControlCenter::startup()
{
    show();
}

void ControlCenter::show()
{
    // Already open: do not slide in again.
    if (m_shown || !m_window || m_slideAnimation->state() == QAbstractAnimation::Running)
        return;

    m_shown = true;
    Q_EMIT shownChanged();
    updateBackgroundColor();

    if (isWayland()) {
        QScreen *screen = targetScreen();
        if (!screen)
            return;

        const QRect screenRect = screen->geometry();
        m_window->setScreen(screen);
        m_window->resize(FrameWidth, screenRect.height());
        m_currentOutputName = screen->name();

        // Set the layer-shell role before the first commit, or it becomes an xdg-toplevel.
        configureLayerShell();

        // Slide like on X11: start right of the screen, end at the edge.
        const bool slide = slideEnabled();
        if (slide) {
            m_waylandSlideOffset = FrameWidth;
            updateLayerShellMargins();
        }

        m_window->show();
        m_window->raise();
        m_window->requestActivate();
        // Blur and radius belong to the surface. Set them after every show.
        applyBlur();

        if (slide) {
            Q_EMIT destRectChanged(QRect(screenRect.right() + 1, screenRect.top(), FrameWidth, screenRect.height()));
            m_slideAnimation->setDuration(WaylandSlideDuration);
            m_slideAnimation->setStartValue(FrameWidth);
            m_slideAnimation->setEndValue(0);
            m_slideAnimation->start();
        }
        Q_EMIT destRectChanged(waylandFrameRect());
    } else {
        // Grow the width from 0 with the right edge fixed. The content stays
        // on the left, so it looks like a slide and never enters the next screen.
        const QRect screenRect = qApp->primaryScreen()->geometry();
        const QRect start(screenRect.right() + 1, screenRect.top(), 0, screenRect.height());
        const QRect end(screenRect.right() + 1 - FrameWidth, screenRect.top(), FrameWidth, screenRect.height());

        Q_EMIT destRectChanged(end);
        m_slideAnimation->setDuration(X11SlideDuration);
        m_slideAnimation->setStartValue(start);
        m_slideAnimation->setEndValue(end);
        m_slideAnimation->start();

        m_window->show();
        m_window->requestActivate();
        applyBlur();

        // Watch the mouse, to hide on clicks outside the panel.
        m_mouseAreaInter->registerRegion();
        connect(m_mouseAreaInter, &DRegionMonitor::buttonRelease, this, &ControlCenter::onMouseButtonReleased,
                Qt::UniqueConnection);
    }

    QTimer::singleShot(pageAnimationDuration(), this, [this] {
        if (m_shown)
            Q_EMIT appeared();
    });

    // Load All Settings after the slide.
    m_delayKillerTimer->stop();
    QTimer::singleShot(pageAnimationDuration(), this, &ControlCenter::prepareRequested);
}

void ControlCenter::hide()
{
    if (!m_window || m_slideAnimation->state() == QAbstractAnimation::Running)
        return;

    m_shown = false;
    m_autoHide = true;
    Q_EMIT shownChanged();

    if (isWayland()) {
        // Hide the window when the slide out is done.
        if (slideEnabled() && m_window->isVisible()) {
            m_slideAnimation->setDuration(WaylandSlideDuration);
            m_slideAnimation->setStartValue(m_waylandSlideOffset);
            m_slideAnimation->setEndValue(m_waylandSlideOffset + FrameWidth);
            Q_EMIT destRectChanged(waylandFrameRect().translated(FrameWidth, 0));
            m_slideAnimation->start();
        } else {
            finishHide();
        }
    } else {
        const QRect screenRect = qApp->primaryScreen()->geometry();
        const QRect start(screenRect.right() + 1 - FrameWidth, screenRect.top(), FrameWidth, screenRect.height());
        const QRect end(screenRect.right() + 1, screenRect.top(), 0, screenRect.height());

        Q_EMIT destRectChanged(end);
        m_slideAnimation->setDuration(X11SlideDuration);
        m_slideAnimation->setStartValue(start);
        m_slideAnimation->setEndValue(end);
        m_slideAnimation->start();
    }

    Q_EMIT disappeared();

    m_mouseAreaInter->unregisterRegion();
    disconnect(m_mouseAreaInter, &DRegionMonitor::buttonRelease, this, &ControlCenter::onMouseButtonReleased);

    m_delayKillerTimer->start();
}

void ControlCenter::toggle()
{
    if (m_slideAnimation->state() == QAbstractAnimation::Running)
        return;

    if (m_shown) {
        hide();
        return;
    }

    show();
#ifdef DISABLE_MAIN_PAGE
    Q_EMIT settingsPageRequested(QString(), QString(), false);
#endif
}

void ControlCenter::hideImmediately()
{
    if (!m_window)
        return;

    m_slideAnimation->stop();
    const bool wasShown = m_shown;
    m_shown = false;
    m_autoHide = true;
    if (wasShown)
        Q_EMIT shownChanged();

    if (isWayland()) {
        m_waylandSlideOffset = 0;
        updateLayerShellMargins();
    } else {
        const QRect screenRect = qApp->primaryScreen()->geometry();
        const QRect r(screenRect.right() + 1, screenRect.top(), 0, screenRect.height());
        Q_EMIT destRectChanged(r);
        m_window->setPosition(r.topLeft());
    }

    m_window->hide();
    Q_EMIT disappeared();

    m_mouseAreaInter->unregisterRegion();
    disconnect(m_mouseAreaInter, &DRegionMonitor::buttonRelease, this, &ControlCenter::onMouseButtonReleased);

    m_delayKillerTimer->start();
}

void ControlCenter::showSettingsPage(const QString &module, const QString &page, bool animation)
{
    Q_EMIT settingsPageRequested(module, page, animation);

    if (!m_shown)
        show();
}

void ControlCenter::setSettingsWindow(QQuickWindow *window)
{
    m_settingsWindow = window;

    // Same as hiding the panel: start the timer that frees or exits.
    connect(window, &QWindow::visibleChanged, this, [this](bool visible) {
        if (!visible && !m_shown)
            m_delayKillerTimer->start();
    });
}

void ControlCenter::showWindowHome()
{
    showWindow(QString(), QString());
}

void ControlCenter::showWindow(const QString &module, const QString &page)
{
    if (!m_settingsWindow)
        return;

    // Same as the fork: the panel goes away when the window opens.
    if (m_shown) {
        backToHome();
        hideImmediately();
    }

    Q_EMIT windowPageRequested(module, page);

    m_settingsWindow->show();
    m_settingsWindow->raise();
    m_settingsWindow->requestActivate();
    // The window draws its own GXTitleBar, so the compositor must not draw
    // one. Like blur, this belongs to the surface, which only exists once
    // show() returns: set it after every show. On X11 the window manager
    // frame stays.
    bernese::window_effects::SetNoTitleBar(m_settingsWindow, true);
}

void ControlCenter::backToHome()
{
    Q_EMIT homeRequested();
}

void ControlCenter::backToNotify()
{
    Q_EMIT notifyRequested();
}

void ControlCenter::backToNav()
{
    Q_EMIT navRequested();
}

void ControlCenter::setAutoHide(bool autoHide)
{
    m_autoHide = autoHide;
}

void ControlCenter::setDebugAutoHide(bool autoHide)
{
    m_debugAutoHide = autoHide;
}

void ControlCenter::startScreenCaptureGuard()
{
    m_screenCaptureGuardTimer->start();
}

void ControlCenter::openManual()
{
    QProcess::startDetached("dman", QStringList("dde"));
}

bool ControlCenter::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_window)
        return false;

    if (event->type() == QEvent::KeyPress) {
        switch (static_cast<QKeyEvent *>(event)->key()) {
#ifdef QT_DEBUG
        case Qt::Key_Escape:
            qApp->quit();
            break;
#endif
        case Qt::Key_F1:
            openManual();
            break;
        default:
            break;
        }
    }

#ifdef HAS_LAYER_SHELL
    // No global mouse watch on Wayland. Losing focus counts as a click outside.
    if (isWayland() && event->type() == QEvent::FocusOut) {
        if (m_screenCaptureGuardTimer->isActive()) {
            m_screenCaptureGuardTimer->stop();
        } else if (m_window->isVisible() && m_autoHide && m_debugAutoHide) {
            hide();
        }
    }
#endif

    return false;
}

bool ControlCenter::isWayland() const
{
    return Wayland::isWaylandSession();
}

bool ControlCenter::slideEnabled() const
{
    // Without ukui_shell we cannot get the pointer position, so no slide.
    return isWayland() && GxdeScreen::isAvailable();
}

QScreen *ControlCenter::targetScreen() const
{
    // Show on the screen with the mouse.
    if (slideEnabled()) {
        const QString outputName = GxdeCursor::currentOutputName();
        for (QScreen *screen : qApp->screens()) {
            if (screen->name() == outputName)
                return screen;
        }
    }

    return qApp->primaryScreen();
}

QScreen *ControlCenter::currentScreen() const
{
    if (isWayland()) {
        for (QScreen *screen : qApp->screens()) {
            if (screen->name() == m_currentOutputName)
                return screen;
        }
        return qApp->primaryScreen();
    }

    for (QScreen *screen : qApp->screens()) {
        const QRect nativeRect = bernese::window_utils::ToNativePixels(
            screen->geometry(), screen->geometry().topLeft(), screen->devicePixelRatio());
        if (nativeRect.contains(m_primaryRect.center()))
            return screen;
    }

    return nullptr;
}

QRect ControlCenter::waylandFrameRect() const
{
    QScreen *screen = currentScreen();
    if (!screen)
        return QRect();

    const QRect screenRect = screen->geometry();
    return QRect(screenRect.right() - FrameWidth + 1 + m_waylandSlideOffset, screenRect.top(),
                 FrameWidth, screenRect.height());
}

void ControlCenter::onScreenRectChanged(const QRect &primaryRect)
{
    if (primaryRect.isEmpty() || !m_window)
        return;

    m_primaryRect = primaryRect;
    QTimer::singleShot(500, this, &ControlCenter::updateFramePosition);
}

void ControlCenter::updateFramePosition()
{
    if (!m_window)
        return;

    QScreen *screen = targetScreen();
    if (!screen)
        return;

    if (isWayland()) {
        m_window->setScreen(screen);
        m_window->resize(FrameWidth, screen->geometry().height());
        m_currentOutputName = screen->name();
        if (m_window->isVisible())
            configureLayerShell();
        Q_EMIT rectChanged(waylandFrameRect());
        return;
    }

    // The primary rect comes from D-Bus in native pixels.
    const QRect screenRect = bernese::window_utils::FromNativePixels(
        m_primaryRect, screen->geometry().topLeft(), screen->devicePixelRatio());
    const QRect frameRect(m_shown ? screenRect.right() - FrameWidth + 1 : screenRect.right() + 1,
                          screenRect.top(), FrameWidth, screenRect.height());
    bernese::window_utils::SetTopLevelWindowGeometry(m_window, screen, frameRect);

    Q_EMIT rectChanged(rect());
}

void ControlCenter::updateLayerShellMargins()
{
#ifdef HAS_LAYER_SHELL
    // With a right anchor only the right margin counts. A negative one
    // pushes the surface off the screen.
    if (m_layerShellWindow)
        m_layerShellWindow->setMargins(QMargins(0, 0, -m_waylandSlideOffset, 0));
#endif
    Q_EMIT rectChanged(waylandFrameRect());
}

void ControlCenter::configureLayerShell()
{
#ifdef HAS_LAYER_SHELL
    if (!m_layerShellWindow)
        m_layerShellWindow = LayerShellQt::Window::get(m_window);

    if (!m_layerShellWindow) {
        qWarning() << "(ControlCenter LayerShell) failed to get layer window";
        return;
    }

    m_layerShellWindow->setScope(QStringLiteral("control-center"));
    m_layerShellWindow->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
    m_layerShellWindow->setAnchors(LayerShellQt::Window::Anchors(
        LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
        | LayerShellQt::Window::AnchorRight));
    m_layerShellWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    m_layerShellWindow->setExclusiveZone(-1);
    m_layerShellWindow->setMargins(QMargins(0, 0, -m_waylandSlideOffset, 0));
    m_layerShellWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    m_layerShellWindow->setCloseOnDismissed(false);
#endif
}

void ControlCenter::applyBlur()
{
    bernese::window_effects::SetBlurBehind(m_window);

    // Same as the old frontend. Its setWindowRadius(1) does nothing without
    // dxcb; the radius that works is the one DBlurEffectWidget sets.
    if (isWayland())
        bernese::window_effects::SetWindowRadius(m_window, WaylandWindowRadius);
}

void ControlCenter::onMouseButtonReleased(const QPoint &point, int flag)
{
    if (flag != ButtonLeft && flag != ButtonRight)
        return;

    if (!m_autoHide || !m_debugAutoHide)
        return;

    if (m_window->isVisible()
        && (bernese::window_utils::NativeWindowGeometry(m_window).contains(point) || checkOnBoard(point)))
        return;

    hide();
}

bool ControlCenter::checkOnBoard(const QPoint &point) const
{
    // A click on the on-screen keyboard is not a click outside.
    return bernese::window_utils::TopLevelWindowClassAt(point).contains("onboard");
}

void ControlCenter::onDelayKillerTimeout()
{
    const QGSettings gsettings("com.deepin.dde.control-center", "/com/deepin/dde/control-center/");
    if (gsettings.keys().contains("autoExit") && gsettings.get("auto-exit").toBool()) {
        if ((m_window && m_window->isVisible())
            || (m_settingsWindow && m_settingsWindow->isVisible())) {
            m_delayKillerTimer->start();
            return;
        }
        qWarning() << "Killer Timeout, now quiiting...";
        qApp->quit();
        return;
    }

#ifndef DCC_KEEP_SETTINGS_LIVE
    Q_EMIT releaseRequested();
#endif
}

void ControlCenter::finishHide()
{
    if (!m_window)
        return;

    m_window->hide();

    // Reset the margin, or the next show without a slide stays off screen.
    if (isWayland()) {
        m_waylandSlideOffset = 0;
        updateLayerShellMargins();
    }
}

void ControlCenter::updateBackgroundColor()
{
    QColor color(0, 0, 0, qRound(m_opacity * 255));
    bool compositing = true;

    // On X11 pick the mask like DBlurEffectWidget does.
    if (!isWayland()) {
        compositing = bernese::window_effects::HasCompositingManager();
        if (!compositing)
            color = QColor("#373F47");
        else if (!bernese::window_effects::IsBlurBehindSupported())
            color.setAlpha(UnblurredMaskAlpha);
    }

    if (compositing != m_compositing) {
        m_compositing = compositing;
        Q_EMIT pageAnimationDurationChanged();
    }

    if (color == m_backgroundColor)
        return;

    m_backgroundColor = color;
    Q_EMIT backgroundColorChanged();
}

} // namespace quick
} // namespace dcc
