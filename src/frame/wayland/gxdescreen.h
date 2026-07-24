#ifndef GXDESCREEN_H
#define GXDESCREEN_H

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>

namespace GxdeScreen {

static const QString Service = QStringLiteral("top.gxde.Wlcom.Screen");
static const QString Path = QStringLiteral("/top/gxde/Wlcom/Screen");
static const QString Interface = QStringLiteral("top.gxde.Wlcom.Screen");

static const QString LegacyService = QStringLiteral("com.kylin.Wlcom");
static const QString LegacyPath = QStringLiteral("/com/kylin/Wlcom/Output");
static const QString LegacyInterface = QStringLiteral("com.kylin.Wlcom.Output");

struct Output
{
    QString name;
    bool enabled = true;
    bool primary = false;
    int width = 0;
    int height = 0;
    int refresh = 0;
    int transform = 0;
    int x = 0;
    int y = 0;
    int brightness = 100;
    double scale = 1.0;
};

inline bool isAvailable()
{
    QDBusConnectionInterface *busInterface = QDBusConnection::sessionBus().interface();
    if (!busInterface)
        return false;

    const QDBusReply<bool> reply = busInterface->isServiceRegistered(Service);
    return reply.isValid() && reply.value();
}

inline bool call(const QString &method, const QVariantList &arguments = QVariantList())
{
    if (!isAvailable())
        return false;

    QDBusMessage message = QDBusMessage::createMethodCall(Service, Path, Interface, method);
    message.setArguments(arguments);
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block);
    return reply.type() == QDBusMessage::ReplyMessage;
}

inline QList<Output> outputs()
{
    QList<Output> result;
    QDBusMessage message =
        QDBusMessage::createMethodCall(LegacyService, LegacyPath, LegacyInterface,
                                       QStringLiteral("ListAllOutputs"));
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return result;

    const QDBusArgument array = qvariant_cast<QDBusArgument>(reply.arguments().first());
    array.beginArray();
    while (!array.atEnd()) {
        QString name;
        QString json;
        array.beginStructure();
        array >> name >> json;
        array.endStructure();

        Output output;
        output.name = name;
        const QJsonObject object = QJsonDocument::fromJson(json.toUtf8()).object();
        output.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        output.primary = object.value(QStringLiteral("primary")).toBool(false);
        output.width = object.value(QStringLiteral("width")).toInt();
        output.height = object.value(QStringLiteral("height")).toInt();
        output.refresh = object.value(QStringLiteral("refresh")).toInt();
        output.transform = object.value(QStringLiteral("transform")).toInt();
        output.x = object.value(QStringLiteral("lx")).toInt();
        output.y = object.value(QStringLiteral("ly")).toInt();
        output.brightness = object.value(QStringLiteral("brightness")).toInt(100);
        output.scale = object.value(QStringLiteral("scale")).toDouble(1.0);
        result.append(output);
    }
    array.endArray();
    return result;
}

inline QStringList outputNames()
{
    QStringList result;
    for (const Output &output : outputs())
        result.append(output.name);
    return result;
}

inline QMap<QString, double> brightness()
{
    QMap<QString, double> result;
    for (const Output &output : outputs())
        result.insert(output.name, output.brightness / 100.0);
    return result;
}

inline bool setBrightness(const QString &output, double brightness)
{
    const int percent = qBound(0, qRound(brightness * 100.0), 100);
    return call(QStringLiteral("SetScreenBrightness"),
                QVariantList() << output << percent);
}

inline bool setMode(uint mode, const QString &output = QString())
{
    return call(QStringLiteral("SetScreenMode"),
                QVariantList() << mode << output);
}

inline bool setScale(double scale)
{
    return call(QStringLiteral("SetScaleRatio"), QVariantList() << scale);
}

inline bool setScale(const QString &output, double scale)
{
    return call(QStringLiteral("SetScreenScale"), QVariantList() << output << scale);
}

inline bool setResolution(int width, int height, int refresh)
{
    return call(QStringLiteral("SetResolutionWRefreshRate"),
                QVariantList() << width << height << refresh);
}

inline bool setResolution(const QString &output, int width, int height, int refresh)
{
    return call(QStringLiteral("SetScreenResolution"),
                QVariantList() << output << width << height << refresh);
}

inline bool setRotation(int angle)
{
    return call(QStringLiteral("RotateScreen"), QVariantList() << angle);
}

inline bool setRotation(const QString &output, int angle)
{
    return call(QStringLiteral("SetScreenRotation"), QVariantList() << output << angle);
}

inline bool setEnabled(const QString &output, bool enabled)
{
    return call(QStringLiteral("SetScreenEnabled"), QVariantList() << output << enabled);
}

inline bool setPrimary(const QString &output)
{
    return call(QStringLiteral("SetPrimaryScreen"), QVariantList() << output);
}

inline bool setPosition(const QString &output, int x, int y)
{
    return call(QStringLiteral("SetScreenPosition"), QVariantList() << output << x << y);
}

inline int rotationToAngle(quint16 rotation)
{
    switch (rotation) {
    case 1:
        return 0;
    case 2:
        return 90;
    case 4:
        return 180;
    case 8:
        return 270;
    default:
        return -1;
    }
}

inline quint16 transformToRotation(int transform)
{
    switch (transform) {
    case 0:
        return 1;
    case 1:
        return 2;
    case 2:
        return 4;
    case 3:
        return 8;
    default:
        return 1;
    }
}

} // namespace GxdeScreen

#endif // GXDESCREEN_H
