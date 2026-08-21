/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 */

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

const QString GxdmConfigPath = QStringLiteral("/etc/gxdm.conf");

bool isValidUser(const QByteArray &userName)
{
    if (userName.isEmpty() || userName.contains('\0')) {
        return false;
    }

    const passwd *entry = getpwnam(userName.constData());
    return entry && userName == entry->pw_name && entry->pw_uid != 0;
}

int writeAutoLoginUser(const QString &userName)
{
    QStringList lines;
    QFile currentConfig(GxdmConfigPath);
    if (currentConfig.exists()) {
        if (!currentConfig.open(QIODevice::ReadOnly)) {
            qCritical("Failed to read %s", qPrintable(GxdmConfigPath));
            return 1;
        }
        const QString content = QString::fromUtf8(currentConfig.readAll());
        lines = content.split(QLatin1Char('\n'));
        if (content.endsWith(QLatin1Char('\n'))) {
            lines.removeLast();
        }
    }

    const QString managedLine = QStringLiteral("User=%1").arg(userName);
    const QString managedSection = QStringLiteral("[Autologin]");
    bool inAutologin = false;
    bool foundAutologin = false;
    bool foundUser = false;
    int lastAutologinStart = -1;

    for (int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines.at(i).trimmed();
        const int commentPosition = trimmed.indexOf(QLatin1Char('#'));
        const QString config = commentPosition < 0
            ? trimmed : trimmed.left(commentPosition).trimmed();
        if (config.startsWith(QLatin1Char('['))
                && config.endsWith(QLatin1Char(']'))) {
            inAutologin = config.mid(1, config.size() - 2)
                == QStringLiteral("Autologin");
            if (inAutologin) {
                lines[i] = managedSection;
                foundAutologin = true;
                lastAutologinStart = i;
            }
            continue;
        }

        const int separatorPosition = config.indexOf(QLatin1Char('='));
        if (inAutologin && separatorPosition >= 0
                && config.left(separatorPosition).trimmed()
                    == QStringLiteral("User")) {
            lines[i] = managedLine;
            foundUser = true;
        }
    }

    if (!foundUser) {
        if (foundAutologin) {
            lines.insert(lastAutologinStart + 1, managedLine);
        } else {
            lines.prepend(managedLine);
            lines.prepend(managedSection);
        }
    }

    const QFileInfo configInfo(GxdmConfigPath);
    if (!QDir().mkpath(configInfo.absolutePath())) {
        qCritical("Failed to create %s", qPrintable(configInfo.absolutePath()));
        return 1;
    }

    QSaveFile output(GxdmConfigPath);
    if (!output.open(QIODevice::WriteOnly)) {
        qCritical("Failed to open %s for writing", qPrintable(GxdmConfigPath));
        return 1;
    }
    output.write(lines.join(QLatin1Char('\n')).toUtf8());
    output.write("\n");
    if (!output.commit()) {
        qCritical("Failed to write %s", qPrintable(GxdmConfigPath));
        return 1;
    }

    QFile::setPermissions(GxdmConfigPath,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner
        | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (geteuid() != 0) {
        qCritical("This helper must run as root");
        return 1;
    }

    const QStringList args = app.arguments();
    if (args.size() == 2 && args.at(1) == QStringLiteral("disable")) {
        return writeAutoLoginUser(QString());
    }

    if (args.size() == 3 && args.at(1) == QStringLiteral("enable")) {
        const QByteArray userName = args.at(2).toLocal8Bit();
        if (!isValidUser(userName)) {
            qCritical("Invalid automatic login user");
            return 2;
        }
        return writeAutoLoginUser(QString::fromLocal8Bit(userName));
    }

    qCritical("Usage: %s enable USER | disable", argv[0]);
    return 2;
}
