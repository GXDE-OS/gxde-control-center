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

#include "faceworker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QDebug>
#include <functional>

using namespace dcc;
using namespace dcc::accounts;

namespace {
const QString kHowdyConfig = QStringLiteral("/etc/howdy/config.ini");
const QString kHowdyModelsDir = QStringLiteral("/etc/howdy/models");
const QString kHowdyModelExt = QStringLiteral(".dat");

// cli.py 的安装位置(与系统已装 howdy 包一致)
const QString kHowdyCli = QStringLiteral("/usr/lib/howdy/cli.py");

// 通过 pkexec 调用 howdy CLI。
// com.github.boltgolt.howdy.cli action(python3 + cli.py 注解)对活动会话
// allow_active=yes,本机桌面用户免密;polkit 弹窗仅在其他环境出现。
// 注意:pkexec 可能不保留 LC_ALL,输出关键字解析仅作 best-effort,
// 最终成败以进程退出码为准。
void runPkexecHowdy(const QStringList &howdyArgs, const std::function<void(bool)> &onDone)
{
    QProcess *process = new QProcess;
    QObject::connect(process, &QProcess::finished, process,
                     [process, onDone] (int exitCode, QProcess::ExitStatus exitStatus) {
        const bool ok = (exitStatus == QProcess::NormalExit && exitCode == 0);
        if (onDone)
            onDone(ok);
        process->deleteLater();
    });

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process->setProcessEnvironment(env);

    QStringList pkexecArgs;
    pkexecArgs << QStringLiteral("/usr/bin/python3") << kHowdyCli << howdyArgs;
    process->start(QStringLiteral("pkexec"), pkexecArgs);
}
}

FaceWorker::FaceWorker(FaceModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_lastGuidance(HowdyLookingCamera)
{
}

bool FaceWorker::howdyInstalled() const
{
    if (QStandardPaths::findExecutable(QStringLiteral("howdy")).isEmpty())
        return false;

    if (!QFileInfo::exists(kHowdyConfig))
        return false;

    // 摄像头不存在时隐藏人脸入口
    const QFileInfoList devices = QDir(QStringLiteral("/dev")).entryInfoList(QDir::System);
    for (const QFileInfo &info : devices) {
        if (info.fileName().startsWith(QStringLiteral("video")))
            return true;
    }

    return false;
}

QString FaceWorker::modelFilePath(const QString &user) const
{
    return kHowdyModelsDir + QStringLiteral("/") + user + kHowdyModelExt;
}

void FaceWorker::refreshDevice()
{
    m_model->setIsValid(howdyInstalled());
}

void FaceWorker::refreshUserFaces(const QString &user)
{
    m_observedUser = user;

    QStringList faces;
    QFile file(modelFilePath(user));
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonArray array = doc.array();
        for (const QJsonValue &value : array)
            faces << QString::number(value.toObject().value(QStringLiteral("id")).toInt());
    } else if (file.exists()) {
        qWarning() << "cannot read howdy model file:" << modelFilePath(user);
    }

    m_model->setUserFaces(user, faces);
}

void FaceWorker::enrollStart(const QString &user)
{
    if (m_enrollProcess) {
        qWarning() << "face enroll is already running";
        return;
    }

    m_currentUser = user;
    m_lastGuidance = HowdyLookingCamera;

    // 立刻跳转到录入页,后续状态通过 FaceModel 推送
    Q_EMIT requestShowAddFace(user);

    m_model->setEnrollStatus(FaceModel::Ready, HowdyLookingCamera);

    QProcess *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process->setProcessEnvironment(env);

    connect(process, &QProcess::readyReadStandardOutput, this, &FaceWorker::onEnrollOutput);
    connect(process, &QProcess::finished, this, &FaceWorker::onEnrollFinished);

    m_enrollProcess = process;
    process->start(QStringLiteral("pkexec"),
                   QStringList({ QStringLiteral("/usr/bin/python3"), kHowdyCli,
                                 QStringLiteral("-U"), user,
                                 QStringLiteral("-y"), QStringLiteral("add") }));
}

void FaceWorker::reEnrollStart()
{
    // howdy add 每次成功都会追加一个模型,直接再次录入即可,
    // 不需要的旧模型可在列表页删除
    enrollStart(m_currentUser);
}

void FaceWorker::stopEnroll()
{
    if (m_enrollProcess) {
        // 断开回调避免取消过程触发失败状态;正在采样的 howdy 进程会自行结束,
        // 多写入的模型可在列表页删除
        m_enrollProcess->disconnect(this);
        m_enrollProcess->terminate();
        m_enrollProcess->deleteLater();
        m_enrollProcess = nullptr;
    }
}

void FaceWorker::saveFace(const QString &user)
{
    refreshUserFaces(user);
}

void FaceWorker::deleteFace(const QString &user, const QString &modelId)
{
    runPkexecHowdy(QStringList({ QStringLiteral("-U"), user,
                                 QStringLiteral("-y"), QStringLiteral("remove"), modelId }),
                   [this, user, modelId] (bool ok) {
        if (!ok)
            qWarning() << "howdy remove failed for" << user << modelId;
        refreshUserFaces(user);
    });
}

void FaceWorker::cleanFaces(const QString &user)
{
    runPkexecHowdy(QStringList({ QStringLiteral("-U"), user,
                                 QStringLiteral("-y"), QStringLiteral("clear") }),
                   [this, user] (bool ok) {
        if (!ok)
            qWarning() << "howdy clear failed for" << user;
        refreshUserFaces(user);
    });
}

void FaceWorker::onEnrollOutput()
{
    if (m_enrollProcess.isNull())
        return;

    const QString output = QString::fromLocal8Bit(m_enrollProcess->readAllStandardOutput());

    if (output.contains(QStringLiteral("look straight into the camera")))
        m_model->setEnrollStatus(FaceModel::Guidance, HowdyLookingCamera);
    else if (output.contains(QStringLiteral("No face detected")))
        m_lastGuidance = HowdyNoFace, m_model->setEnrollStatus(FaceModel::Guidance, HowdyNoFace);
    else if (output.contains(QStringLiteral("Multiple faces")))
        m_lastGuidance = HowdyMultipleFaces, m_model->setEnrollStatus(FaceModel::Guidance, HowdyMultipleFaces);
    else if (output.contains(QStringLiteral("black frames")) || output.contains(QStringLiteral("too dark")))
        m_lastGuidance = HowdyDarkFrames, m_model->setEnrollStatus(FaceModel::Guidance, HowdyDarkFrames);
}

void FaceWorker::onEnrollFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (sender() != m_enrollProcess.data())
        return;

    m_enrollProcess->deleteLater();
    m_enrollProcess = nullptr;

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        m_model->setEnrollStatus(FaceModel::Finished, HowdySuccess);
        refreshUserFaces(m_currentUser);
        return;
    }

    const HowdyStatus code = (m_lastGuidance != HowdyLookingCamera) ? m_lastGuidance : HowdyError;
    m_model->setEnrollStatus(FaceModel::Failed, code);
}
