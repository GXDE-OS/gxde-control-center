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

#include "timeoutdialog.h"
#include "wayland/waylandhelper.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

TimeoutDialog::TimeoutDialog(const int timeout, QString messageModel, QWidget *parent)
    : DDialog(parent)
    , m_timeoutRefreshTimer(new QTimer(this))
    , m_timeout(timeout)
    , m_messageModel(messageModel)
{
    // set default title, message icon
    setTitle(tr("Do you want to save the display settings?"));
    if (messageModel.isEmpty()) {
        m_messageModel = tr("If no operation, the display will be restored after %1s.");
    }
    setMessage(m_messageModel.arg(m_timeout));
    setIcon(QIcon(":/widgets/themes/dark/icons/display_setting.svg"), QSize(64, 64));

    addButton(tr("Restore"), true, DDialog::ButtonRecommend);
    addButton(tr("Save"));

    m_timeoutRefreshTimer->setInterval(1000);

    connect(m_timeoutRefreshTimer, &QTimer::timeout, this, &TimeoutDialog::onRefreshTimeout);

    setWindowFlags(windowFlags() | Qt::X11BypassWindowManagerHint);
}

int TimeoutDialog::exec()
{
    m_timeoutRefreshTimer->start();

#ifdef HAS_LAYER_SHELL
    configureLayerShell();
#endif

    return DDialog::exec();
}

void TimeoutDialog::open()
{
    if (!isVisible()) {
        m_timeoutRefreshTimer->start();
    }

#ifdef HAS_LAYER_SHELL
    configureLayerShell();
#endif

    DDialog::open();
}

#ifdef HAS_LAYER_SHELL
void TimeoutDialog::configureLayerShell() {
    if (!Wayland::isWaylandSession()) {
        return;
    }

    adjustSize();
    setAttribute(Qt::WA_NativeWindow, true);
    createWinId();

    QWindow* window = windowHandle();
    if (!window) {
        return;
    }

    if (QGuiApplication::primaryScreen()) {
        window->setScreen(QGuiApplication::primaryScreen());
    }

    if (!m_layerShellWindow) {
        m_layerShellWindow = LayerShellQt::Window::get(window);
    }

    if (!m_layerShellWindow) {
        return;
    }

    m_layerShellWindow->setScope(
        QStringLiteral("control-center-display-confirmation"));
    m_layerShellWindow->setScreenConfiguration(
        LayerShellQt::Window::ScreenFromQWindow);
    m_layerShellWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    m_layerShellWindow->setAnchors(LayerShellQt::Window::Anchors(
        LayerShellQt::Window::AnchorTop
        | LayerShellQt::Window::AnchorLeft));
    m_layerShellWindow->setExclusiveZone(-1);
    m_layerShellWindow->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityOnDemand);
    m_layerShellWindow->setCloseOnDismissed(false);

    const QRect screenRect = window->screen()->geometry();
    const QPoint centered(
        qMax(0, (screenRect.width() - width()) / 2),
        qMax(0, (screenRect.height() - height()) / 2));
    setLayerShellPosition(centered);
}

void TimeoutDialog::setLayerShellPosition(const QPoint& position) {
    if (!m_layerShellWindow || !windowHandle() || !windowHandle()->screen()) {
        return;
    }

    const QSize screenSize = windowHandle()->screen()->geometry().size();
    const QPoint boundedPosition(
        qBound(0, position.x(), qMax(0, screenSize.width() - width())),
        qBound(0, position.y(), qMax(0, screenSize.height() - height())));
    m_layerShellWindow->setMargins(QMargins(
        boundedPosition.x(), boundedPosition.y(), 0, 0));
}
#endif

void TimeoutDialog::onRefreshTimeout()
{
    m_timeout--;
    setMessage(m_messageModel.arg(m_timeout));

    if (m_timeout < 1) {
        reject();
    }
}

QString TimeoutDialog::messageModel() const
{
    return m_messageModel;
}

void TimeoutDialog::setMessageModel(const QString &messageModel)
{
    m_messageModel = messageModel;
    setMessage(m_messageModel.arg(m_timeout));
}
