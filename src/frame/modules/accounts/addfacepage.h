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

#ifndef ADDFACEPAGE_H
#define ADDFACEPAGE_H

#include "widgets/contentwidget.h"
#include "widgets/buttontuple.h"
#include "facemodel.h"

#include <QWidget>

namespace dcc {
namespace widgets {
class TipsLabel;
}

namespace accounts {
class AddFacePage : public ContentWidget
{
    Q_OBJECT
public:
    explicit AddFacePage(QWidget *parent = nullptr);

    void setFaceModel(FaceModel *model);
    void setUsername(const QString &name);

Q_SIGNALS:
    void requestSaveFace(const QString &name);
    void requestReEnrollStart();

private Q_SLOTS:
    void saveFace();
    void reEnrollStart();
    void onEnrollStatusChanged(FaceModel::EnrollStatus status, qint32 code);

private:
    QString guidanceText(qint32 code) const;

private:
    FaceModel *m_model;
    widgets::ButtonTuple *m_buttonTuple;
    widgets::TipsLabel *m_tip;
    QString m_username;
};
}
}

#endif // ADDFACEPAGE_H
