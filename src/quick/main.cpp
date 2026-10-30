/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
 * Copyright (C) 2026 shenmo <jifengshenmo@outlook.com>
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

#include "controlcenter.h"
#include "controlcenteradaptor.h"
#include "homewidgets.h"
#include "modulehost.h"
#include "userinfo.h"

#include <DDBusSender>
#include <DLog>
#include <dthememanager.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDir>
#include <QFont>
#include <QIcon>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>

#include <wayland-client.h>

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

using namespace dcc::quick;

namespace {

QString readStyleSheet(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(f.readAll()) : QString();
}

// The qss files of the modules, in the same order as the old main.cpp.
QString styleSheetFromTheme(const QString &theme)
{
    static const QStringList moduleList = {
        "frame", "widgets",    "accounts",        "defapp",   "mouse",   "wacom",
        "sound", "bluetooth",  "personalization", "datetime", "display", "keyboard",
        "power", "systeminfo", "update",          "network",  "cloudsync"
    };

    QString ret;
    for (const QString &module : moduleList) {
        const QDir dir(QString(":/%1/themes/%2").arg(module, theme));
        for (const QFileInfo &info : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
            if (info.suffix() == "qss")
                ret.append(readStyleSheet(info.absoluteFilePath()));
        }
    }

    return ret;
}

void applyLegacyTheme()
{
    // Old widgets need the DTK dsemidark style and the module qss.
    // DThemeManager works with a plain QApplication.
    DThemeManager::instance()->setTheme("semidark");
    qApp->setStyleSheet(styleSheetFromTheme("common") + styleSheetFromTheme("dark"));
}

void applyIconTheme()
{
    // Same as DApplication on Wayland: Qt does not get the icon theme there,
    // so take it from the deepin qt-theme config. DApplication reads
    // XSETTINGS first, which a Wayland session does not have.
    if (!QGuiApplication::platformName().startsWith("wayland"))
        return;

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "deepin", "qt-theme");
    const QString iconTheme = settings.value("Theme/IconThemeName").toString();
    if (!iconTheme.isEmpty())
        QIcon::setThemeName(iconTheme);
}

void updateLabelFonts(const double pointSizeF)
{
    auto pxToPt = [](int px) -> qreal {
        return px * 72.0 / 96;
    };

    // Set the three label fonts from the system font size, like before.
    int small = 0, normal = 0, large = 0;
    if (pointSizeF <= 8.2) {
        small = 10, normal = 11, large = 12;
    } else if (pointSizeF <= 9) {
        small = 11, normal = 12, large = 16;
    } else if (pointSizeF <= 9.75) {
        small = 11, normal = 13, large = 15;
    } else if (pointSizeF <= 11.25) {
        small = 12, normal = 15, large = 18;
    }

    QFont font(qApp->font());
    if (small > 0) {
        font.setPointSizeF(pxToPt(small));
        qApp->setFont(font, "dcc::widgets::SmallLabel");
        font.setPointSizeF(pxToPt(normal));
        qApp->setFont(font, "dcc::widgets::NormalLabel");
        font.setPointSizeF(pxToPt(large));
        qApp->setFont(font, "dcc::widgets::LargeLabel");
        return;
    }

    const int normalPixels = qRound(pointSizeF * 96.0 / 72.0);
    font.setPointSizeF(pxToPt(normalPixels - 2));
    qApp->setFont(font, "dcc::widgets::SmallLabel");
    font.setPointSizeF(pointSizeF);
    qApp->setFont(font, "dcc::widgets::NormalLabel");
    font.setPointSizeF(pxToPt(normalPixels + 4));
    qApp->setFont(font, "dcc::widgets::LargeLabel");
}

// Set the label fonts again when the system font size changes.
class FontWatcher : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == qApp && event->type() == QEvent::ApplicationFontChange)
            updateLabelFonts(qApp->font().pointSizeF());
        return false;
    }
};

void installTranslator(const QString &name, const QStringList &dirs)
{
    for (const QString &dir : dirs) {
        QTranslator *translator = new QTranslator(qApp);
        if (translator->load(QLocale::system(), name, "_", dir)) {
            qApp->installTranslator(translator);
            return;
        }
        delete translator;
    }
}

// Same places DApplication::loadTranslator() looks in.
void loadTranslators()
{
    QStringList appDirs;
    QStringList dtkDirs;
    for (const QString &path : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        appDirs << path + "/" + qApp->applicationName() + "/translations";
        dtkDirs << path + "/dtk2widget/translations";
    }
    appDirs.prepend(QDir::currentPath() + "/translations");

    installTranslator("qtbase", { QLibraryInfo::path(QLibraryInfo::TranslationsPath) });
    installTranslator("dtkwidget2", dtkDirs);
    installTranslator(qApp->applicationName(), appDirs);
    installTranslator("gxde-network-utils", { "/usr/share/gxde-network-utils/translations" });
}

bool checkWaylandReallyWorks()
{
    const char *display = getenv("WAYLAND_DISPLAY");
    if (!display)
        return false;

    // The variable alone does not mean the compositor works.
    struct wl_display *d = wl_display_connect(display);
    if (!d)
        return false;

    wl_display_disconnect(d);
    return true;
}

void callRunningInstance(const QString &method, const QStringList &args = QStringList())
{
    auto sender = DDBusSender()
                      .service("com.deepin.dde.ControlCenter")
                      .interface("com.deepin.dde.ControlCenter")
                      .path("/com/deepin/dde/ControlCenter")
                      .method(method);
    for (const QString &arg : args)
        sender = sender.arg(arg);
    sender.call();
}

} // namespace

int main(int argc, char *argv[])
{
    // Keyboard and mouse sub pages crash without this.
    qDBusRegisterMetaType<QMap<QString, QString>>();

    if (checkWaylandReallyWorks()) {
        // No LayerShellQt::Shell::useLayerShell(): it makes every window a
        // layer surface, also hidden host windows and module dialogs.
        // ControlCenter sets it on the panel window only.
        qputenv("QT_QPA_PLATFORM", "wayland");
    } else {
        qunsetenv("WAYLAND_DISPLAY");
    }

    // The panel background is see-through.
    QQuickWindow::setDefaultAlphaBuffer(true);
    // A custom style needs the full module URI.
    QQuickStyle::setStyle("QtQuick.Controls.Gxde");
    QQuickStyle::setFallbackStyle("Basic");

    QApplication app(argc, argv);
    app.setOrganizationName("GXDE");
    app.setApplicationName("gxde-control-center");
    app.setApplicationVersion("4.0");
    app.setQuitOnLastWindowClosed(false);
    app.setDesktopFileName("gxde-control-center");
    applyIconTheme();
    app.setWindowIcon(QIcon::fromTheme("preferences-system"));

    loadTranslators();
    applyLegacyTheme();

    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();

    QCommandLineOption showOption(QStringList() << "s" << "show", "show control center side panel(hide for default).");
    QCommandLineOption toggleOption(QStringList() << "t" << "toggle", "toggle control center side panel visible.");
    QCommandLineOption sidebarOption("sidebar", "show control center side panel.");
    QCommandLineOption windowOption("window", "show control center standalone window (default).");
    QCommandLineOption daemonOption("daemon", "run as resident service without showing UI.");
    QCommandLineOption moduleOption("m", "the module' id of which to be shown.", "module");
    QCommandLineOption pageOption("p", "specified module page", "page");

    QCommandLineParser parser;
    parser.setApplicationDescription("DDE Control Center");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(showOption);
    parser.addOption(toggleOption);
    parser.addOption(moduleOption);
    parser.addOption(pageOption);
    parser.addOption(sidebarOption);
    parser.addOption(windowOption);
    parser.addOption(daemonOption);
    parser.process(app);

    // Same options as GXDE CC Ultimate: the window opens with no options,
    // the old -s and -t still mean the panel.
    const bool daemonRequested = parser.isSet(daemonOption);
    const bool sideRequested = parser.isSet(showOption)
                               || parser.isSet(toggleOption)
                               || parser.isSet(sidebarOption);
    const bool windowRequested = parser.isSet(windowOption)
                                 || (!daemonRequested && !sideRequested);

    const QString reqModule = parser.value(moduleOption);
    const QString reqPage = parser.value(pageOption);

    QTimer::singleShot(0, &app, [] { updateLabelFonts(qApp->font().pointSizeF()); });
    app.installEventFilter(new FontWatcher(&app));

    ControlCenter controlCenter;
    ControlCenterAdaptor adaptor(&controlCenter);
    Q_UNUSED(adaptor)

    QDBusConnection conn = QDBusConnection::sessionBus();
    if (!conn.registerService("com.deepin.dde.ControlCenter")
        || !conn.registerObject("/com/deepin/dde/ControlCenter", &controlCenter)) {
        qDebug() << "dbus service already registered!";

        if (daemonRequested)
            return 0;

        if (windowRequested) {
            if (reqModule.isEmpty())
                callRunningInstance("ShowWindow");
            else
                callRunningInstance("ShowWindowPage", { reqModule, reqPage });
        } else if (parser.isSet(toggleOption)) {
            callRunningInstance("Toggle");
        } else if (!reqModule.isEmpty()) {
            callRunningInstance("ShowPage", { reqModule, reqPage });
        } else {
            callRunningInstance("Show");
        }

#ifndef QT_DEBUG
        return 0;
#endif
    }

    ModuleHost moduleHost;
    // The window has its own modules, like each SettingsSurface in the fork.
    ModuleHost windowModuleHost;
    UserInfo userInfo;
    HomeWidgets homeWidgets;

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        { "controlCenter", QVariant::fromValue(&controlCenter) },
        { "moduleHost", QVariant::fromValue(&moduleHost) },
        { "userInfo", QVariant::fromValue(&userInfo) },
        { "homeWidgets", QVariant::fromValue(&homeWidgets) },
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("org.gxde.controlcenter", "Main");

    QQuickWindow *window = engine.rootObjects().isEmpty()
                               ? nullptr
                               : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return -1;
    controlCenter.setWindow(window);

    engine.setInitialProperties({
        { "controlCenter", QVariant::fromValue(&controlCenter) },
        { "moduleHost", QVariant::fromValue(&windowModuleHost) },
    });
    engine.loadFromModule("org.gxde.controlcenter", "SettingsWindow");

    QQuickWindow *settingsWindow = engine.rootObjects().size() < 2
                                       ? nullptr
                                       : qobject_cast<QQuickWindow *>(engine.rootObjects().at(1));
    if (!settingsWindow)
        return -1;
    controlCenter.setSettingsWindow(settingsWindow);

    if (windowRequested) {
        QTimer::singleShot(1, &controlCenter, [&] {
            controlCenter.showWindow(reqModule, reqPage);
        });
    } else if (sideRequested) {
        QTimer::singleShot(1, &controlCenter, &ControlCenter::startup);

        if (!reqModule.isEmpty()) {
            QTimer::singleShot(10, &controlCenter, [&] {
                controlCenter.showSettingsPage(reqModule, reqPage);
            });
        }
    }

    const int ret = app.exec();

    // Delete module widgets before the QML scene.
    moduleHost.unload();
    windowModuleHost.unload();
    return ret;
}
