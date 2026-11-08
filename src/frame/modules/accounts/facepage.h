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

#ifndef FACEPAGE_H
#define FACEPAGE_H

#include "widgets/contentwidget.h"
#include "user.h"
#include "facemodel.h"

namespace dcc {
namespace widgets {
class SettingsGroup;
}

namespace accounts {

class FacePage : public ContentWidget
{
    Q_OBJECT
public:
    explicit FacePage(User *user, QWidget *parent = nullptr);

    void setFaceModel(FaceModel *model);

Q_SIGNALS:
    void requestAddFace(const QString &name);
    void requestDeleteFace(const QString &name, const QString &chara);
    void requestCleanFaces(User *user);

private Q_SLOTS:
    void onFacesChanged(const QString &user, const QStringList &faces);
    void addFace();

private:
    widgets::SettingsGroup *m_listGrp;
    User *m_user;
    FaceModel *m_model;
    QPushButton *m_addBtn;
    QPushButton *m_cleanBtn;
    QStringList m_faces;
};

}
}

#endif // FACEPAGE_H
