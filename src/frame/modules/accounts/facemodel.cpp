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

#include "facemodel.h"

namespace dcc {
namespace accounts {

FaceModel::FaceModel(QObject *parent)
    : QObject(parent)
    , m_isValid(false)
    , m_enrollStatus(Ready)
{
}

bool FaceModel::isValid() const
{
    return m_isValid;
}

void FaceModel::setIsValid(const bool isValid)
{
    if (isValid == m_isValid)
        return;

    m_isValid = isValid;
    Q_EMIT validChanged(isValid);
}

FaceModel::EnrollStatus FaceModel::enrollStatus() const
{
    return m_enrollStatus;
}

void FaceModel::setEnrollStatus(const EnrollStatus &status, const qint32 code)
{
    m_enrollStatus = status;
    Q_EMIT enrollStatusChanged(status, code);
}

void FaceModel::setUserFaces(const QString &user, const QStringList &faces)
{
    if (m_userFaces.value(user) == faces)
        return;

    m_userFaces[user] = faces;
    Q_EMIT userFacesChanged(user, faces);
}

QStringList FaceModel::userFaces(const QString &user) const
{
    return m_userFaces.value(user);
}

void FaceModel::cleanUserFaces(const QString &user)
{
    m_userFaces.remove(user);
    Q_EMIT userFacesChanged(user, QStringList());
}

}
}
