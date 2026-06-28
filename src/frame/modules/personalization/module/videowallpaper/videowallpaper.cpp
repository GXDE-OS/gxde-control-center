#include "videowallpaper.h"
#include "videowallpapermodel.h"
#include "widgets/nextpagewidget.h"
#include "widgets/labels/tipslabel.h"
#include "widgets/settingsgroup.h"
#include "../../personalizationmodel.h"
#include "widgets/dccslider.h"

#include "videothumbnail.h"
#include "videothumbnailview.h"

#include <QLineEdit>
#include <QPixmap>
#include <QDir>

using namespace dcc;
using namespace dcc::widgets;
using namespace dcc::personalization;

dcc::personalization::VideoWallpaper::VideoWallpaper(QWidget *parent)
    : ContentWidget{parent}
{
    m_mainlayout         = new QVBoxLayout;
    m_widget             = new TranslucentFrame;

    SettingsGroup* settingGroup = new SettingsGroup();

    m_mainlayout->addWidget(settingGroup);

    m_enableSwitch = new SwitchWidget("Video Wallpaper");

    m_videoWallpaperSettingGroup = new QFrame();
    m_videoWallpaperChooseWidget = new FileChooseWidget();
    m_playButton = new QPushButton(tr("Play"));
    m_pauseButton = new QPushButton(tr("Pause"));
    m_volumeSetting = new TitledSliderItem("Volume");
    m_moreSettingButton = new NextPageWidget();

    m_videoWallpaperChooseWidget->setTitle(tr("Choose wallpaper path:"));
    m_videoWallpaperChooseWidget->setType(tr("Video (*.mp4);;All files(*.*)"));
    m_volumeSetting->slider()->setRange(0, 100);
    m_moreSettingButton->setTitle(tr("More Settings"));

    connect(m_enableSwitch, &SwitchWidget::checkedChanged, this, &VideoWallpaper::EnableOptionChange);
    connect(m_playButton, &QPushButton::clicked, this, &VideoWallpaper::Play);
    connect(m_pauseButton, &QPushButton::clicked, this, [](){VideoWallpaperModel().pause();});
    connect(m_volumeSetting->slider(), &QSlider::valueChanged, this, &VideoWallpaper::SetVolumeTip);
    connect(m_volumeSetting->slider(), &QSlider::valueChanged, this, &VideoWallpaper::SetVolume);
    connect(m_moreSettingButton, &NextPageWidget::clicked, this, [](){VideoWallpaperModel().activeWindow();});

    // 并列显示按钮
    mediaControl = new QHBoxLayout;
    mediaControl->addWidget(m_playButton);
    mediaControl->addWidget(m_pauseButton);

    settingGroup->appendItem(m_enableSwitch);
    settingGroup->appendItem(m_videoWallpaperChooseWidget);
    m_mainlayout->addLayout(mediaControl);
    settingGroup->appendItem(m_moreSettingButton);
    m_mainlayout->addWidget(new TipsLabel(tr("Press \"Play\" button to set new wallpaper\nPower by fantascene-dynamic-wallpaper")));

    /*VideoThumbnailView *view = new VideoThumbnailView();
    view->setVideoDirList(QStringList() << "/usr/share/fantascene-dynamic-wallpaper/normal/"
                          << QDir::homePath() + "/Videos/fantascene/");
    m_mainlayout->addWidget(view);*/

    m_mainlayout->addSpacing(10);
    m_mainlayout->setContentsMargins(0, 0, 0, 0);
    m_mainlayout->setSpacing(10);

    m_widget->setLayout(m_mainlayout);
    setTitle(tr("Video Wallpaper"));
    setContent(m_widget);

    // 加载设置
    LoadSetting();

}

void VideoWallpaper::LoadSetting()
{
    VideoWallpaperModel setting;
    m_videoWallpaperSettingGroup->setEnabled(setting.isVideoWallpaperEnabled());
    m_enableSwitch->setChecked(setting.isVideoWallpaperEnabled());
}

void VideoWallpaper::setModel(PersonalizationModel *const model)
{
    //m_fontSizeWidget->setModel(model->getFontSizeModel());
    //m_fontSettingsWidget->setModel(model);
}

void VideoWallpaper::OpenMoreSettingsWindow(){
    VideoWallpaperModel().activeWindow();
}

void VideoWallpaper::SetVolumeTip(int volume)
{
    m_volumeSetting->setValueLiteral(QString::number(volume) + "%");
}

void VideoWallpaper::SetVolume(int volume)
{
    VideoWallpaperModel volumeSetting;
    volumeSetting.setVolume(volume);
}

void VideoWallpaper::Play()
{
    QString videoPath = m_videoWallpaperChooseWidget->edit()->text();
    VideoWallpaperModel volumeSetting;
    if(videoPath != ""){
        volumeSetting.setFile(videoPath);
    }
    volumeSetting.play();
}

void VideoWallpaper::EnableOptionChange()
{
    VideoWallpaperModel setting;
    setting.setVideoWallpaperEnabled(m_enableSwitch->checked());
    m_enableSwitch->setChecked(setting.isVideoWallpaperEnabled());
    m_videoWallpaperSettingGroup->setEnabled(setting.isVideoWallpaperEnabled());

}
