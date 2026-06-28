#include "videowallpapermodel.h"

#include <QDBusConnection>
#include <QDebug>
#include <QFile>
#include <QDir>

VideoWallpaperModel::VideoWallpaperModel()
{

}

void VideoWallpaperModel::play(const QString path)
{
    if(!path.isEmpty()){
        this->setFile(path);
    }
    QDBusMessage dbus = QDBusMessage::createMethodCall(m_videoDBusDestination,
                                                       m_videoDBusPath,
                                                       m_videoDBusinterface,
                                                       "play");
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
}

void VideoWallpaperModel::activeWindow()
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(m_videoDBusDestination,
                                                       m_videoDBusPath,
                                                       m_videoDBusinterface,
                                                       "activeWindow");
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
}

void VideoWallpaperModel::clear()
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(m_videoDBusDestination,
                                                       m_videoDBusPath,
                                                       m_videoDBusinterface,
                                                       "clear");
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
}

void VideoWallpaperModel::pause()
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(m_videoDBusDestination,
                                                       m_videoDBusPath,
                                                       m_videoDBusinterface,
                                                       "pause");
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
}

void VideoWallpaperModel::setFile(const QString path)
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(m_videoDBusDestination,
                                                       m_videoDBusPath,
                                                       m_videoDBusinterface,
                                                       "setFile");
    dbus << path;
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
}

void VideoWallpaperModel::setVolume(const int volume)
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(m_videoDBusDestination,
                                                       m_videoDBusPath,
                                                       m_videoDBusinterface,
                                                       "setFile");
    dbus << volume;
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
}

void VideoWallpaperModel::stop()
{
    QDBusMessage dbus = QDBusMessage::createMethodCall(m_videoDBusDestination,
                                                       m_videoDBusPath,
                                                       m_videoDBusinterface,
                                                       "stop");
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
}

void VideoWallpaperModel::setVideoWallpaperEnabled(const bool value)
{
    if(!QFile::exists("/usr/share/applications/fantascene-dynamic-wallpaper.desktop")){
        // Setting error
        qDebug() << "Can't find gxde top panel config file: /usr/share/applications/fantascene-dynamic-wallpaper.desktop";
        return;
    }
    if(value){
        QFile::copy("/usr/share/applications/fantascene-dynamic-wallpaper.desktop",
                    QDir::homePath() + "/.config/autostart/fantascene-dynamic-wallpaper.desktop"); // 设置自动启动
        system("setsid fantascene-dynamic-wallpaper > /dev/null 2>&1 &");
        return;
    }
    QFile::remove(QDir::homePath() + "/.config/autostart/fantascene-dynamic-wallpaper.desktop"); // 移除自动启动
    system("killall fantascene-dynamic-wallpaper -9");
}

bool VideoWallpaperModel::isVideoWallpaperEnabled() const
{
    return QFile::exists(QDir::homePath() + "/.config/autostart/fantascene-dynamic-wallpaper.desktop");
}
