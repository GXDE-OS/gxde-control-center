#include "videothumbnail.h"
#include <QImage>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QEventLoop>
#include <QTimer>
#include <QFileInfo>

VideoThumbnail::VideoThumbnail()
{
}

QImage VideoThumbnail::getVideoThumbnailQt5(const QString& filePath, qint64 positionMs) {
    QImage thumbnail;

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qWarning() << "File not found:" << filePath;
        return thumbnail;
    }

    QMediaPlayer player;
    QVideoSink videoSink;
    player.setVideoOutput(&videoSink);

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QObject::connect(&timeoutTimer, &QTimer::timeout, [&]() {
        qWarning() << "Operation timed out";
        loop.quit();
    });

    QObject::connect(&player, &QMediaPlayer::errorOccurred,
        [&](QMediaPlayer::Error, const QString &errorString) {
            qWarning() << "Media error:" << errorString;
            loop.quit();
        });

    bool frameCaptured = false;

    QObject::connect(&videoSink, &QVideoSink::videoFrameChanged, [&](const QVideoFrame &frame) {
        if (frame.isValid() && !frameCaptured) {
            thumbnail = frame.toImage();
            if (!thumbnail.isNull()) {
                frameCaptured = true;
                loop.quit();
            }
        }
    });

    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, [&](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia) {
            player.setPosition(positionMs);
            timeoutTimer.start(3000);
        } else if (status == QMediaPlayer::InvalidMedia) {
            qWarning() << "Invalid media file";
            loop.quit();
        }
    });

    player.setSource(QUrl::fromLocalFile(filePath));
    timeoutTimer.start(5000);
    loop.exec();

    return thumbnail;
}
