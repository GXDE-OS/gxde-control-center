/*
 * Copyright (C) 2015 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
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

#include "frame.h"
#include "dbuscontrolcenterservice.h"

#include <DApplication>
#include <DDBusSender>
#include <DLog>
#include <QStyle>
#include <QDBusMetaType>
#include <QMap>
#include <wayland-client.h>

#ifdef HAS_LAYER_SHELL
#include <LayerShellQt/Shell>
#endif

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

static const QString getQssFromFile(const QString &name)
{
#ifdef QT_DEBUG
//    qDebug() << "load qss file: " << name;
#endif

    QString qss;

    QFile f(name);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qss = f.readAll();
        f.close();
    }

    return qss;
}

static const QString getStyleSheetFromDir(QDir dir)
{
    QString ret;

    for (auto name : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (name.suffix() == "qss") {
            ret.append(getQssFromFile(name.absoluteFilePath()));
        }
    }

    return ret;
}

static const QString styleSheetFromTheme(const QString &theme)
{
    QStringList moduleList = {
        "frame", "widgets",    "accounts",        "defapp",   "mouse",   "wacom",
        "sound", "bluetooth",  "personalization", "datetime", "display", "keyboard",
        "power", "systeminfo", "update",          "network",  "cloudsync"
    };

    QString ret;

    const QString resources = ":/%1/themes/" + theme;
    for (auto module : moduleList)
    {
        QString dir = resources.arg(module);
        ret.append(getStyleSheetFromDir(dir));
    }

    return ret;
}

static void onThemeChange(const QString &theme)
{
    QString qss;
    qss.append(styleSheetFromTheme("common"));
    qss.append(styleSheetFromTheme(theme));

#ifdef QT_DEBUG
//    qDebug() << "set theme:" << theme;
//    qDebug().noquote() << "qss: " << Qt::endl << qss;
#endif

    qApp->setStyleSheet(qss);
}

static void onFontSizeChanged(const float pointSizeF) {
    // TODO(hualet): DPI default to 96.
    auto PxToPt = [] (int px) -> float {
        return px * 72.0 / 96;
    };

    QFont font(qApp->font());
    if (pointSizeF <= 8.2) {
        font.setPointSizeF(PxToPt(10));
        qApp->setFont(font, "dcc::widgets::SmallLabel");
        font.setPointSizeF(PxToPt(11));
        qApp->setFont(font, "dcc::widgets::NormalLabel");
        font.setPointSizeF(PxToPt(12));
        qApp->setFont(font, "dcc::widgets::LargeLabel");
    } else if (pointSizeF == 9) {
        font.setPointSizeF(PxToPt(11));
        qApp->setFont(font, "dcc::widgets::SmallLabel");
        font.setPointSizeF(PxToPt(12));
        qApp->setFont(font, "dcc::widgets::NormalLabel");
        font.setPointSizeF(PxToPt(16));
        qApp->setFont(font, "dcc::widgets::LargeLabel");
    }  else if (pointSizeF == 9.7) {
        font.setPointSizeF(PxToPt(11));
        qApp->setFont(font, "dcc::widgets::SmallLabel");
        font.setPointSizeF(PxToPt(13));
        qApp->setFont(font, "dcc::widgets::NormalLabel");
        font.setPointSizeF(PxToPt(15));
        qApp->setFont(font, "dcc::widgets::LargeLabel");
    } else if (pointSizeF == 11.2) {
        font.setPointSizeF(PxToPt(12));
        qApp->setFont(font, "dcc::widgets::SmallLabel");
        font.setPointSizeF(PxToPt(15));
        qApp->setFont(font, "dcc::widgets::NormalLabel");
        font.setPointSizeF(PxToPt(18));
        qApp->setFont(font, "dcc::widgets::LargeLabel");
    } else if (pointSizeF >= 12) {
        font.setPointSizeF(PxToPt(14));
        qApp->setFont(font, "dcc::widgets::SmallLabel");
        font.setPointSizeF(PxToPt(16));
        qApp->setFont(font, "dcc::widgets::NormalLabel");
        font.setPointSizeF(PxToPt(20));
        qApp->setFont(font, "dcc::widgets::LargeLabel");
    }
}

static bool checkWaylandReallyWorks() {
    const char *display = getenv("WAYLAND_DISPLAY");
    if (!display) return false;
    
    // 尝试连接 Wayland socket
    struct wl_display *d = wl_display_connect(display);
    if (d) {
        wl_display_disconnect(d);
        return true; // 真的用 Wayland
    }
    return false; // 假的，用 X11
}


int main(int argc, char *argv[])
{
    // 修复点击键盘与语言和鼠标子页设置崩溃的问题
    qDBusRegisterMetaType<QMap<QString, QString>>();

    if (checkWaylandReallyWorks()) {
        qunsetenv("DTK2_XWAYLAND");
        qputenv("QT_QPA_PLATFORM", "wayland");
#ifdef HAS_LAYER_SHELL
        LayerShellQt::Shell::useLayerShell();
#endif
    } else {
        qunsetenv("WAYLAND_DISPLAY");
        DApplication::loadDXcbPlugin();
    }

    DApplication app(argc, argv);
    app.setOrganizationName("GXDE");
    app.setApplicationName("gxde-control-center");
    app.setApplicationVersion("4.0");
    app.setAttribute(Qt::AA_UseHighDpiPixmaps);
    app.loadTranslator();
//    app.loadTranslator(QList<QLocale>() << QLocale::Chinese);
    app.setQuitOnLastWindowClosed(false);
    app.setTheme("light");

    // load gxde-network-utils translator
    QTranslator translator;
    translator.load("/usr/share/gxde-network-utils/translations/gxde-network-utils_" + QLocale::system().name());
    app.installTranslator(&translator);

    app.setTheme("semidark");
    onThemeChange("dark");

    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();

    // take care of command line options
    QCommandLineOption showOption(QStringList() << "s" << "show", "show control center(hide for default).");
    QCommandLineOption toggleOption(QStringList() << "t" << "toggle", "toggle control center visible.");
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
    parser.process(app);

    const QString &reqModule = parser.value(moduleOption);
    const QString &reqPage = parser.value(pageOption);

    QTimer::singleShot(0, [] { onFontSizeChanged(qApp->font().pointSizeF()); });

    Frame f;
    DBusControlCenterService adaptor(&f);
    Q_UNUSED(adaptor);
    QDBusConnection conn = QDBusConnection::sessionBus();
    if (!conn.registerService("com.deepin.dde.ControlCenter") ||
        !conn.registerObject("/com/deepin/dde/ControlCenter", &f)) {
        qDebug() << "dbus service already registered!";

        if (parser.isSet(toggleOption))
            DDBusSender()
                    .service("com.deepin.dde.ControlCenter")
                    .interface("com.deepin.dde.ControlCenter")
                    .path("/com/deepin/dde/ControlCenter")
                    .method("Toggle")
                    .call();

        if (!reqModule.isEmpty())
            DDBusSender()
                    .service("com.deepin.dde.ControlCenter")
                    .interface("com.deepin.dde.ControlCenter")
                    .path("/com/deepin/dde/ControlCenter")
                    .method("ShowPage")
                    .arg(reqModule)
                    .arg(reqPage)
                    .call();

        else if (parser.isSet(showOption))
            DDBusSender()
                    .service("com.deepin.dde.ControlCenter")
                    .interface("com.deepin.dde.ControlCenter")
                    .path("/com/deepin/dde/ControlCenter")
                    .method("Show")
                    .call();

#ifndef QT_DEBUG
        return 0;
#endif
    }

#ifndef QT_DEBUG
    if (parser.isSet(showOption) || parser.isSet(toggleOption))
#endif
        QTimer::singleShot(1, &f, &Frame::startup);

    if (!reqModule.isEmpty())
        QTimer::singleShot(10, &f, [&] { f.showSettingsPage(reqModule, reqPage); });

    QObject::connect(&f, &Frame::fontSizeChanged, &f, [=] {
        onFontSizeChanged(qApp->font().pointSizeF());
    });

    return app.exec();
}
