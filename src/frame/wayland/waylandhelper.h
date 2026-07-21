#ifndef WAYLANDHELPER_H
#define WAYLANDHELPER_H

#include <QGuiApplication>
#include <QString>

namespace Wayland {

inline bool isWaylandSession() {
    if (!qGuiApp) {
        return false;
    }
    return QGuiApplication::platformName().contains(
        QStringLiteral("wayland"), Qt::CaseInsensitive);
}

}

#endif // WAYLANDHELPER_H
