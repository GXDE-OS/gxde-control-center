#include "vncremote.h"
#include "widgets/labels/tipslabel.h"
#include "widgets/settingsgroup.h"

#include <QLineEdit>
#include <QDir>
#include <QProcess>
#include <QDBusConnection>

#define CHROOTDBUS_DESTINATION "com.gxde.daemon.system.info"
#define CHROOTDBUS_PATH "/com/gxde/daemon/system/info"
#define CHROOTDBUS_INTERFACE "com.gxde.daemon.system.info"

using namespace dcc;
using namespace dcc::widgets;
using namespace dcc::display;

dcc::display::VNCRemote::VNCRemote(QWidget *parent)
    : ContentWidget{parent}
{
    m_mainlayout         = new QVBoxLayout;
    m_widget             = new TranslucentFrame;

    SettingsGroup *group = new SettingsGroup();

    m_enableSwitch = new SwitchWidget("VNC Remote");
    group->appendItem(m_enableSwitch); 

    // 连接密码设置
    QVBoxLayout *passwordControlLayout = new QVBoxLayout();

    passwordTips = new TipsLabel(tr("Connect Password:"));

    //统一输入框样式
    passwordFirst = new PasswdEditWidget();
    passwordAgain = new PasswdEditWidget();

    SettingsGroup *passwordGroup = new SettingsGroup();
    passwordGroup->appendItem(passwordFirst);
    passwordGroup->appendItem(passwordAgain);

    passwordFirst->setTitle(tr("New Password"));
    passwordAgain->setTitle("Repeat Password");
    
    passwordControlLayout->addWidget(passwordTips);
    passwordControlLayout->addWidget(group); 
    passwordControlLayout->addWidget(passwordGroup);  

    removePasswordButton = new QPushButton(tr("Remove Password"));
    setPasswordButton = new QPushButton(tr("Set"));

    passwordSettingStatus = new TipsLabel("");

    // passwordControlLayout->addWidget(passwordFirst);
    // passwordControlLayout->addWidget(passwordAgain);
    passwordControlLayout->addWidget(setPasswordButton);
    passwordControlLayout->addWidget(removePasswordButton);
    passwordControlLayout->addWidget(passwordSettingStatus);

    // 加载设置
    LoadSetting();

    //connect(m_enableSwitch, &SwitchWidget::checkedChanged, this, &VNCRemote::EnableOptionChange);
    connect(m_enableSwitch, &SwitchWidget::checkedChanged, this, &VNCRemote::SetVNCEnabled);
    connect(setPasswordButton, &QPushButton::clicked, this, &VNCRemote::SetVNCPassword);
    connect(removePasswordButton, &QPushButton::clicked, this, &VNCRemote::RemovePassword);

    m_mainlayout->addLayout(passwordControlLayout);
    m_mainlayout->addWidget(new TipsLabel(tr("You can use IP address and VNC View to connect.\nPort: 5900\nPower by x11vnc")));
    m_mainlayout->addSpacing(10);
    m_mainlayout->setContentsMargins(0, 0, 0, 0);
    m_mainlayout->setSpacing(10);

    setTitle(tr("VNC Remote"));
    m_widget->setLayout(m_mainlayout);
    setContent(m_widget);
}

void VNCRemote::RestartX11VNC()
{
    system("killall x11vnc -9");
    QDBusMessage dbus = QDBusMessage::createMethodCall(CHROOTDBUS_DESTINATION,
                                                       CHROOTDBUS_PATH,
                                                       CHROOTDBUS_INTERFACE,
                                                       "IsInChroot");
    QDBusMessage res = QDBusConnection::sessionBus().call(dbus);
    bool isChroot = false;
    if (res.arguments().count() > 0) {
        isChroot = res.arguments().first().toBool();
    }
    if(QFile::exists(QDir::homePath() + "/.vnc/passwd")) {
        if (isChroot) {
            // 小小电脑/Chroot 下因为环境限制，不支持 XShm
            // 需要禁用该部分以正确启用 VNC
            system("setsid x11vnc --forever -rfbauth -noshm ~/.vnc/passwd &");
        }
        else {
            system("setsid x11vnc --forever -rfbauth ~/.vnc/passwd &");
        }
    }
    else {
        if (isChroot) {
            system("setsid x11vnc --forever -noshm &");
        }
        else {
            system("setsid x11vnc --forever &");
        }
    }
}

void VNCRemote::RemovePassword()
{
    QFile::remove(QDir::homePath() + "/.vnc/passwd");
    passwordSettingStatus->setText("Removed");
    RestartX11VNC();
}

void VNCRemote::SetVNCEnabled()
{
    if(!QDir(QDir::homePath() + "/.config/GXDE").exists()){
        QDir(QDir::homePath() + "/.config/GXDE").mkpath(QDir::homePath() + "/.config/GXDE");
    }
    if(m_enableSwitch->checked()) {
        QFile file(QDir::homePath() + "/.config/GXDE/gxde-x11vnc");
        file.open(QFile::WriteOnly);
        file.write("Enabled");
        file.close();
        RestartX11VNC();
    }
    else {
        QFile::remove(QDir::homePath() + "/.config/GXDE/gxde-x11vnc");
        system("killall x11vnc -9");
    }
    EnableOptionChange();
}

void VNCRemote::LoadSetting()
{
    m_enableSwitch->setChecked(QFile::exists(QDir::homePath() + "/.config/GXDE/gxde-x11vnc"));
    EnableOptionChange();
}

void VNCRemote::setModel(DisplayModel *const model)
{

}

void VNCRemote::SetVNCPassword()
{
    QProcess process;
    process.start("x11vnc", QStringList() << "-storepasswd");
    process.waitForStarted();
    process.write((passwordFirst->text() + "\n" + passwordAgain->text() + "\ny\n").toUtf8());
    process.waitForFinished();
    if(process.exitCode() != 0) {
        passwordSettingStatus->setText("Setting Error!");
    }
    else {
        passwordSettingStatus->setText("Done");
        RestartX11VNC();
    }
    process.close();
}

void VNCRemote::EnableOptionChange()
{
    if(m_enableSwitch->checked()) {
        passwordFirst->setEnabled(true);
        passwordAgain->setEnabled(true);
        setPasswordButton->setEnabled(true);
        removePasswordButton->setEnabled(true);
        passwordSettingStatus->setEnabled(true);
    }
    else {
        passwordFirst->setDisabled(true);
        passwordAgain->setDisabled(true);
        setPasswordButton->setDisabled(true);
        removePasswordButton->setDisabled(true);
        passwordSettingStatus->setDisabled(true);
    }
}

