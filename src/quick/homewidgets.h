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

#ifndef HOMEWIDGETS_H
#define HOMEWIDGETS_H

#include <QObject>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

class NotifyWidget;
class QuickControlPanel;
class UpdateNotifier;

namespace dcc {
namespace quick {

// Old widgets on the home page: notifications, quick settings and the update
// notice. LegacyWidget shows them; this class turns their signals into ones
// QML can use.
class HomeWidgets : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("HomeWidgets is provided by the application")

    Q_PROPERTY(QObject *notifyWidget READ notifyWidget CONSTANT)
    Q_PROPERTY(QObject *quickControlPanel READ quickControlPanel CONSTANT)
    Q_PROPERTY(QObject *updateNotifier READ updateNotifier CONSTANT)

public:
    explicit HomeWidgets(QObject *parent = nullptr);
    ~HomeWidgets() override;

    QObject *notifyWidget() const;
    QObject *quickControlPanel() const;
    QObject *updateNotifier() const;

public Q_SLOTS:
    // Shown on the notify page, hidden on the grid page.
    void setQuickControlPanelVisible(bool visible);
    void appear();
    void disappear();

Q_SIGNALS:
    void allSettingsRequested();
    void settingsPageRequested(const QString &module, const QString &page, bool animation);
    void screenCaptureStarted();

private:
    QPointer<NotifyWidget> m_notifyWidget;
    QPointer<QuickControlPanel> m_quickControlPanel;
    QPointer<UpdateNotifier> m_updateNotifier;
};

} // namespace quick
} // namespace dcc

#endif // HOMEWIDGETS_H
