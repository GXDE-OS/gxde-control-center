/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 */

#include "blursettingswindow.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

namespace {

const QString ServiceName = QStringLiteral("top.gxde.ControlCenter.BlurSettings");
const QString ObjectPath = QStringLiteral("/top/gxde/ControlCenter/BlurSettings");
const QString InterfaceName = QStringLiteral("top.gxde.ControlCenter.BlurSettings");

void installTranslator(QApplication *app, QTranslator *translator) {
    const QString fileName = QStringLiteral("gxde-control-center_%1.qm")
        .arg(QLocale::system().name());
    const QDir applicationDir(QCoreApplication::applicationDirPath());
    const QStringList searchDirectories = {
        applicationDir.absoluteFilePath(QStringLiteral("../share/gxde-control-center/translations")),
        applicationDir.absoluteFilePath(QStringLiteral("../../../translations"))
    };

    for (const QString &directory : searchDirectories) {
        if (translator->load(QDir(directory).filePath(fileName))) {
            app->installTranslator(translator);
            return;
        }
    }
}

}  // namespace

int main(int argc, char *argv[]) {
    qunsetenv("QT_WAYLAND_SHELL_INTEGRATION");

    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("GXDE"));
    app.setApplicationName(QStringLiteral("gxde-blur-settings"));
    QGuiApplication::setDesktopFileName(QStringLiteral("gxde-control-center"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-system")));

    QTranslator translator;
    installTranslator(&app, &translator);
    app.setApplicationDisplayName(QCoreApplication::translate(
        "dcc::personalization::GxwmWidget", "Blur settings"));

    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.registerService(ServiceName)) {
        QDBusMessage message = QDBusMessage::createMethodCall(
            ServiceName, ObjectPath, InterfaceName, QStringLiteral("Show"));
        connection.send(message);
        return 0;
    }

    dcc::personalization::BlurSettingsWindow window;
    if (!connection.registerObject(ObjectPath, InterfaceName, &window,
                                   QDBusConnection::ExportAllSlots)) {
        qWarning("Failed to register blur settings D-Bus object");
    }

    window.Show();
    return app.exec();
}
