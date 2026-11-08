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

#ifndef FACEMODEL_H
#define FACEMODEL_H

#include <QObject>
#include <QMap>
#include <QStringList>

namespace dcc {
namespace accounts {

// howdy 录入过程的引导状态(add 命令输出的关键行,见 howdy/src/cli/add.py)
enum HowdyStatus {
    HowdyLookingCamera = 0,
    HowdyNoFace,
    HowdyMultipleFaces,
    HowdyDarkFrames,
    HowdySuccess,
    HowdyError
};

class FaceModel : public QObject
{
    Q_OBJECT
public:
    explicit FaceModel(QObject *parent = nullptr);

    // 录入状态,Guidance 时携带 deepin-face 的原始状态码
    enum EnrollStatus {
        Ready,
        Guidance,
        Finished,
        Failed
    };

    bool isValid() const;
    void setIsValid(const bool isValid);

    EnrollStatus enrollStatus() const;
    void setEnrollStatus(const EnrollStatus &status, const qint32 code);

    void setUserFaces(const QString &user, const QStringList &faces);
    QStringList userFaces(const QString &user) const;
    void cleanUserFaces(const QString &user);

Q_SIGNALS:
    void validChanged(const bool isValid) const;
    void enrollStatusChanged(const FaceModel::EnrollStatus status, const qint32 code) const;
    void userFacesChanged(const QString &user, const QStringList &faces) const;

private:
    bool m_isValid;
    EnrollStatus m_enrollStatus;
    QMap<QString, QStringList> m_userFaces;
};
}
}

#endif // FACEMODEL_H
