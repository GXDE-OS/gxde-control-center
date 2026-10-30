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

#include "modulehost.h"

#include "moduleinitthread.h"
#include "modules/modulewidget.h"
#include "widgets/contentwidget.h"

#include "modules/accounts/accountsmodule.h"
#include "modules/bluetooth/bluetoothmodule.h"
#include "modules/datetime/datetimemodule.h"
#include "modules/defapp/defaultappsmodule.h"
#include "modules/display/displaymodule.h"
#include "modules/keyboard/keyboardmodule.h"
#include "modules/mouse/mousemodule.h"
#include "modules/network/networkmodule.h"
#include "modules/personalization/personalizationmodule.h"
#include "modules/power/powermodule.h"
#include "modules/sound/soundmodule.h"
#include "modules/sync/syncmodule.h"
#include "modules/systeminfo/systeminfomodule.h"
#include "modules/update/updatemodule.h"
#include "modules/wacom/wacommodule.h"

#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <QTimer>

namespace dcc {
namespace quick {

namespace {

// Same list as ModuleList in the old NavigationBar.
const QStringList &enabledModules()
{
    static const QStringList modules = {
#ifndef DISABLE_ACCOUNT
        "accounts",
#endif
#ifndef DISABLE_CLOUD_SYNC
        "cloudsync",
#endif
#ifndef DISABLE_DISPALY
        "display",
#endif
#ifndef DISABLE_DEFAULT_APPLICATIONS
        "defapp",
#endif
#ifndef DISABLE_PERSONALIZATION
        "personalization",
#endif
#ifndef DISABLE_NETWORK
        "network",
#endif
#ifndef DISABLE_BLUETOOTH
        "bluetooth",
#endif
#ifndef DISABLE_SOUND
        "sound",
#endif
#ifndef DISABLE_DATETIME
        "datetime",
#endif
#ifndef DISABLE_POWER
        "power",
#endif
#ifndef DISABLE_MOUSE
        "mouse",
#endif
#ifndef DISABLE_KAYBOARD
        "keyboard",
#endif
#ifndef DISABLE_WACOM
        "wacom",
#endif
#ifndef DISABLE_SYS_UPDATE
        "update",
#endif
#ifndef DISABLE_SYSINFO
        "systeminfo",
#endif
    };

    return modules;
}

// Use the NavModel translation contexts, so the .ts files still work.
struct ModuleTitle {
    const char *name;
    const char *context;
    const char *title;
};

const ModuleTitle ModuleTitles[] = {
    { "accounts", "dcc::accounts::AccountsWidget", QT_TRANSLATE_NOOP("dcc::accounts::AccountsWidget", "Accounts") },
    { "cloudsync", "dcc::cloudsync::SyncWidget", QT_TRANSLATE_NOOP("dcc::cloudsync::SyncWidget", "Cloud Sync") },
    { "display", "dcc::display::DisplayWidget", QT_TRANSLATE_NOOP("dcc::display::DisplayWidget", "Display") },
    { "defapp", "dcc::defapp::DefAppViewer", QT_TRANSLATE_NOOP("dcc::defapp::DefAppViewer", "Default Applications") },
    { "personalization", "dcc::personalization::PersonalizationWidget", QT_TRANSLATE_NOOP("dcc::personalization::PersonalizationWidget", "Personalization") },
    { "network", "dcc::network::NetworkModuleWidget", QT_TRANSLATE_NOOP("dcc::network::NetworkModuleWidget", "Network") },
    { "bluetooth", "dcc::bluetooth::BluetoothModule", QT_TRANSLATE_NOOP("dcc::bluetooth::BluetoothModule", "Bluetooth") },
    { "sound", "dcc::sound::SoundWidget", QT_TRANSLATE_NOOP("dcc::sound::SoundWidget", "Sound") },
    { "datetime", "dcc::datetime::Datetime", QT_TRANSLATE_NOOP("dcc::datetime::Datetime", "Time and Date") },
    { "power", "dcc::power::PowerWidget", QT_TRANSLATE_NOOP("dcc::power::PowerWidget", "Power Management") },
    { "mouse", "dcc::mouse::MouseWidget", QT_TRANSLATE_NOOP("dcc::mouse::MouseWidget", "Mouse") },
    { "keyboard", "dcc::keyboard::KeyboardWidget", QT_TRANSLATE_NOOP("dcc::keyboard::KeyboardWidget", "Keyboard and Language") },
    { "wacom", "dcc::wacom::WacomWidget", QT_TRANSLATE_NOOP("dcc::wacom::WacomWidget", "Wacom") },
    { "update", "dcc::update::UpdateCtrlWidget", QT_TRANSLATE_NOOP("dcc::update::UpdateCtrlWidget", "Update") },
    { "systeminfo", "dcc::systeminfo::SystemInfoWidget", QT_TRANSLATE_NOOP("dcc::systeminfo::SystemInfoWidget", "System Information") },
};

} // namespace

ModuleHost::ModuleHost(QObject *parent)
    : QAbstractListModel(parent)
    // Like the old frontend: hidden until the module finds a device.
    , m_hiddenModules({ "bluetooth", "wacom" })
    , m_loadDelay(0)
    , m_visibleQueueCount(0)
{
}

ModuleHost::~ModuleHost()
{
    unload();
}

int ModuleHost::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_ready.size();
}

QVariant ModuleHost::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_ready.size())
        return QVariant();

    ModuleInterface *module = m_ready.at(index.row());
    switch (role) {
    case NameRole:
        return module->name();
    case WidgetRole:
        return QVariant::fromValue<QObject *>(m_moduleWidgets.value(module).data());
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ModuleHost::roleNames() const
{
    return {
        { NameRole, "name" },
        { WidgetRole, "widget" },
    };
}

QStringList ModuleHost::moduleNames() const
{
    return enabledModules();
}

QStringList ModuleHost::hiddenModules() const
{
    return m_hiddenModules;
}

bool ModuleHost::loaded() const
{
    return !m_modules.isEmpty();
}

QString ModuleHost::displayName(const QString &module) const
{
    for (const ModuleTitle &title : ModuleTitles) {
        if (module == QLatin1String(title.name))
            return QCoreApplication::translate(title.context, title.title);
    }

    return QCoreApplication::translate("NavModel", "Navigation");
}

bool ModuleHost::isReady(const QString &module) const
{
    ModuleInterface *inter = findModule(module);
    return inter && m_ready.contains(inter);
}

bool ModuleHost::allReady() const
{
    return loaded() && m_ready.size() == m_modules.size();
}

QObject *ModuleHost::moduleWidget(const QString &module) const
{
    return m_moduleWidgets.value(findModule(module)).data();
}

void ModuleHost::load()
{
    if (loaded())
        return;

    // Load order is the order on All Settings.
#ifndef DISABLE_ACCOUNT
    loadModule(new accounts::AccountsModule(this));
#endif
#ifndef DISABLE_CLOUD_SYNC
    loadModule(new cloudsync::SyncModule(this));
#endif
#ifndef DISABLE_DISPALY
    loadModule(new display::DisplayModule(this));
#endif
#ifndef DISABLE_DEFAULT_APPLICATIONS
    loadModule(new defapp::DefaultAppsModule(this));
#endif
#ifndef DISABLE_PERSONALIZATION
    loadModule(new personalization::PersonalizationModule(this));
#endif
#ifndef DISABLE_NETWORK
    loadModule(new network::NetworkModule(this));
#endif
#ifndef DISABLE_BLUETOOTH
    loadModule(new bluetooth::BluetoothModule(this));
#endif
#ifndef DISABLE_SOUND
    loadModule(new sound::SoundModule(this));
#endif
#ifndef DISABLE_DATETIME
    loadModule(new datetime::DatetimeModule(this));
#endif
#ifndef DISABLE_POWER
    loadModule(new power::PowerModule(this));
#endif
#ifndef DISABLE_MOUSE
    loadModule(new mouse::MouseModule(this));
#endif
#ifndef DISABLE_KAYBOARD
    loadModule(new keyboard::KeyboardModule(this));
#endif
#ifndef DISABLE_WACOM
    loadModule(new wacom::WacomModule(this));
#endif
#ifndef DISABLE_SYS_UPDATE
    loadModule(new update::UpdateModule(this));
#endif
#ifndef DISABLE_SYSINFO
    loadModule(new systeminfo::SystemInfoModule(this));
#endif

    Q_EMIT loadedChanged();
}

void ModuleHost::unload()
{
    if (!loaded())
        return;

    // Module destructors expect initialize() to be done.
    finishInitThreads();

    beginResetModel();

    // Same as the old ~SettingsWidget: pages first, then modules, then any
    // module widgets that are still alive.
    for (const auto &contents : std::as_const(m_contents)) {
        for (const QPointer<ContentWidget> &content : contents) {
            if (content) {
                content->setVisible(false);
                content->deleteLater();
            }
        }
    }

    // Clear the lists first. Deleting a widget runs QML code that calls back
    // into this class, and it must not find the deleted modules.
    const auto modules = std::exchange(m_modules, {});
    const auto widgets = std::exchange(m_moduleWidgets, {});
    m_ready.clear();
    m_contents.clear();
    m_active.clear();
    m_loadDelay = 0;

    qDeleteAll(modules);
    for (const QPointer<QWidget> &widget : widgets)
        delete widget.data();

    endResetModel();
    Q_EMIT loadedChanged();
}

void ModuleHost::setModuleActive(const QString &module, bool active)
{
    ModuleInterface *inter = findModule(module);
    if (!inter || !m_ready.contains(inter) || m_active.value(inter) == active)
        return;

    m_active[inter] = active;
    if (active)
        inter->moduleActive();
    else
        inter->moduleDeactive();
}

void ModuleHost::showPage(const QString &module, const QString &page)
{
    ModuleInterface *inter = findModule(module);
    if (inter && m_ready.contains(inter) && !page.isEmpty())
        inter->showPage(page);
}

void ModuleHost::contentPopped(QObject *content)
{
    ContentWidget *w = qobject_cast<ContentWidget *>(content);
    if (!w)
        return;

    for (auto it = m_contents.begin(); it != m_contents.end(); ++it) {
        if (it.value().removeAll(w) > 0) {
            w->setVisible(false);
            it.key()->contentPopped(w);
            return;
        }
    }
}

void ModuleHost::setContentAppeared(QObject *content, bool appeared)
{
    ContentWidget *w = qobject_cast<ContentWidget *>(content);
    if (!w)
        return;

    if (appeared)
        Q_EMIT w->appear();
    else
        Q_EMIT w->disappear();
}

void ModuleHost::requestBack(QObject *content)
{
    if (ContentWidget *w = qobject_cast<ContentWidget *>(content))
        w->sendBackSignal();
}

void ModuleHost::resetAll()
{
    for (ModuleInterface *module : std::as_const(m_modules))
        module->reset();
}

void ModuleHost::pushWidget(ModuleInterface *const inter, ContentWidget *const w)
{
    Q_ASSERT(m_modules.contains(inter));

    m_contents[inter].append(w);
    // A module can push the same page again. Qt::UniqueConnection does not
    // work with a lambda, so drop the old connection first.
    disconnect(w, &ContentWidget::back, this, nullptr);
    connect(w, &ContentWidget::back, this, [this, w] {
        Q_EMIT contentBackRequested(w);
    });

    Q_EMIT contentPushed(w, inter->name());
}

void ModuleHost::setFrameAutoHide(ModuleInterface *const inter, const bool autoHide)
{
    Q_UNUSED(inter)

    // Several modules can ask at once. Auto hide comes back after the last one.
    m_visibleQueueCount += autoHide ? -1 : 1;

    if (m_visibleQueueCount == 0) {
        QTimer::singleShot(100, this, [this] {
            if (m_visibleQueueCount == 0)
                Q_EMIT autoHideRequested(true);
        });
    } else {
        Q_EMIT autoHideRequested(false);
    }
}

void ModuleHost::setModuleVisible(ModuleInterface *const inter, const bool visible)
{
    // Modules may call this from their init thread.
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, inter, visible] {
            setModuleVisible(inter, visible);
        }, Qt::QueuedConnection);
        return;
    }

    if (QWidget *widget = inter->moduleWidget()) {
        keepOffScreen(widget);
        widget->setVisible(visible);
    }

    const QString name = inter->name();
    if (visible == !m_hiddenModules.contains(name))
        return;

    if (visible)
        m_hiddenModules.removeAll(name);
    else
        m_hiddenModules.append(name);

    Q_EMIT hiddenModulesChanged();
}

void ModuleHost::showModulePage(const QString &module, const QString &page, bool animation)
{
    Q_EMIT modulePageRequested(module, page, animation);
}

void ModuleHost::loadModule(ModuleInterface *module)
{
    m_modules.append(module);
    m_contents.insert(module, {});

    ModuleInitThread *thread = new ModuleInitThread(module, this);
    m_initThreads.append(thread);
    connect(thread, &ModuleInitThread::moduleInitFinished, this, &ModuleHost::onModuleInitFinished,
            Qt::QueuedConnection);
    connect(thread, &ModuleInitThread::finished, thread, &ModuleInitThread::deleteLater,
            Qt::QueuedConnection);

    // Start the threads at different times, so they do not all hit D-Bus at once.
    QTimer::singleShot(m_loadDelay, thread, [thread] { thread->start(QThread::LowestPriority); });
    m_loadDelay = (m_loadDelay + 50) % 340;
}

void ModuleHost::onModuleInitFinished(ModuleInterface *module)
{
    if (!m_modules.contains(module))
        return;

    // Insert in navigation order.
    int row = 0;
    for (ModuleInterface *other : std::as_const(m_modules)) {
        if (other == module)
            break;
        if (m_ready.contains(other))
            ++row;
    }

    keepOffScreen(module->moduleWidget());
    m_moduleWidgets.insert(module, module->moduleWidget());
    m_active.insert(module, false);

    beginInsertRows(QModelIndex(), row, row);
    m_ready.insert(row, module);
    endInsertRows();

    Q_EMIT moduleReady(module->name());
}

void ModuleHost::finishInitThreads()
{
    for (const QPointer<ModuleInitThread> &thread : std::as_const(m_initThreads)) {
        if (!thread)
            continue;

        // Start threads that are still waiting for their timer.
        if (!thread->isRunning() && !thread->isFinished())
            thread->start();
        thread->wait();

        // This also stops the start timer. A queued moduleInitFinished is
        // ignored, because the module is no longer in the list.
        delete thread.data();
    }

    m_initThreads.clear();
}

void ModuleHost::keepOffScreen(QWidget *widget)
{
    // Only a hosted widget is shown, and LegacyWidget draws it. The window
    // hosts one module at a time, so the others have no parent, and a
    // show() from the module would open them as real windows.
    if (widget && widget->isWindow())
        widget->setAttribute(Qt::WA_DontShowOnScreen);
}

ModuleInterface *ModuleHost::findModule(const QString &name) const
{
    for (ModuleInterface *module : m_modules) {
        if (module->name() == name)
            return module;
    }

    return nullptr;
}

} // namespace quick
} // namespace dcc
