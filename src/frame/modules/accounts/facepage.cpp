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

#include "facepage.h"
#include "widgets/settingsgroup.h"
#include "widgets/settingsitem.h"
#include "widgets/settingsheaderitem.h"
#include "widgets/labels/tipslabel.h"
#include "widgets/translucentframe.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <dimagebutton.h>

DWIDGET_USE_NAMESPACE

using namespace dcc;
using namespace dcc::widgets;
using namespace dcc::accounts;

FacePage::FacePage(User *user, QWidget *parent)
    : ContentWidget(parent)
    , m_listGrp(new SettingsGroup(tr("Face List")))
    , m_user(user)
{
    setTitle(tr("Face Password"));

    m_addBtn = new QPushButton;
    m_addBtn->setText(tr("Add Face"));

    m_cleanBtn = new QPushButton;
    m_cleanBtn->setText(tr("Clear Face"));
    m_cleanBtn->setObjectName("FingerClean");

    TipsLabel *tip = new TipsLabel;
    tip->setText(tr("Face password is used in system login, application authorization and other scenes. Look at the camera when password needed."));
    tip->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    layout->addWidget(m_listGrp);
    layout->addWidget(m_cleanBtn);
    layout->addWidget(m_addBtn);
    layout->addWidget(tip);
    layout->addStretch();

    TranslucentFrame *w = new TranslucentFrame;
    w->setLayout(layout);

    setContent(w);

    connect(m_addBtn, &QPushButton::clicked, this, &FacePage::addFace);
    connect(m_cleanBtn, &QPushButton::clicked, this, [this] { Q_EMIT requestCleanFaces(m_user); });
}

void FacePage::setFaceModel(FaceModel *model)
{
    m_model = model;

    connect(model, &FaceModel::userFacesChanged, this, &FacePage::onFacesChanged);

    onFacesChanged(m_user->name(), model->userFaces(m_user->name()));
}

void FacePage::onFacesChanged(const QString &user, const QStringList &faces)
{
    if (user != m_user->name())
        return;

    m_faces = faces;
    m_listGrp->clear();

    int i = 1;
    for (const QString &chara : faces) {
        widgets::SettingsItem *item = new widgets::SettingsItem;
        item->setFixedHeight(36);

        QHBoxLayout *layout = new QHBoxLayout;
        layout->setContentsMargins(20, 0, 10, 0);

        QLabel *title = new QLabel(tr("Face") + QString::number(i++));
        DImageButton *removeBtn = new DImageButton(
            ":/accounts/themes/common/icons/avator_delete_normal.svg",
            ":/accounts/themes/common/icons/avator_delete_hover.svg",
            ":/accounts/themes/common/icons/avator_delete_press.svg");

        layout->addWidget(title);
        layout->addStretch();
        layout->addWidget(removeBtn);
        item->setLayout(layout);

        connect(removeBtn, &DImageButton::clicked, this, [this, chara] {
            Q_EMIT requestDeleteFace(m_user->name(), chara);
        });

        m_listGrp->appendItem(item);
    }

    const bool empty = faces.isEmpty();
    m_addBtn->setVisible(true);
    m_listGrp->headerItem()->setVisible(!empty);
    m_cleanBtn->setVisible(!empty);
}

void FacePage::addFace()
{
    Q_EMIT requestAddFace(m_user->name());
}
