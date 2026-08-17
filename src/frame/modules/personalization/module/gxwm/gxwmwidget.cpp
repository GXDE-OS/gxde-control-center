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

#include "gxwmwidget.h"

#include "widgets/labels/tipslabel.h"
#include "widgets/settingsgroup.h"
#include "widgets/settingsitem.h"
#include "widgets/switchwidget.h"
#include "widgets/translucentframe.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QHBoxLayout>
#include <QVBoxLayout>

using namespace dcc;
using namespace dcc::widgets;
using namespace dcc::personalization;

namespace {

const QString WindowBtnService = QStringLiteral("top.gxde.Wlcom.WindowBtn");
const QString WindowBtnPath = QStringLiteral("/top/gxde/Wlcom/WindowBtn");
const QString WindowBtnInterface = QStringLiteral("top.gxde.Wlcom.WindowBtn");

const QString WindowCornerService = QStringLiteral("top.gxde.Wlcom.WindowCorner");
const QString WindowCornerPath = QStringLiteral("/top/gxde/Wlcom/WindowCorner");
const QString WindowCornerInterface = QStringLiteral("top.gxde.Wlcom.WindowCorner");

QDBusMessage call(QDBusInterface *iface, const QString &method,
        const QVariantList &args = QVariantList()) {
    if (!iface || !iface->isValid()) {
        return QDBusMessage::createError(QDBusError::UnknownObject,
                QStringLiteral("invalid interface"));
    }
    return iface->callWithArgumentList(QDBus::Block, method, args);
}

class WarningItem : public SettingsItem {
public:
    explicit WarningItem(QWidget *parent = nullptr)
            : SettingsItem(parent)
            , m_label(new TipsLabel) {
        m_label->setWordWrap(true);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->addWidget(m_label);
    }

    void setText(const QString &text) { m_label->setText(text); }

private:
    TipsLabel *m_label;
};

}  // namespace

GxwmWidget::GxwmWidget(QWidget *parent)
        : ContentWidget(parent)
        , m_windowBtnIface(new QDBusInterface(
            WindowBtnService, WindowBtnPath, WindowBtnInterface,
            QDBusConnection::sessionBus(), this))
        , m_windowCornerIface(new QDBusInterface(
            WindowCornerService, WindowCornerPath, WindowCornerInterface,
            QDBusConnection::sessionBus(), this))
        , m_minBtnSwitch(new SwitchWidget(tr("最小化按钮")))
        , m_maxBtnSwitch(new SwitchWidget(tr("最大化按钮")))
        , m_closeBtnSwitch(new SwitchWidget(tr("关闭按钮")))
        , m_forceRoundCornerSwitch(new SwitchWidget(tr("强制裁剪圆角（不稳定）")))
        , m_excludeLayerShellSwitch(new SwitchWidget(
            tr("排除 layer-shell 表面（顶栏、Dock、控制中心等）"))) {
    auto *gtkGroup = new SettingsGroup(tr("GTK标题栏按钮"));
    gtkGroup->appendItem(m_minBtnSwitch);
    gtkGroup->appendItem(m_maxBtnSwitch);
    gtkGroup->appendItem(m_closeBtnSwitch);

    auto *expGroup = new SettingsGroup(tr("实验性功能"));
    auto *warningItem = new WarningItem;
    warningItem->setText(tr("除非您知道自己在做什么，否则不要操作以下设置"));
    expGroup->appendItem(warningItem);
    expGroup->appendItem(m_forceRoundCornerSwitch);
    expGroup->appendItem(m_excludeLayerShellSwitch);

    auto *frame = new TranslucentFrame;
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(gtkGroup);
    layout->addWidget(expGroup);
    layout->addStretch();

    setTitle(tr("窗口管理器"));
    setContent(frame);

    connect(m_minBtnSwitch, &SwitchWidget::checkedChanged, this,
        &GxwmWidget::onGtkButtonsChanged);
    connect(m_maxBtnSwitch, &SwitchWidget::checkedChanged, this,
        &GxwmWidget::onGtkButtonsChanged);
    connect(m_closeBtnSwitch, &SwitchWidget::checkedChanged, this,
        &GxwmWidget::onGtkButtonsChanged);
    connect(m_forceRoundCornerSwitch, &SwitchWidget::checkedChanged, this,
        &GxwmWidget::onForceRoundCornerChanged);
    connect(m_excludeLayerShellSwitch, &SwitchWidget::checkedChanged, this,
        &GxwmWidget::onExcludeLayerShellChanged);

    refresh();
}

void GxwmWidget::refresh() {
    const QSignalBlocker minBlocker(m_minBtnSwitch);
    const QSignalBlocker maxBlocker(m_maxBtnSwitch);
    const QSignalBlocker closeBlocker(m_closeBtnSwitch);
    const QSignalBlocker forceBlocker(m_forceRoundCornerSwitch);
    const QSignalBlocker excludeBlocker(m_excludeLayerShellSwitch);

    const QDBusMessage btnReply =
        call(m_windowBtnIface, QStringLiteral("GetGtkDecorationButtons"));
    if (btnReply.type() == QDBusMessage::ReplyMessage &&
        btnReply.arguments().size() == 3) {
        const QVariantList args = btnReply.arguments();
        m_minBtnSwitch->setChecked(args.at(0).toBool());
        m_maxBtnSwitch->setChecked(args.at(1).toBool());
        m_closeBtnSwitch->setChecked(args.at(2).toBool());
    }

    const QDBusMessage forceReply =
        call(m_windowCornerIface, QStringLiteral("GetForceRoundCorner"));
    if (forceReply.type() == QDBusMessage::ReplyMessage &&
        !forceReply.arguments().isEmpty()) {
        const bool force = forceReply.arguments().first().toBool();
        m_forceRoundCornerSwitch->setChecked(force);
        m_excludeLayerShellSwitch->setEnabled(force);
    }

    const QDBusMessage excludeReply = call(
        m_windowCornerIface,
        QStringLiteral("GetForceRoundCornerExcludeLayerShell"));
    if (excludeReply.type() == QDBusMessage::ReplyMessage &&
        !excludeReply.arguments().isEmpty()) {
        m_excludeLayerShellSwitch->setChecked(
            excludeReply.arguments().first().toBool());
    }
}

void GxwmWidget::onGtkButtonsChanged() {
    const QDBusMessage reply =
        call(m_windowBtnIface, QStringLiteral("SetGtkDecorationButtons"),
             QVariantList() << m_minBtnSwitch->checked()
                << m_maxBtnSwitch->checked()
                << m_closeBtnSwitch->checked());

    if (reply.type() != QDBusMessage::ReplyMessage) {
        refresh();
    }
}

void GxwmWidget::onForceRoundCornerChanged(bool enabled) {
    const QDBusMessage reply =
        call(m_windowCornerIface, QStringLiteral("SetForceRoundCorner"),
             QVariantList() << enabled);

    if (reply.type() != QDBusMessage::ReplyMessage) {
        refresh();
        return;
    }

    m_excludeLayerShellSwitch->setEnabled(enabled);
}

void GxwmWidget::onExcludeLayerShellChanged(bool enabled) {
    const QDBusMessage reply = call(
        m_windowCornerIface,
        QStringLiteral("SetForceRoundCornerExcludeLayerShell"),
        QVariantList() << enabled);

    if (reply.type() != QDBusMessage::ReplyMessage) {
        refresh();
    }
}
