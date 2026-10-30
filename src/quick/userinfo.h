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

#ifndef USERINFO_H
#define USERINFO_H

#include <com_deepin_daemon_accounts_user.h>

#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

namespace dcc {
namespace quick {

// Avatar and name of the current user, for the home page header.
class UserInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("UserInfo is provided by the application")

    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString userName READ userName NOTIFY userNameChanged)
    Q_PROPERTY(QUrl avatar READ avatar NOTIFY avatarChanged)

public:
    explicit UserInfo(QObject *parent = nullptr);

    bool available() const;
    QString userName() const;
    QUrl avatar() const;

    // On HiDPI use the big picture from icons/bigger/, like AvatarWidget.
    Q_INVOKABLE QUrl avatarFor(qreal devicePixelRatio) const;

Q_SIGNALS:
    void userNameChanged();
    void avatarChanged();

private:
    com::deepin::daemon::accounts::User *m_user;
};

} // namespace quick
} // namespace dcc

#endif // USERINFO_H
