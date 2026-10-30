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

#ifndef LEGACYWIDGETITEM_H
#define LEGACYWIDGETITEM_H

#include <QPointer>
#include <QQuickPaintedItem>
#include <QWidget>
#include <QtQml/qqmlregistration.h>

namespace dcc {
namespace quick {

// Shows a QWidget inside Qt Quick.
//
// Used for module pages that are not QML yet, and for the widgets that
// ModuleInterface and PluginInterface give us, so old plugins keep working.
//
// The widget becomes a hidden top-level window (WA_DontShowOnScreen). We
// draw it with QWidget::render() when it asks for a repaint, and send mouse,
// wheel and key events to its own QWindow. Qt then does hit testing, grabs,
// hover and focus for us. Only public API is used.
class LegacyWidgetItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LegacyWidget)

    Q_PROPERTY(QObject *widget READ widget WRITE setWidget NOTIFY widgetChanged)
    // False when the module hides the widget, e.g. with setModuleVisible.
    Q_PROPERTY(bool widgetVisible READ widgetVisible NOTIFY widgetVisibleChanged)

public:
    explicit LegacyWidgetItem(QQuickItem *parent = nullptr);
    ~LegacyWidgetItem() override;

    QObject *widget() const;
    void setWidget(QObject *widget);
    bool widgetVisible() const;

    void paint(QPainter *painter) override;

    // The item that shows this hidden window. Popups use it to find the
    // real parent window.
    static LegacyWidgetItem *hostOf(const QWindow *window);

Q_SIGNALS:
    void widgetChanged();
    void widgetVisibleChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void attach();
    void detach();
    void syncSize();
    void syncGlobalPosition();
    void updateImplicitSize();
    void scheduleRepaint();
    bool sendToWidget(QEvent *event);
    void forwardMouse(QMouseEvent *event, QEvent::Type type);
    void forwardKey(QKeyEvent *event);
    void syncFocusWidget(Qt::FocusReason reason);
    void syncCursor(const QPointF &pos);

    QPointer<QWidget> m_widget;
    QPointer<QWidget> m_focusWidget;
    QPointF m_lastMousePos;
    bool m_mouseDown;
    bool m_repaintPending;
};

} // namespace quick
} // namespace dcc

#endif // LEGACYWIDGETITEM_H
