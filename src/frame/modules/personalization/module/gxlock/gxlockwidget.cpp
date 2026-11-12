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

#include "gxlockwidget.h"

#include "widgets/filechoosewidget.h"
#include "widgets/nextpagewidget.h"
#include "widgets/optionitem.h"
#include "widgets/settingsgroup.h"
#include "widgets/translucentframe.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QLineEdit>
#include <QStandardPaths>
#include <QVBoxLayout>

using namespace dcc;
using namespace dcc::widgets;
using namespace dcc::personalization;

namespace {

const QString LockService = QStringLiteral("top.gxde.Lock");
const QString LockPath = QStringLiteral("/top/gxde/Lock");
const QString LockInterface = QStringLiteral("top.gxde.Lock");

const QString ClassicStyle = QStringLiteral("classic");
const QString ModernStyle = QStringLiteral("modern");

QDBusMessage call(QDBusInterface *iface, const QString &method,
        const QVariantList &args = QVariantList()) {
    if (!iface || !iface->isValid()) {
        return QDBusMessage::createError(QDBusError::UnknownObject,
            QStringLiteral("invalid interface"));
    }
    return iface->callWithArgumentList(QDBus::Block, method, args);
}

// gxde-lock setters reply with a bool.
bool callOk(QDBusInterface *iface, const QString &method,
        const QVariantList &args = QVariantList()) {
    const QDBusMessage reply = call(iface, method, args);
    return reply.type() == QDBusMessage::ReplyMessage
        && !reply.arguments().isEmpty()
        && reply.arguments().first().toBool();
}

QString callString(QDBusInterface *iface, const QString &method) {
    const QDBusMessage reply = call(iface, method);
    if (reply.type() != QDBusMessage::ReplyMessage
        || reply.arguments().isEmpty()) {
        return QString();
    }
    return reply.arguments().first().toString();
}

}  // namespace

bool GxlockWidget::isAvailable()
{
    // Do not call it here: activation would start the locker.
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (!bus) {
        return false;
    }
    return bus->isServiceRegistered(LockService).value()
        || bus->activatableServiceNames().value().contains(LockService);
}

QDBusInterface *GxlockWidget::createInterface(QObject *parent)
{
    return new QDBusInterface(LockService, LockPath, LockInterface,
        QDBusConnection::sessionBus(), parent);
}

GxlockWidget::GxlockWidget(QWidget *parent)
        : ContentWidget(parent)
        , m_lockIface(createInterface(this))
        , m_classicStyle(new OptionItem)
        , m_modernStyle(new OptionItem)
        , m_userWallpaper(new FileChooseWidget) {
    auto *styleGroup = new SettingsGroup(tr("Lock screen style"));
    m_classicStyle->setTitle(tr("Classic"));
    m_classicStyle->setContentsMargins(20, 0, 10, 0);
    styleGroup->appendItem(m_classicStyle);
    m_modernStyle->setTitle(tr("Modern"));
    m_modernStyle->setContentsMargins(20, 0, 10, 0);
    styleGroup->appendItem(m_modernStyle);
    // gxde-lock refuses a style whose binary is not installed.
    if (QStandardPaths::findExecutable(QStringLiteral("gxde-lock-neo"))
            .isEmpty()) {
        m_modernStyle->setVisible(false);
    }

    auto *userGroup =
        new SettingsGroup(tr("Lock screen wallpaper (current user)"));
    m_userWallpaper->setTitle(tr("Choose lock screen wallpaper"));
    m_userWallpaper->setType(tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    userGroup->appendItem(m_userWallpaper);
    auto *userDefault = new NextPageWidget;
    userDefault->setTitle(tr("Restore default lock screen wallpaper"));
    userGroup->appendItem(userDefault);

    auto *globalGroup = new SettingsGroup(tr("Lock screen wallpaper (global)"));
    auto *globalWallpaper = new FileChooseWidget;
    globalWallpaper->setTitle(tr("Change wallpaper"));
    globalWallpaper->setType(tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    globalGroup->appendItem(globalWallpaper);
    auto *globalDefault = new NextPageWidget;
    globalDefault->setTitle(tr("Restore default wallpaper"));
    globalGroup->appendItem(globalDefault);

    auto *cursorGroup =
        new SettingsGroup(tr("Lock screen mouse cursor (global)"));
    auto *cursorThemes = new NextPageWidget;
    cursorThemes->setTitle(tr("Select mouse cursor"));
    cursorGroup->appendItem(cursorThemes);

    auto *frame = new TranslucentFrame;
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(styleGroup);
    layout->addWidget(userGroup);
    layout->addWidget(globalGroup);
    layout->addWidget(cursorGroup);
    layout->addStretch();

    setTitle(tr("Lock screen"));
    setContent(frame);

    connect(m_classicStyle, &OptionItem::selectedChanged, this,
        [this] { setStyle(ClassicStyle); });
    connect(m_modernStyle, &OptionItem::selectedChanged, this,
        [this] { setStyle(ModernStyle); });
    QDBusConnection::sessionBus().connect(LockService, LockPath,
        LockInterface, QStringLiteral("LockScreenStyleChanged"), this,
        SLOT(onStyleChanged(QString)));

    connect(m_userWallpaper->edit(), &QLineEdit::textChanged, this,
        [this](const QString &path) {
            if (!path.isEmpty()) {
                callOk(m_lockIface,
                    QStringLiteral("SetLockWallpaperOverride"), {path});
            }
        });
    connect(userDefault, &NextPageWidget::clicked, this, [this] {
        callOk(m_lockIface, QStringLiteral("ClearLockWallpaperOverride"));
        refresh();
    });

    connect(globalWallpaper->edit(), &QLineEdit::textChanged, this,
        [this](const QString &path) {
            if (!path.isEmpty()) {
                callOk(m_lockIface, QStringLiteral("SetWallpaper"), {path});
            }
        });
    connect(globalDefault, &NextPageWidget::clicked, this, [this] {
        // No global wallpaper: each user falls back to their own.
        callOk(m_lockIface, QStringLiteral("ClearWallpaper"));
    });

    connect(cursorThemes, &NextPageWidget::clicked, this,
        &GxlockWidget::requestShowCursorThemes);

    connect(m_userWallpaper, &FileChooseWidget::requestFrameKeepAutoHide,
        this, &GxlockWidget::requestFrameKeepAutoHide);
    connect(globalWallpaper, &FileChooseWidget::requestFrameKeepAutoHide,
        this, &GxlockWidget::requestFrameKeepAutoHide);

    refresh();
}

void GxlockWidget::onStyleChanged(const QString &style) {
    m_classicStyle->setSelected(style != ModernStyle);
    m_modernStyle->setSelected(style == ModernStyle);
}

void GxlockWidget::setStyle(const QString &style) {
    if (!callOk(m_lockIface, QStringLiteral("SetLockScreenStyle"), {style})) {
        refresh();
        return;
    }
    onStyleChanged(style);
}

void GxlockWidget::refresh() {
    onStyleChanged(callString(m_lockIface, QStringLiteral("LockScreenStyle")));

    // Empty when the user has no wallpaper of their own.
    const QSignalBlocker blocker(m_userWallpaper->edit());
    m_userWallpaper->edit()->setText(
        callString(m_lockIface, QStringLiteral("LockWallpaperOverride")));
}
