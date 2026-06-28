#include "videowallpaperchoose.h"
#include "widgets/titledslideritem.h"
#include "widgets/dccslider.h"

#include <QVBoxLayout>

using namespace dcc;
using namespace dcc::personalization;
using namespace dcc::widgets;

dcc::personalization::VideoWallpaperChoose::VideoWallpaperChoose(QWidget *parent)
    : TranslucentFrame(parent)
{
    m_mainlayout = new QVBoxLayout;
    m_mainWidget = new SettingsGroup;

    m_mainWidget->appendItem(m_sizeWidget);
    m_mainlayout->addWidget(m_mainWidget);

    m_mainlayout->setSpacing(0);
    m_mainlayout->setContentsMargins(0, 0, 0, 0);

    setLayout(m_mainlayout);
    //m_sizeWidget = new TitledSliderItem(tr("Size"));
}
