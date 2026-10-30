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

#ifndef MODULEHOST_H
#define MODULEHOST_H

#include "interfaces/frameproxyinterface.h"
#include "interfaces/moduleinterface.h"

#include <QAbstractListModel>
#include <QHash>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

class ModuleInitThread;

namespace dcc {
namespace quick {

// FrameProxyInterface for the QML frontend. Replaces the old SettingsWidget.
//
// Loads the modules in the old order. The module widgets are a list model
// for the All Settings page. Pages from pushWidget() go to the QML page
// stack through contentPushed.
class ModuleHost : public QAbstractListModel, public FrameProxyInterface
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ModuleHost is provided by the application")

    // All built modules, in navigation order.
    Q_PROPERTY(QStringList moduleNames READ moduleNames CONSTANT)
    // Modules hidden by themselves. The home grid and nav bar skip them.
    Q_PROPERTY(QStringList hiddenModules READ hiddenModules NOTIFY hiddenModulesChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        WidgetRole,
    };

    explicit ModuleHost(QObject *parent = nullptr);
    ~ModuleHost() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QStringList moduleNames() const;
    QStringList hiddenModules() const;
    bool loaded() const;

    // Translated module name, same as the old navigation.
    Q_INVOKABLE QString displayName(const QString &module) const;
    Q_INVOKABLE bool isReady(const QString &module) const;
    // True when every module is initialized.
    Q_INVOKABLE bool allReady() const;
    // Widget of a loaded module, or null.
    Q_INVOKABLE QObject *moduleWidget(const QString &module) const;

    // Create and init all modules, like the old prepareAllSettingsPage.
    Q_INVOKABLE void load();
    // Delete all modules and pages, like deleting the old SettingsWidget.
    Q_INVOKABLE void unload();

    // Called when a module scrolls in or out of view on All Settings.
    Q_INVOKABLE void setModuleActive(const QString &module, bool active);
    // Ask a loaded module to show one of its pages.
    Q_INVOKABLE void showPage(const QString &module, const QString &page);
    // Called after QML pops a page, so the module can drop it.
    Q_INVOKABLE void contentPopped(QObject *content);
    // Emit appear or disappear on a page when the panel shows or hides.
    Q_INVOKABLE void setContentAppeared(QObject *content, bool appeared);
    // Mouse back button on a page. Same as its back button.
    Q_INVOKABLE void requestBack(QObject *content);
    Q_INVOKABLE void resetAll();

    // FrameProxyInterface
    void pushWidget(ModuleInterface *const inter, ContentWidget *const w) override;
    void setFrameAutoHide(ModuleInterface *const inter, const bool autoHide) override;
    void setModuleVisible(ModuleInterface *const inter, const bool visible) override;
    void showModulePage(const QString &module, const QString &page, bool animation) override;

Q_SIGNALS:
    void hiddenModulesChanged();
    void loadedChanged();
    void moduleReady(const QString &module);
    void contentPushed(QObject *content, const QString &module);
    // The back button of a page was clicked.
    void contentBackRequested(QObject *content);
    void autoHideRequested(bool autoHide);
    void modulePageRequested(const QString &module, const QString &page, bool animation);

private:
    void loadModule(ModuleInterface *module);
    void onModuleInitFinished(ModuleInterface *module);
    void finishInitThreads();
    ModuleInterface *findModule(const QString &name) const;
    static void keepOffScreen(QWidget *widget);

    QList<ModuleInterface *> m_modules;
    QList<QPointer<ModuleInitThread>> m_initThreads;
    // Initialized modules in the model, in navigation order.
    QList<ModuleInterface *> m_ready;
    QHash<ModuleInterface *, QList<QPointer<ContentWidget>>> m_contents;
    QHash<ModuleInterface *, QPointer<QWidget>> m_moduleWidgets;
    QHash<ModuleInterface *, bool> m_active;
    QStringList m_hiddenModules;
    int m_loadDelay;
    int m_visibleQueueCount;
};

} // namespace quick
} // namespace dcc

#endif // MODULEHOST_H
