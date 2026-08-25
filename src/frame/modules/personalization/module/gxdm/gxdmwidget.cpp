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

#include "gxdmwidget.h"

#include "../themewidget/theme.h"
#include "widgets/filechoosewidget.h"
#include "widgets/labels/tipslabel.h"
#include "widgets/nextpagewidget.h"
#include "widgets/settingsgroup.h"
#include "widgets/settingsitem.h"
#include "widgets/switchwidget.h"
#include "widgets/translucentframe.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QVBoxLayout>

using namespace dcc;
using namespace dcc::widgets;
using namespace dcc::personalization;

namespace {

const QString DisplayManagerService = QStringLiteral("top.gxde.DisplayManager");
const QString DisplayManagerPath = QStringLiteral("/top/gxde/DisplayManager");
const QString DisplayManagerInterface = QStringLiteral("top.gxde.DisplayManager");

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

bool GxdmWidget::isAvailable()
{
    const QDBusMessage reply = QDBusMessage::createMethodCall(
        DisplayManagerService, DisplayManagerPath, DisplayManagerInterface,
        QStringLiteral("LkScrStat"));
    return QDBusConnection::sessionBus().call(reply, QDBus::Block).type()
            == QDBusMessage::ReplyMessage;
}

GxdmWidget::GxdmWidget(QWidget *parent)
        : ContentWidget(parent)
        , m_displayManagerIface(new QDBusInterface(
            DisplayManagerService, DisplayManagerPath, DisplayManagerInterface,
            QDBusConnection::sessionBus(), this))
        , m_x11GreeterSwitch(new SwitchWidget(tr(
            "Switch to the X11 welcome screen frontend (restart required)")))
        , m_lockWallpaperChooser(nullptr) {
    auto *loginGroup = new SettingsGroup(tr("Welcome screen wallpaper (global)"));

    auto *wallpaperChooser = new FileChooseWidget;
    wallpaperChooser->setTitle(tr("Change wallpaper"));
    wallpaperChooser->setType(tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    loginGroup->appendItem(wallpaperChooser);

    NextPageWidget *defaultWallpaper = new NextPageWidget;
    defaultWallpaper->setTitle(tr("Restore default wallpaper"));
    loginGroup->appendItem(defaultWallpaper);

    // 欢迎界面鼠标指针放入二级菜单
    auto *cursorGroup =
        new SettingsGroup(tr("Welcome screen mouse cursor (global)"));
    NextPageWidget *cursorThemes = new NextPageWidget;
    cursorThemes->setTitle(tr("Select mouse cursor"));
    cursorGroup->appendItem(cursorThemes);

    auto *lockGroup = new SettingsGroup(tr("Lock screen manager"));

    m_lockWallpaperChooser = new FileChooseWidget;
    m_lockWallpaperChooser->setTitle(tr("Choose lock screen wallpaper"));
    m_lockWallpaperChooser->setType(tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    lockGroup->appendItem(m_lockWallpaperChooser);

    NextPageWidget *defaultLockWallpaper = new NextPageWidget;
    defaultLockWallpaper->setTitle(tr("Restore default lock screen wallpaper"));
    lockGroup->appendItem(defaultLockWallpaper);

    auto *expGroup = new SettingsGroup(tr("Experimental features"));
    auto *warningItem = new WarningItem;
    warningItem->setText(tr("Do not touch the settings below unless you know what you are doing"));
    expGroup->appendItem(warningItem);
    expGroup->appendItem(m_x11GreeterSwitch);

    auto *frame = new TranslucentFrame;
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(loginGroup);
    layout->addWidget(cursorGroup);
    layout->addWidget(lockGroup);
    layout->addWidget(expGroup);
    layout->addStretch();

    setTitle(tr("Display manager"));
    setContent(frame);

    connect(wallpaperChooser->edit(), &QLineEdit::textChanged, this,
        &GxdmWidget::onWallpaperChanged);
    connect(defaultWallpaper, &NextPageWidget::clicked, this, [this] {
        // 清空自定义壁纸，回到默认行为：
        // 单用户跟随其锁屏壁纸，多用户使用默认登录壁纸
        call(m_displayManagerIface, QStringLiteral("ClearWallpaper"));
    });
    connect(cursorThemes, &NextPageWidget::clicked, this,
        &GxdmWidget::requestShowCursorThemes);
    connect(m_lockWallpaperChooser->edit(), &QLineEdit::textChanged, this,
        &GxdmWidget::onLockWallpaperChanged);
    connect(defaultLockWallpaper, &NextPageWidget::clicked, this, [this] {
        const QDBusMessage reply = call(m_displayManagerIface,
            QStringLiteral("ClearLockWallpaperOverride"));
        if (reply.type() == QDBusMessage::ReplyMessage &&
            m_lockWallpaperChooser) {
            const QSignalBlocker blocker(m_lockWallpaperChooser->edit());
            m_lockWallpaperChooser->edit()->clear();
        } else {
            refresh();
        }
    });
    connect(m_x11GreeterSwitch, &SwitchWidget::checkedChanged, this,
        &GxdmWidget::onGreeterServerChanged);

    connect(wallpaperChooser, &FileChooseWidget::requestFrameKeepAutoHide, this,
        &GxdmWidget::requestFrameKeepAutoHide);
    connect(m_lockWallpaperChooser, &FileChooseWidget::requestFrameKeepAutoHide,
        this, &GxdmWidget::requestFrameKeepAutoHide);

    refresh();
}

GxdmCursorThemeWidget::GxdmCursorThemeWidget(ThemeModel *cursorModel,
        QWidget *parent)
        : ContentWidget(parent)
        , m_displayManagerIface(new QDBusInterface(
            DisplayManagerService, DisplayManagerPath, DisplayManagerInterface,
            QDBusConnection::sessionBus(), this))
{
    auto *cursorTheme = new Theme(tr("Welcome screen mouse cursor (global)"));
    if (cursorModel) {
        cursorTheme->setModel(cursorModel);
    }

    auto *frame = new TranslucentFrame;
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(cursorTheme);
    layout->addStretch();

    setTitle(tr("Welcome screen mouse cursor (global)"));
    setContent(frame);

    connect(cursorTheme, &Theme::requestSetDefault, this,
        [this, cursorTheme](const QJsonObject &value) {
            const QDBusMessage reply = call(m_displayManagerIface,
                QStringLiteral("SetCursor"),
                QVariantList() << value[QStringLiteral("Id")].toString());
            if (reply.type() == QDBusMessage::ReplyMessage) {
                cursorTheme->setDefault(value[QStringLiteral("Id")].toString());
            }
        });
}

void GxdmWidget::refresh() {
    const QSignalBlocker blocker(m_x11GreeterSwitch);

    const QDBusMessage reply =
        call(m_displayManagerIface, QStringLiteral("GreeterDisplayServer"));
    if (reply.type() == QDBusMessage::ReplyMessage &&
        !reply.arguments().isEmpty()) {
        m_x11GreeterSwitch->setChecked(
            reply.arguments().first().toString() == QStringLiteral("x11"));
    }

    // Show the current lock-screen wallpaper override (e.g. set through the
    // file manager context menu or the wallpaper chooser) instead of leaving
    // the chooser empty.
    if (m_lockWallpaperChooser) {
        const QSignalBlocker lockBlocker(m_lockWallpaperChooser->edit());
        const QDBusMessage lockReply = call(m_displayManagerIface,
            QStringLiteral("LockWallpaperOverride"));
        if (lockReply.type() == QDBusMessage::ReplyMessage &&
            !lockReply.arguments().isEmpty()) {
            m_lockWallpaperChooser->edit()->setText(
                lockReply.arguments().first().toString());
        } else {
            m_lockWallpaperChooser->edit()->clear();
        }
    }
}

void GxdmWidget::onGreeterServerChanged(bool enabled) {
    const QDBusMessage reply =
        call(m_displayManagerIface, QStringLiteral("SetGreeterDisplayServer"),
            QVariantList() << (enabled ? QStringLiteral("x11")
                : QStringLiteral("wayland")));

    if (reply.type() != QDBusMessage::ReplyMessage) {
        refresh();
    }
}

void GxdmWidget::onWallpaperChanged(const QString &path) {
    if (path.isEmpty()) {
        return;
    }
    call(m_displayManagerIface, QStringLiteral("SetWallpaper"),
        QVariantList() << path);
}

void GxdmWidget::onLockWallpaperChanged(const QString &path) {
    if (path.isEmpty()) {
        return;
    }
    call(m_displayManagerIface, QStringLiteral("SetLockWallpaperOverride"),
        QVariantList() << path);
}
