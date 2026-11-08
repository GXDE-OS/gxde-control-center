/*
 * Copyright (C) 2011 ~ 2026 Deepin Technology Co., Ltd.
 *
 * Maintainer: gfdgd_xi
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

#ifndef FACEWORKER_H
#define FACEWORKER_H

#include "facemodel.h"

#include <QObject>
#include <QProcess>
#include <QPointer>

namespace dcc {
namespace accounts {

// 基于 howdy(https://github.com/boltgolt/howdy)的人脸管理后端:
// - 模型列表直接读取 /etc/howdy/models/<user>.dat(root:root 0644,普通用户可读)
// - 录入/删除/清空通过 pkexec 调用 howdy CLI(root 才能写模型目录)
// - 登录/锁屏时的验证由 howdy 的 PAM 模块完成,不经过控制中心
class FaceWorker : public QObject
{
    Q_OBJECT
public:
    explicit FaceWorker(FaceModel *model, QObject *parent = nullptr);

    void refreshDevice();
    void refreshUserFaces(const QString &user);

Q_SIGNALS:
    void requestShowAddFace(const QString &user);

public Q_SLOTS:
    void enrollStart(const QString &user);
    void reEnrollStart();
    void stopEnroll();
    void saveFace(const QString &user);
    void deleteFace(const QString &user, const QString &modelId);
    void cleanFaces(const QString &user);

private Q_SLOTS:
    void onEnrollOutput();
    void onEnrollFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QString modelFilePath(const QString &user) const;
    bool howdyInstalled() const;
    void startEnroll(const QString &user);

private:
    FaceModel *m_model;
    QString m_currentUser;
    QString m_observedUser;
    HowdyStatus m_lastGuidance;
    QPointer<QProcess> m_enrollProcess;
};

}
}

#endif // FACEWORKER_H
