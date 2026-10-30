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

#include "userinfo.h"

#include <com_deepin_daemon_accounts.h>

#include <QFile>

#include <unistd.h>

using AccountsInter = com::deepin::daemon::Accounts;
using UserInter = com::deepin::daemon::accounts::User;

namespace dcc {
namespace quick {

UserInfo::UserInfo(QObject *parent)
    : QObject(parent)
    , m_user(nullptr)
{
#ifndef DISABLE_ACCOUNT
    const QString uid = QString::number(getuid());
    AccountsInter accountsInter("com.deepin.daemon.Accounts", "/com/deepin/daemon/Accounts",
                                QDBusConnection::systemBus());
    for (const QString &path : accountsInter.userList()) {
        UserInter *inter = new UserInter("com.deepin.daemon.Accounts", path, QDBusConnection::systemBus(), this);
        if (inter->uid() == uid) {
            m_user = inter;
            break;
        }
        delete inter;
    }

    if (m_user) {
        connect(m_user, &UserInter::IconFileChanged, this, &UserInfo::avatarChanged);
        connect(m_user, &UserInter::UserNameChanged, this, &UserInfo::userNameChanged);
    }
#endif
}

bool UserInfo::available() const
{
    return m_user;
}

QString UserInfo::userName() const
{
    return m_user ? m_user->userName() : QString();
}

QUrl UserInfo::avatar() const
{
    return m_user ? QUrl(m_user->iconFile()) : QUrl();
}

QUrl UserInfo::avatarFor(qreal devicePixelRatio) const
{
    const QUrl url = avatar();
    if (devicePixelRatio <= 1.0)
        return url;

    const QUrl bigger(url.toString().replace("icons/", "icons/bigger/"));
    return QFile::exists(bigger.toLocalFile()) ? bigger : url;
}

} // namespace quick
} // namespace dcc
