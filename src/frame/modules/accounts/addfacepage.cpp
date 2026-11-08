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

#include "addfacepage.h"
#include "widgets/translucentframe.h"
#include "widgets/labels/tipslabel.h"

#include <QVBoxLayout>
#include <QDebug>

using namespace dcc;
using namespace dcc::accounts;
using namespace dcc::widgets;

AddFacePage::AddFacePage(QWidget *parent)
    : ContentWidget(parent)
    , m_buttonTuple(new ButtonTuple)
    , m_tip(new TipsLabel)
{
    setTitle(tr("Add Face"));

    m_tip->setWordWrap(true);
    m_tip->setAlignment(Qt::AlignHCenter);
    m_tip->setText(guidanceText(HowdyLookingCamera));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    layout->addSpacing(40);
    layout->addWidget(m_tip);
    layout->addSpacing(20);
    layout->addWidget(m_buttonTuple);
    layout->addStretch();

    m_buttonTuple->leftButton()->setText(tr("Re-enter"));
    m_buttonTuple->rightButton()->setText(tr("Done"));
    m_buttonTuple->setVisible(false);

    TranslucentFrame *w = new TranslucentFrame;
    w->setLayout(layout);

    setContent(w);

    connect(m_buttonTuple->leftButton(), &QPushButton::clicked, this, &AddFacePage::reEnrollStart);
    connect(m_buttonTuple->rightButton(), &QPushButton::clicked, this, &AddFacePage::saveFace);
}

void AddFacePage::setFaceModel(FaceModel *model)
{
    m_model = model;

    connect(model, &FaceModel::enrollStatusChanged, this, &AddFacePage::onEnrollStatusChanged);
    onEnrollStatusChanged(model->enrollStatus(), HowdyLookingCamera);
}

void AddFacePage::setUsername(const QString &name)
{
    m_username = name;
}

void AddFacePage::saveFace()
{
    Q_EMIT requestSaveFace(m_username);

    Q_EMIT back();
}

void AddFacePage::reEnrollStart()
{
    m_buttonTuple->setVisible(false);

    Q_EMIT requestReEnrollStart();
}

void AddFacePage::onEnrollStatusChanged(FaceModel::EnrollStatus status, qint32 code)
{
    if (status == FaceModel::Ready) {
        m_tip->setText(guidanceText(HowdyLookingCamera));
        return;
    }

    if (status == FaceModel::Guidance) {
        m_tip->setText(guidanceText(code));
        return;
    }

    if (status == FaceModel::Finished) {
        m_tip->setText(tr("Added successfully"));
        m_buttonTuple->setVisible(true);
        return;
    }

    if (status == FaceModel::Failed) {
        m_tip->setText(guidanceText(code));
        m_buttonTuple->setVisible(true);
        return;
    }
}

QString AddFacePage::guidanceText(qint32 code) const
{
    switch (code) {
    case HowdyNoFace:
        return tr("No face detected, please look at the camera");
    case HowdyMultipleFaces:
        return tr("Multiple faces detected, please make sure you are the only one in the frame");
    case HowdyDarkFrames:
        return tr("The camera only sees dark frames, please check the infrared emitter or the ambient light");
    case HowdyError:
        return tr("Enroll failed, please try again");
    case HowdyLookingCamera:
    default:
        return tr("Please look at the camera");
    }
}
