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
#include "widgets/nextpagewidget.h"
#include "widgets/optionitem.h"
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

const QString ViewService = QStringLiteral("com.kylin.Wlcom");
const QString ViewPath = QStringLiteral("/com/kylin/Wlcom/View");
const QString ViewInterface = QStringLiteral("com.kylin.Wlcom.View");

const int MinimizeEffectScale = 0;
const int MinimizeEffectMagicLamp = 1;

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
        , m_viewIface(new QDBusInterface(
            ViewService, ViewPath, ViewInterface,
            QDBusConnection::sessionBus(), this))
        , m_scaleOption(new OptionItem(tr("Zoom"), false))
        , m_magicLampOption(new OptionItem(tr("Magic lamp"), false))
        , m_minBtnSwitch(new SwitchWidget(tr("Minimize button")))
        , m_maxBtnSwitch(new SwitchWidget(tr("Maximize button")))
        , m_closeBtnSwitch(new SwitchWidget(tr("Close button")))
        , m_forceRoundCornerSwitch(new SwitchWidget(tr("Force clipping of rounded corners (unstable)")))
        , m_excludeLayerShellSwitch(new SwitchWidget(
            tr("Exclude layer-shell surfaces (top bar, Dock, control center, etc.)"))) {
    auto *gtkGroup = new SettingsGroup(tr("GTK title bar buttons"));
    gtkGroup->appendItem(m_minBtnSwitch);
    gtkGroup->appendItem(m_maxBtnSwitch);
    gtkGroup->appendItem(m_closeBtnSwitch);

    auto *animationGroup = new SettingsGroup(tr("Minimize animation"));
    m_scaleOption->setContentsMargins(20, 0, 10, 0);
    m_magicLampOption->setContentsMargins(20, 0, 10, 0);
    animationGroup->appendItem(m_scaleOption);
    animationGroup->appendItem(m_magicLampOption);

    auto *effectGroup = new SettingsGroup(tr("Window effects"));
    auto *blurSettings = new NextPageWidget;
    blurSettings->setTitle(tr("Blur settings"));
    effectGroup->appendItem(blurSettings);

    auto *expGroup = new SettingsGroup(tr("Experimental features"));
    auto *warningItem = new WarningItem;
    warningItem->setText(tr("Do not touch the settings below unless you know what you are doing"));
    expGroup->appendItem(warningItem);
    expGroup->appendItem(m_forceRoundCornerSwitch);
    expGroup->appendItem(m_excludeLayerShellSwitch);

    auto *frame = new TranslucentFrame;
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(gtkGroup);
    layout->addWidget(animationGroup);
    layout->addWidget(effectGroup);
    layout->addWidget(expGroup);
    layout->addStretch();

    setTitle(tr("Window manager"));
    setContent(frame);

    connect(m_minBtnSwitch, &SwitchWidget::checkedChanged, this,
        &GxwmWidget::onGtkButtonsChanged);
    connect(m_maxBtnSwitch, &SwitchWidget::checkedChanged, this,
        &GxwmWidget::onGtkButtonsChanged);
    connect(m_closeBtnSwitch, &SwitchWidget::checkedChanged, this,
        &GxwmWidget::onGtkButtonsChanged);
    connect(m_scaleOption, &OptionItem::selectedChanged, this, [this] {
        onMinimizeEffectChanged(MinimizeEffectScale);
    });
    connect(m_magicLampOption, &OptionItem::selectedChanged, this, [this] {
        onMinimizeEffectChanged(MinimizeEffectMagicLamp);
    });
    connect(blurSettings, &NextPageWidget::clicked, this,
            &GxwmWidget::requestShowBlurSettings);
    connect(m_forceRoundCornerSwitch, &SwitchWidget::checkedChanged, this,
        &GxwmWidget::onForceRoundCornerChanged);
    connect(m_excludeLayerShellSwitch, &SwitchWidget::checkedChanged, this,
        &GxwmWidget::onExcludeLayerShellChanged);

    setMinimizeEffectSelection(MinimizeEffectScale);
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

    const QDBusMessage effectReply =
        call(m_viewIface, QStringLiteral("GetMinimizeEffect"));
    if (effectReply.type() == QDBusMessage::ReplyMessage &&
        !effectReply.arguments().isEmpty()) {
        setMinimizeEffectSelection(effectReply.arguments().first().toInt());
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

void GxwmWidget::setMinimizeEffectSelection(int effect) {
    const bool scale = effect == MinimizeEffectScale;
    m_scaleOption->blockSignals(true);
    m_magicLampOption->blockSignals(true);
    m_scaleOption->setSelected(scale);
    m_magicLampOption->setSelected(!scale);
    m_scaleOption->blockSignals(false);
    m_magicLampOption->blockSignals(false);
}

void GxwmWidget::onMinimizeEffectChanged(int effect) {
    setMinimizeEffectSelection(effect);

    const QDBusMessage reply =
        // 修复在 wayland 下切换最小化动画无效的问题
        call(m_viewIface, QStringLiteral("SetMinimizeEffect"),
             QVariantList() << QVariant::fromValue(static_cast<quint32>(effect)));

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
