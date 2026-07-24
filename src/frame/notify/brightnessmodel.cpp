#include "brightnessmodel.h"
#include "wayland/gxdescreen.h"
#include <QDebug>

BrightnessModel::BrightnessModel()
{

}

// TODO: Bad, empty
double BrightnessModel::GetFirstDisplayBrightness()
{
    const QMap<QString, double> brightness = GetBrightness();
    return brightness.isEmpty() ? 1.0 : brightness.constBegin().value();
}

// TODO: Bad, empty
QMap<QString, double> BrightnessModel::GetBrightness()
{
    const QMap<QString, double> gxdeBrightness = GxdeScreen::brightness();
    if (!gxdeBrightness.isEmpty())
        return gxdeBrightness;

    auto firstMap = DBusDictMethodWithoutArg(SERVICE,
                         PATH,
                         INTERFACES,
                         "GetBrightness");
    qDebug() << DBusDictMethodWithoutArg(SERVICE,
                                     PATH,
                                     INTERFACES,
                                     "GetBrightness").keys();
    QMap<QString, double> newMap;
    QMap<QString, QVariant>::Iterator iter;
    // 将 QVarient => double
    for (iter = firstMap.begin(); iter != firstMap.end(); ++iter) {
        newMap.insert(iter.key(), iter.value().toDouble());
    }
    return newMap;
}

QStringList BrightnessModel::ListOutputNames()
{
    const QStringList gxdeOutputs = GxdeScreen::outputNames();
    if (!gxdeOutputs.isEmpty())
        return gxdeOutputs;

    auto list = DBusMethodWithoutArg(SERVICE,
                      PATH,
                      INTERFACES,
                      "ListOutputNames");
    QStringList outputList;
    for (QVariant i: list) {
        outputList << i.toStringList();
    }
    return outputList;
}

void BrightnessModel::SetAllScreenBrightness(double brightness)
{
    for(QString i: ListOutputNames()) {
        SetBrightness(i, brightness);
    }
}

void BrightnessModel::SetBrightness(QString display,
                       double brightness)
{
    if (GxdeScreen::setBrightness(display, brightness))
        return;

    DBusMethodWithArg(SERVICE,
                      PATH,
                      INTERFACES,
                      "SetBrightness",
                      QList<QVariant>() << display << brightness);
}

// 不带参数 call
QList<QVariant> BrightnessModel::DBusMethodWithoutArg(QString service,
                                            QString path,
                                            QString interface,
                                            QString method)
{
    QDBusInterface dbus(service,
                        path,
                        interface);
    return dbus.call(method).arguments();
}

// 不带参数 call（Dict）
QVariantMap BrightnessModel::DBusDictMethodWithoutArg(QString service,
                                            QString path,
                                            QString interface,
                                            QString method)
{
    QDBusInterface dbus(service,
                        path,
                        interface);
    QDBusReply<QVariantMap> reply = dbus.call(method);
    return reply.value();
}

// 带参数 call
QList<QVariant> BrightnessModel::DBusMethodWithArg(QString service,
                                            QString path,
                                            QString interface,
                                            QString method,
                                            QList<QVariant> argument)
{
    QDBusInterface dbus(service,
                        path,
                        interface);
    return dbus.callWithArgumentList(QDBus::CallMode::NoBlock, method, argument).arguments();
}
