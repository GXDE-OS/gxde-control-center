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

#include "legacywidgetitem.h"

#include <QApplication>
#include <QHash>
#include <QPainter>
#include <QQuickWindow>
#include <QWindow>

namespace dcc {
namespace quick {

namespace {

// Hidden host window -> the item that shows it.
QHash<const QWindow *, LegacyWidgetItem *> &hostedWindows()
{
    static QHash<const QWindow *, LegacyWidgetItem *> windows;
    return windows;
}

// Menus, combo popups and dialogs of hosted widgets get the hidden host
// window as transient parent. The window manager cannot place them by it,
// and on Wayland a popup needs a real parent surface. So before they show,
// use the QQuickWindow instead.
class PopupParentFixer : public QObject
{
public:
    static void install()
    {
        static PopupParentFixer *fixer = nullptr;
        if (!fixer) {
            fixer = new PopupParentFixer;
            qApp->installEventFilter(fixer);
        }
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::Show || !watched->isWidgetType())
            return false;

        QWidget *widget = static_cast<QWidget *>(watched);
        QWindow *window = widget->isWindow() ? widget->windowHandle() : nullptr;
        if (!window || !window->transientParent())
            return false;

        if (LegacyWidgetItem *host = LegacyWidgetItem::hostOf(window->transientParent())) {
            if (host->window())
                window->setTransientParent(host->window());
        }

        return false;
    }
};

} // namespace

LegacyWidgetItem::LegacyWidgetItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_mouseDown(false)
    , m_repaintPending(false)
{
    // Leave the back and forward buttons to QML.
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
    setAcceptHoverEvents(true);
    setActiveFocusOnTab(false);
    setFlag(ItemAcceptsInputMethod, false);

    PopupParentFixer::install();
}

LegacyWidgetItem::~LegacyWidgetItem()
{
    detach();
}

QObject *LegacyWidgetItem::widget() const
{
    return m_widget.data();
}

void LegacyWidgetItem::setWidget(QObject *widget)
{
    QWidget *w = qobject_cast<QWidget *>(widget);
    if (w == m_widget)
        return;

    detach();
    m_widget = w;
    attach();

    Q_EMIT widgetChanged();
    Q_EMIT widgetVisibleChanged();
}

bool LegacyWidgetItem::widgetVisible() const
{
    return m_widget && m_widget->isVisible();
}

LegacyWidgetItem *LegacyWidgetItem::hostOf(const QWindow *window)
{
    return hostedWindows().value(window);
}

void LegacyWidgetItem::attach()
{
    if (!m_widget)
        return;

    // Keep it hidden if the module hid it. The module shows it later.
    const bool explicitlyHidden = m_widget->testAttribute(Qt::WA_WState_ExplicitShowHide)
                                  && m_widget->isHidden();

    if (!m_widget->isWindow() || !m_widget->testAttribute(Qt::WA_DontShowOnScreen)) {
        m_widget->setParent(nullptr, Qt::Window | Qt::FramelessWindowHint);
        m_widget->setAttribute(Qt::WA_DontShowOnScreen);
    }

    // show() makes no QWindow for a hidden window, but events and popups need one.
    m_widget->winId();

    m_widget->installEventFilter(this);
    connect(m_widget, &QObject::destroyed, this, [this] {
        hostedWindows().remove(hostedWindows().key(this));
        Q_EMIT widgetChanged();
        Q_EMIT widgetVisibleChanged();
        update();
    });

    syncSize();
    if (!explicitlyHidden)
        m_widget->show();

    if (QWindow *handle = m_widget->windowHandle())
        hostedWindows().insert(handle, this);

    updateImplicitSize();
    update();
}

void LegacyWidgetItem::detach()
{
    if (!m_widget)
        return;

    hostedWindows().remove(m_widget->windowHandle());
    m_widget->removeEventFilter(this);
    disconnect(m_widget, nullptr, this, nullptr);
    m_widget = nullptr;
    m_focusWidget = nullptr;
}

void LegacyWidgetItem::paint(QPainter *painter)
{
    m_repaintPending = false;

    if (!m_widget || !m_widget->isVisible())
        return;

    // Skip the window background, so the QML background shows through.
    m_widget->render(painter, QPoint(), QRegion(), QWidget::DrawChildren);
}

bool LegacyWidgetItem::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_widget)
        return false;

    switch (event->type()) {
    case QEvent::UpdateRequest:
    case QEvent::UpdateLater:
        scheduleRepaint();
        break;
    case QEvent::Resize:
    case QEvent::LayoutRequest:
        updateImplicitSize();
        scheduleRepaint();
        break;
    case QEvent::Show:
    case QEvent::Hide:
        // The host window exists after the first show().
        if (QWindow *handle = m_widget->windowHandle())
            hostedWindows().insert(handle, this);
        Q_EMIT widgetVisibleChanged();
        scheduleRepaint();
        break;
    default:
        break;
    }

    return false;
}

void LegacyWidgetItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);

    if (newGeometry.size() != oldGeometry.size())
        syncSize();
}

void LegacyWidgetItem::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickPaintedItem::itemChange(change, value);

    if (change == ItemSceneChange && value.window && m_widget)
        m_widget->setScreen(value.window->screen());
}

void LegacyWidgetItem::syncSize()
{
    if (!m_widget)
        return;

    const QSize size(qRound(width()), qRound(height()));
    if (size.isEmpty()) {
        // Before layout, use the size hint so QML gets an implicit size.
        m_widget->adjustSize();
        return;
    }

    // Widgets with a fixed height, like ModuleWidget, ignore this height.
    m_widget->resize(size);
}

void LegacyWidgetItem::syncGlobalPosition()
{
    if (!m_widget || !window())
        return;

    // Popups use mapToGlobal(), so move the host window with the item.
    const QPoint global = mapToGlobal(QPointF(0, 0)).toPoint();
    if (m_widget->pos() != global)
        m_widget->move(global);
}

void LegacyWidgetItem::updateImplicitSize()
{
    if (!m_widget)
        return;

    const QSize hint = m_widget->sizeHint().expandedTo(m_widget->minimumSize());
    setImplicitWidth(hint.width());

    if (m_widget->minimumHeight() == m_widget->maximumHeight())
        setImplicitHeight(m_widget->minimumHeight());
    else if (m_widget->hasHeightForWidth() && width() > 0)
        setImplicitHeight(m_widget->heightForWidth(qRound(width())));
    else
        setImplicitHeight(hint.height());
}

void LegacyWidgetItem::scheduleRepaint()
{
    if (m_repaintPending)
        return;

    m_repaintPending = true;
    update();
}

bool LegacyWidgetItem::sendToWidget(QEvent *event)
{
    QWindow *handle = m_widget ? m_widget->windowHandle() : nullptr;
    if (!handle)
        return false;

    QCoreApplication::sendEvent(handle, event);
    return event->isAccepted();
}

void LegacyWidgetItem::forwardMouse(QMouseEvent *event, QEvent::Type type)
{
    // Item and host window use the same coordinates.
    const QPointF pos = event->position();
    QMouseEvent copy(type, pos, pos, event->globalPosition(), event->button(),
                     event->buttons(), event->modifiers(), event->pointingDevice());
    copy.setTimestamp(event->timestamp());
    sendToWidget(&copy);

    m_lastMousePos = pos;
}

void LegacyWidgetItem::mousePressEvent(QMouseEvent *event)
{
    if (!m_widget) {
        event->ignore();
        return;
    }

    syncGlobalPosition();
    m_mouseDown = true;
    forwardMouse(event, QEvent::MouseButtonPress);
    syncFocusWidget(Qt::MouseFocusReason);

    // Always accept, to get the move and release events.
    event->accept();
}

void LegacyWidgetItem::mouseMoveEvent(QMouseEvent *event)
{
    forwardMouse(event, QEvent::MouseMove);
    syncCursor(event->position());
}

void LegacyWidgetItem::mouseReleaseEvent(QMouseEvent *event)
{
    m_mouseDown = false;
    forwardMouse(event, QEvent::MouseButtonRelease);
    syncFocusWidget(Qt::MouseFocusReason);
}

void LegacyWidgetItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    forwardMouse(event, QEvent::MouseButtonDblClick);
}

void LegacyWidgetItem::mouseUngrabEvent()
{
    if (!m_mouseDown)
        return;

    // A Flickable took the mouse. Send a release so the widget is not stuck pressed.
    m_mouseDown = false;
    const QPointF global = mapToGlobal(m_lastMousePos);
    QMouseEvent release(QEvent::MouseButtonRelease, m_lastMousePos, m_lastMousePos, global,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    sendToWidget(&release);
}

void LegacyWidgetItem::hoverEnterEvent(QHoverEvent *event)
{
    syncGlobalPosition();

    const QPointF pos = event->position();
    QEnterEvent enter(pos, pos, mapToGlobal(pos));
    sendToWidget(&enter);
    hoverMoveEvent(event);
}

void LegacyWidgetItem::hoverMoveEvent(QHoverEvent *event)
{
    const QPointF pos = event->position();
    QMouseEvent move(QEvent::MouseMove, pos, pos, mapToGlobal(pos), Qt::NoButton,
                     Qt::NoButton, event->modifiers());
    sendToWidget(&move);
    syncCursor(pos);
}

void LegacyWidgetItem::hoverLeaveEvent(QHoverEvent *event)
{
    Q_UNUSED(event)

    QEvent leave(QEvent::Leave);
    sendToWidget(&leave);
    unsetCursor();
}

void LegacyWidgetItem::wheelEvent(QWheelEvent *event)
{
    syncGlobalPosition();

    if (!m_widget) {
        event->ignore();
        return;
    }

    // Send to the widget under the mouse. QApplication passes it up to the
    // parents and keeps the accepted state. Through QWidgetWindow we lose it.
    const QPointF pos = event->position();
    QWidget *target = m_widget->childAt(pos.toPoint());
    if (!target)
        target = m_widget;

    const QPointF local = target->mapFrom(m_widget, pos);
    QWheelEvent copy(local, event->globalPosition(), event->pixelDelta(), event->angleDelta(),
                     event->buttons(), event->modifiers(), event->phase(), event->inverted(),
                     event->source(), event->pointingDevice());
    copy.setTimestamp(event->timestamp());
    QCoreApplication::sendEvent(target, &copy);

    // Give unused wheel events back to the Flickable.
    event->setAccepted(copy.isAccepted());
}

void LegacyWidgetItem::keyPressEvent(QKeyEvent *event)
{
    forwardKey(event);
    syncFocusWidget(Qt::TabFocusReason);
}

void LegacyWidgetItem::keyReleaseEvent(QKeyEvent *event)
{
    forwardKey(event);
}

void LegacyWidgetItem::forwardKey(QKeyEvent *event)
{
    QKeyEvent copy(event->type(), event->key(), event->modifiers(), event->nativeScanCode(),
                   event->nativeVirtualKey(), event->nativeModifiers(), event->text(),
                   event->isAutoRepeat(), event->count(), event->device());
    copy.setTimestamp(event->timestamp());
    copy.setAccepted(false);

    // Give unused keys, like Esc, back to QML.
    event->setAccepted(sendToWidget(&copy));
}

void LegacyWidgetItem::focusInEvent(QFocusEvent *event)
{
    QQuickPaintedItem::focusInEvent(event);
    syncFocusWidget(event->reason());
}

void LegacyWidgetItem::focusOutEvent(QFocusEvent *event)
{
    QQuickPaintedItem::focusOutEvent(event);

    if (m_focusWidget) {
        QFocusEvent out(QEvent::FocusOut, event->reason());
        QCoreApplication::sendEvent(m_focusWidget, &out);
        m_focusWidget->update();
    }
    m_focusWidget = nullptr;
}

void LegacyWidgetItem::syncFocusWidget(Qt::FocusReason reason)
{
    // The host window is never active, so QWidget::setFocus() sends no focus
    // events. Send FocusIn and FocusOut here, or line edits show no cursor.
    QWidget *current = m_widget ? m_widget->focusWidget() : nullptr;
    if (current && current->focusPolicy() == Qt::NoFocus)
        current = nullptr;

    if (current && !hasActiveFocus())
        forceActiveFocus(reason);

    if (!hasActiveFocus())
        current = nullptr;

    if (current == m_focusWidget)
        return;

    if (m_focusWidget) {
        QFocusEvent out(QEvent::FocusOut, reason);
        QCoreApplication::sendEvent(m_focusWidget, &out);
        m_focusWidget->update();
    }

    m_focusWidget = current;

    if (m_focusWidget) {
        QFocusEvent in(QEvent::FocusIn, reason);
        QCoreApplication::sendEvent(m_focusWidget, &in);
        m_focusWidget->update();
    }
}

void LegacyWidgetItem::syncCursor(const QPointF &pos)
{
    if (!m_widget)
        return;

    // The host window is hidden, so the item shows the widget cursor.
    QWidget *target = m_widget->childAt(pos.toPoint());
    while (target && target != m_widget && !target->testAttribute(Qt::WA_SetCursor))
        target = target->parentWidget();

    if (target && target->testAttribute(Qt::WA_SetCursor))
        setCursor(target->cursor());
    else
        unsetCursor();
}

} // namespace quick
} // namespace dcc
