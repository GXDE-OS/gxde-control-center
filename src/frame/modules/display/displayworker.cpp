/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
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

#include "displayworker.h"
#include "displaymodel.h"
#include "monitorsettingdialog.h"
#include "wayland/gxdescreen.h"
#include "widgets/utils.h"

#include <QDebug>
#include <QTimer>
#include <limits>

using namespace dcc;
using namespace dcc::display;

#define GSETTINGS_MINIMUM_BRIGHTNESS    "brightness-minimum"

const QString DisplayInterface("com.deepin.daemon.Display");

DisplayWorker::DisplayWorker(DisplayModel *model, QObject *parent)
    : QObject(parent),

      m_model(model),
      m_displayInter(DisplayInterface, "/com/deepin/daemon/Display", QDBusConnection::sessionBus(), this),
      m_dccSettings(new QGSettings("com.deepin.dde.control-center", QByteArray(), this)),
      m_appearanceInter(new AppearanceInter("com.deepin.daemon.Appearance",
                                      "/com/deepin/daemon/Appearance",
                                      QDBusConnection::sessionBus(), this)),
      m_isGxde(GxdeScreen::isAvailable())
{
    // TODO:
    model->setPrimary(m_displayInter.primary());

    m_displayInter.setSync(false);
    m_appearanceInter->setSync(false);

    connect(&m_displayInter, &DisplayInter::MonitorsChanged, this, &DisplayWorker::onMonitorListChanged);
    connect(&m_displayInter, &DisplayInter::BrightnessChanged, this, &DisplayWorker::onMonitorsBrightnessChanged);
    connect(&m_displayInter, &DisplayInter::BrightnessChanged, model, &DisplayModel::setBrightnessMap);
    connect(&m_displayInter, &DisplayInter::ScreenHeightChanged, model, &DisplayModel::setScreenHeight);
    connect(&m_displayInter, &DisplayInter::ScreenWidthChanged, model, &DisplayModel::setScreenWidth);
    connect(&m_displayInter, &DisplayInter::DisplayModeChanged, model, &DisplayModel::setDisplayMode);
    connect(&m_displayInter, &DisplayInter::CurrentCustomIdChanged, model, &DisplayModel::setCurrentConfig);
    connect(&m_displayInter, &DisplayInter::CustomIdListChanged, model, &DisplayModel::setConfigList);
//    connect(&m_displayInter, &DisplayInter::HasCustomConfigChanged, model, &DisplayModel::setHasConfig);
    connect(&m_displayInter, static_cast<void (DisplayInter::*)(const QString &) const>(&DisplayInter::PrimaryChanged), model, &DisplayModel::setPrimary);

    onMonitorListChanged(m_displayInter.monitors());
    onMonitorsBrightnessChanged(m_displayInter.brightness());
    model->setScreenHeight(m_displayInter.screenHeight());
    model->setScreenWidth(m_displayInter.screenWidth());
    model->setConfigList(m_displayInter.customIdList());
    model->setCurrentConfig(m_displayInter.currentCustomId());
//    model->setHasConfig(m_displayInter.hasCustomConfig());
    model->setDisplayMode(m_displayInter.displayMode());
    refreshGxdeState();

    const bool isRedshiftValid = QProcess::execute("which", QStringList() << "redshift") == 0;

    if (isRedshiftValid)
        updateNightModeStatus();

    m_model->setRedshiftIsValid(isRedshiftValid);
    m_model->setMinimumBrightnessScale(m_dccSettings->get(GSETTINGS_MINIMUM_BRIGHTNESS).toDouble());
}

DisplayWorker::~DisplayWorker()
{
    qDeleteAll(m_monitors.keys());
    qDeleteAll(m_monitors.values());
}

int DisplayWorker::currentMonitorModeId(Monitor* monitor) const {
    if (!monitor) {
        return -1;
    }

    for (const GxdeScreen::Output& output : GxdeScreen::outputs()) {
        if (output.name != monitor->name()) {
            continue;
        }

        int bestMode = -1;
        int bestRefreshDelta = std::numeric_limits<int>::max();
        for (const Resolution &mode : monitor->modeList()) {
            if (mode.width() != output.width || mode.height() != output.height) {
                continue;
            }

            const int refreshDelta =
                qAbs(qRound(mode.rate() * 1000.0) - output.refresh);
            if (refreshDelta < bestRefreshDelta) {
                bestRefreshDelta = refreshDelta;
                bestMode = mode.id();
            }
        }

        if (bestMode >= 0) {
            return bestMode;
        }

        break;
    }

    return monitor->currentMode().id();
}

DisplayWorker::OutputModeState DisplayWorker::currentOutputMode(
        Monitor* monitor) const {
    OutputModeState state;
    if (!monitor) {
        return state;
    }

    for (const GxdeScreen::Output& output : GxdeScreen::outputs()) {
        if (output.name != monitor->name()) {
            continue;
        }

        state.output = output.name;
        state.width = output.width;
        state.height = output.height;
        state.refresh = output.refresh;
        state.valid = output.enabled && output.width > 0 && output.height > 0
            && output.refresh > 0;
        break;
    }
    return state;
}

bool DisplayWorker::restoreOutputMode(Monitor* monitor,
        const OutputModeState &state) {
    if (!state.valid) {
        return false;
    }

    // wlcom exposes refresh in mHz, DBus sets in Hz.
    const int refreshHz = qMax(1, qRound(state.refresh / 1000.0));
    const bool restored = GxdeScreen::setResolution(
        state.output, state.width, state.height, refreshHz);
    qInfo() << "(Display) Output: Restore output mode"
        << state.output << state.width << state.height << state.refresh
        << "| result ->" << restored;

    if (!restored) {
        return false;
    }

    if (monitor) {
        int bestRefreshDelta = std::numeric_limits<int>::max();
        Resolution restoredMode;
        bool foundMode = false;
        for (const Resolution &mode : monitor->modeList()) {
            if (mode.width() != state.width || mode.height() != state.height) {
                continue;
            }

            const int refreshDelta =
                qAbs(qRound(mode.rate() * 1000.0) - state.refresh);
            if (refreshDelta < bestRefreshDelta) {
                bestRefreshDelta = refreshDelta;
                restoredMode = mode;
                foundMode = true;
            }
        }
        if (foundMode) {
            monitor->setCurrentMode(restoredMode);
        }
    }

    refreshGxdeState();
    return true;
}

void DisplayWorker::active()
{
    m_model->setAllowEnableMultiScaleRatio(
        valueByQSettings<bool>(DCC_CONFIG_FILES,
                               "Display",
                               "AllowEnableMultiScaleRatio",
                               false)
    );

    QDBusPendingCallWatcher *scalewatcher = new QDBusPendingCallWatcher(m_appearanceInter->GetScaleFactor());
    connect(scalewatcher, &QDBusPendingCallWatcher::finished, this, &DisplayWorker::onGetScaleFinished);

    QDBusPendingCallWatcher *screenscaleswatcher = new QDBusPendingCallWatcher(m_appearanceInter->GetScreenScaleFactors());
    connect(screenscaleswatcher, &QDBusPendingCallWatcher::finished, this, &DisplayWorker::onGetScreenScalesFinished);
}

void DisplayWorker::saveChanges()
{
    qDebug() << Q_FUNC_INFO;

    if (m_isGxde) {
        m_gxdeSnapshot.clear();
        return;
    }
    m_displayInter.Save().waitForFinished();
}

void DisplayWorker::discardChanges()
{
    qDebug() << Q_FUNC_INFO;

    if (m_isGxde) {
        restoreGxdeSnapshot();
        return;
    }
    m_displayInter.ResetChanges().waitForFinished();
}

void DisplayWorker::mergeScreens()
{
    qDebug() << Q_FUNC_INFO;

    m_model->setIsMerge(true);

    if (m_isGxde) {
        if (!GxdeScreen::setMode(0)) {
            qWarning() << "(GXDE) Display: Failed to set duplicate mode!!";
        }

        refreshGxdeState();
        return;
    }

    // TODO: make asynchronous
    const QList<Resolution> commonModes = m_displayInter.ListOutputsCommonModes();
    Q_ASSERT(!commonModes.isEmpty());

    const auto mode = commonModes.first();
    const auto rotate = m_model->primaryMonitor()->rotate();
    const auto brightness = m_model->primaryMonitor()->brightness();

    QList<QDBusPendingReply<>> replys;

    for (auto *mon : m_model->monitorList())
    {
        auto *mInter = m_monitors[mon];
        Q_ASSERT(mInter);

        replys << mInter->SetPosition(0, 0);
        replys << mInter->SetModeBySize(mode.width(), mode.height());
        replys << mInter->SetRotation(rotate);
        replys << m_displayInter.SetBrightness(mon->name(), brightness);
    }

    for (auto r : replys)
        r.waitForFinished();

    m_displayInter.ApplyChanges().waitForFinished();
}

void DisplayWorker::splitScreens()
{
    qDebug() << Q_FUNC_INFO;

    m_model->setIsMerge(false);

    if (m_isGxde) {
        if (!GxdeScreen::setMode(1))
            qWarning() << "(GXDE) Display: Failed to set extend mode!!";
        refreshGxdeState();
        return;
    }

    const auto mList = m_model->monitorList();
    Q_ASSERT(mList.size() == 2);

    auto *primary = m_model->primaryMonitor();
    Q_ASSERT(m_monitors.contains(primary));
    m_monitors[primary]->SetPosition(0, 0).waitForFinished();

    int xOffset = primary->w();
    for (auto *mon : mList)
    {
        // pass primary
        if (mon == primary)
            continue;

        Q_ASSERT(m_monitors.contains(mon));
        auto *mInter = m_monitors[mon];

        mInter->SetPosition(xOffset, 0).waitForFinished();
        xOffset += mon->w();
    }

    m_displayInter.ApplyChanges();
}

void DisplayWorker::duplicateMode()
{
    switchMode(MERGE_MODE);
    saveChanges();
}

void DisplayWorker::extendMode()
{
    switchMode(EXTEND_MODE);
    saveChanges();
}

void DisplayWorker::onlyMonitor(const QString &monName)
{
    switchMode(SINGLE_MODE, monName);
}

void DisplayWorker::createConfig(const QString &config)
{
    const auto reply = m_displayInter.SwitchMode(CUSTOM_MODE, config);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply);
    watcher->setProperty("ConfigName", config);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, &DisplayWorker::onCreateConfigFinshed);
}

void DisplayWorker::switchConfig(const QString &config)
{
    switchMode(CUSTOM_MODE, config);
}

void DisplayWorker::deleteConfig(const QString &config)
{
    m_displayInter.DeleteCustomMode(config);
}

void DisplayWorker::modifyConfigName(const QString &oldName, const QString &newName)
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_displayInter.ModifyConfigName(oldName, newName));

    connect(w, &QDBusPendingCallWatcher::finished, this, &DisplayWorker::onModifyConfigNameFinished);
}

void DisplayWorker::switchMode(const int mode, const QString &name)
{
    qDebug() << Q_FUNC_INFO << mode << name;

    uint gxdeMode = 0;
    bool supportedByGxde = true;
    switch (mode) {
    case MERGE_MODE:
        gxdeMode = 0;
        break;
    case EXTEND_MODE:
        gxdeMode = 1;
        break;
    case SINGLE_MODE:
        gxdeMode = 2;
        break;
    default:
        supportedByGxde = false;
        break;
    }
    if (supportedByGxde && m_isGxde) {
        if (!GxdeScreen::setMode(gxdeMode, name)) {
            qWarning() << "(Display) Switcher: Failed to switch mode"
                << gxdeMode << name;
            return;
        }
        refreshGxdeState();
        return;
    }

    m_displayInter.SwitchMode(mode, name).waitForFinished();
}

void DisplayWorker::onMonitorListChanged(const QList<QDBusObjectPath> &mons)
{
    QList<QString> ops;
    for (const auto *mon : m_monitors.keys())
        ops << mon->path();

    QList<QString> pathList;
    for (const QDBusObjectPath& op : mons) {
        const QString path = op.path();
        if (pathList.contains(path))
            continue;
        pathList << path;
        if (!ops.contains(path)) {
            monitorAdded(path);
            ops << path;
        }
    }

    for (const QString& op : ops) {
        if (!pathList.contains(op)) {
            monitorRemoved(op);
        }
    }
}

void DisplayWorker::onMonitorsBrightnessChanged(const BrightnessMap &brightness)
{
    if (brightness.isEmpty()) return;

    for (auto it = m_monitors.begin(); it != m_monitors.end(); ++it) {
        it.key()->setBrightness(brightness[it.key()->name()]);
    }
}

void DisplayWorker::onModifyConfigNameFinished(QDBusPendingCallWatcher *w)
{
    w->deleteLater();
}

void DisplayWorker::onGetScaleFinished(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<double> reply = w->reply();

    m_model->setUIScale(reply);

    w->deleteLater();
}

void DisplayWorker::onGetScreenScalesFinished(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<QMap<QString,double>> reply = w->reply();
    QMap<QString,double> rmap = reply;

    for (auto& m : m_model->monitorList()){
        if (rmap.find(m->name()) != rmap.end()){
            m->setScale(rmap[m->name()]);
        }
    }

    w->deleteLater();
}

void DisplayWorker::onCreateConfigFinshed(QDBusPendingCallWatcher *w)
{
    const QString &name = w->property("ConfigName").toString();

    Q_EMIT m_model->configCreated(name);

    w->deleteLater();
}

#ifndef DCC_DISABLE_ROTATE
void DisplayWorker::setMonitorRotate(Monitor *mon, const quint16 rotate)
{
    const int angle = GxdeScreen::rotationToAngle(rotate);
    const bool gxdeRotated =
        angle >= 0 && (GxdeScreen::setRotation(mon->name(), angle) ||
                       (mon == m_model->primaryMonitor() && GxdeScreen::setRotation(angle)));
    if (gxdeRotated) {
        mon->setRotate(rotate);
        return;
    }

    MonitorInter *inter = m_monitors.value(mon);
    Q_ASSERT(inter);

    inter->SetRotation(rotate).waitForFinished();
    m_displayInter.ApplyChanges();
}

void DisplayWorker::setMonitorRotateAll(const quint16 rotate)
{
    const int angle = GxdeScreen::rotationToAngle(rotate);
    bool allRotated = angle >= 0 && !m_model->monitorList().isEmpty();
    if (allRotated) {
        for (Monitor *monitor : m_model->monitorList())
            allRotated = GxdeScreen::setRotation(monitor->name(), angle) && allRotated;
    }
    if (allRotated) {
        for (Monitor *monitor : m_model->monitorList())
            monitor->setRotate(rotate);
        return;
    }
    if (angle >= 0 && GxdeScreen::setRotation(angle)) {
        if (Monitor *primary = m_model->primaryMonitor())
            primary->setRotate(rotate);
    }

    for (auto *mi : m_monitors)
        mi->SetRotation(rotate).waitForFinished();

    m_displayInter.ApplyChanges();
}
#endif

void DisplayWorker::setPrimary(const int index) {
    if (index < 0 || index >= m_model->monitorList().size()) {
        return;
    }

    Monitor* monitor = m_model->monitorList()[index];
    if (m_isGxde) {
        if (GxdeScreen::setPrimary(monitor->name())) {
            refreshGxdeState();
        } else {
            qWarning() << "(Display) PriScr: Failed to set primary"
                << monitor->name();
        }
        return;
    }
    m_displayInter.SetPrimary(monitor->name());
}

void DisplayWorker::setMonitorEnable(Monitor *mon, const bool enabled)
{
    if (!mon)
        return;

    if (m_isGxde) {
        if (!GxdeScreen::setEnabled(mon->name(), enabled))
            qWarning() << "(GXDE display) failed to set output enabled"
                       << mon->name() << enabled;
        refreshGxdeState();
        return;
    }

    MonitorInter *inter = m_monitors.value(mon);
    Q_ASSERT(inter);

    inter->Enable(enabled).waitForFinished();
    m_displayInter.ApplyChanges();
}

void DisplayWorker::applyChanges()
{
    m_displayInter.ApplyChanges().waitForFinished();
}

void DisplayWorker::setMonitorResolution(Monitor *mon, const int mode)
{
    const QList<Resolution> modes = mon->modeList();
    for (const Resolution &resolution : modes) {
        if (resolution.id() != mode)
            continue;

        if (setMonitorResolutionBySize(
                mon, mode, resolution.width(), resolution.height(),
                qRound(resolution.rate() * 1000.0)))
            return;
        break;
    }

    MonitorInter *inter = m_monitors.value(mon);
    Q_ASSERT(inter);

    inter->SetMode(mode).waitForFinished();
    m_displayInter.ApplyChanges().waitForFinished();
}

bool DisplayWorker::setMonitorResolutionBySize(Monitor *mon, int mode,
        int width, int height, int refresh)
{
    if (!mon || width <= 0 || height <= 0 || refresh <= 0)
        return false;

    // libkywc and ListAllOutputs expose refresh in mHz; the convenience
    // D-Bus API accepts Hz and resolves it to the closest physical mode.
    const int refreshHz = qMax(1, qRound(refresh / 1000.0));
    if (m_isGxde) {
        const bool configured = GxdeScreen::setResolution(
            mon->name(), width, height, refreshHz);
        if (configured)
            refreshGxdeState();
        else
            qWarning() << "(GXDE display) failed to set resolution"
                       << mon->name() << width << height << refreshHz;
        return configured;
    }

    if (mode < 0)
        return false;

    MonitorInter *inter = m_monitors.value(mon);
    if (!inter)
        return false;
    QDBusPendingReply<> reply = inter->SetMode(mode);
    reply.waitForFinished();
    if (reply.isError())
        return false;
    QDBusPendingReply<> applyReply = m_displayInter.ApplyChanges();
    applyReply.waitForFinished();
    return !applyReply.isError();
}

void DisplayWorker::setMonitorBrightness(Monitor *mon, const double brightness)
{
    if (!mon)
        return;

    const double value = std::max(brightness, m_model->minimumBrightnessScale());
    if (m_isGxde) {
        const bool configured = GxdeScreen::setBrightness(mon->name(), value);
        qInfo() << "(GXDE display) set brightness"
                << mon->name() << qRound(value * 100.0)
                << "result=" << configured;
        if (configured)
            mon->setBrightness(value);
        return;
    }

    m_displayInter.SetAndSaveBrightness(mon->name(), value).waitForFinished();
}

void DisplayWorker::setMonitorPosition(Monitor *mon, const int x, const int y)
{
    if (!mon)
        return;

    if (m_isGxde) {
        mon->setX(x);
        mon->setY(y);
        if (!m_layoutUpdatePending) {
            m_layoutUpdatePending = true;
            QTimer::singleShot(0, this, [this] {
                m_layoutUpdatePending = false;
                applyGxdeLayout();
            });
        }
        return;
    }

    MonitorInter *inter = m_monitors.value(mon);
    Q_ASSERT(inter);

    inter->SetPosition(x, y).waitForFinished();
    m_displayInter.ApplyChanges().waitForFinished();
}

void DisplayWorker::setUiScale(const double value)
{
    double rv=value;
    if (rv < 0) rv = m_model->uiScale();

    GxdeScreen::setScale(rv);

    for (auto &mm : m_model->monitorList()) {
        mm->setScale(-1);
    }
    QDBusPendingCall call = m_appearanceInter->SetScaleFactor(rv);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [=] {
        if (call.isError()) {
            Q_EMIT m_model->uiScaleChanged(rv);
            qWarning() << call.error();
        }
        watcher->deleteLater();
    });
}

void DisplayWorker::setIndividualScaling(Monitor *m, const double scaling)
{
    if (m && scaling > 0) {
        m->setScale(scaling);
        GxdeScreen::setScale(m->name(), scaling);
    }

    double primaryscale = m_model->primaryMonitor()->scale();
    GxdeScreen::setScale(primaryscale);
    m_appearanceInter->SetScaleFactor(primaryscale);

    QMap<QString, double> scalemap;
    for (auto& m : m_model->monitorList()){
        if (m->scale() > 0) {
            scalemap[m->name()]=m->scale();
        }
        else {
            scalemap[m->name()]=1;
        }
    }
    m_appearanceInter->SetScreenScaleFactors(scalemap);
}

void DisplayWorker::setNightMode(const bool nightmode)
{
    QProcess *process = new QProcess(this);

    QString cmd;
    QString serverCmd;
    if (nightmode) {
        cmd = "start";
        serverCmd = "enable";
    } else {
        cmd = "stop";
        serverCmd = "disable";
    }

    connect(process, static_cast<void (QProcess::*)(int exitCode, QProcess::ExitStatus)>(&QProcess::finished), this, [=] {
        process->close();
        process->deleteLater();
        // reload
        updateNightModeStatus();
    });

    process->start("bash", QStringList() << "-c" << QString("systemctl --user %1 redshift.service && systemctl --user %2 redshift.service")
                  .arg(serverCmd)
                  .arg(cmd));

    m_model->setRedshiftSetting(true);
}

//void DisplayWorker::loadRotations(Monitor * const mon)
//{
//    MonitorInter *inter = m_monitors.value(mon);
//    Q_ASSERT(inter);

//    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(inter->ListRotations(), this);
//    connect(watcher, &QDBusPendingCallWatcher::finished, [=] (QDBusPendingCallWatcher *watcher) { loadRotationsFinished(mon, watcher); });
//}

//void DisplayWorker::loadRotationsFinished(Monitor * const mon, QDBusPendingCallWatcher *watcher)
//{
//    QDBusPendingReply<RotationList> reply = *watcher;
//    mon->setRotateList(reply.value());
//    watcher->deleteLater();
//}

//void DisplayWorker::loadModes(Monitor * const mon)
//{
//    MonitorInter *inter = m_monitors.value(mon);
//    Q_ASSERT(inter);

//    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(inter->ListModes(), this);
//    connect(watcher, &QDBusPendingCallWatcher::finished, [=] (QDBusPendingCallWatcher *watcher) { loadModesFinished(mon, watcher); });
//}

//void DisplayWorker::loadModesFinished(Monitor * const mon, QDBusPendingCallWatcher *watcher)
//{
//    QDBusPendingReply<ResolutionList> reply = *watcher;
//    mon->setModeList(reply.value());
//    watcher->deleteLater();
//}

void DisplayWorker::monitorAdded(const QString &path)
{
    MonitorInter *inter = new MonitorInter(DisplayInterface, path, QDBusConnection::sessionBus(), this);
    Monitor *mon = new Monitor(this);

    connect(inter, &MonitorInter::XChanged, mon, &Monitor::setX);
    connect(inter, &MonitorInter::YChanged, mon, &Monitor::setY);
    connect(inter, &MonitorInter::WidthChanged, mon, &Monitor::setW);
    connect(inter, &MonitorInter::HeightChanged, mon, &Monitor::setH);
    connect(inter, &MonitorInter::RotationChanged, mon, &Monitor::setRotate);
    connect(inter, &MonitorInter::NameChanged, mon, &Monitor::setName);
    connect(inter, &MonitorInter::CurrentModeChanged, mon, &Monitor::setCurrentMode);
    connect(inter, &MonitorInter::ModesChanged, mon, &Monitor::setModeList);
    connect(inter, &MonitorInter::RotationsChanged, mon, &Monitor::setRotateList);
    connect(&m_displayInter, static_cast<void (DisplayInter::*)(const QString &) const>(&DisplayInter::PrimaryChanged), mon, &Monitor::setPrimary);

    // NOTE: DO NOT using async dbus call. because we need to have a unique name to distinguish each monitor
    Q_ASSERT(inter->isValid());
    mon->setName(inter->name());

    if (m_isGxde) {
        bool presentInWlcom = false;
        for (const GxdeScreen::Output &output : GxdeScreen::outputs()) {
            if (output.name == mon->name()) {
                presentInWlcom = true;
                break;
            }
        }

        bool duplicateName = false;
        for (Monitor *existing : m_monitors.keys()) {
            if (existing->name() == mon->name()) {
                duplicateName = true;
                break;
            }
        }

        if (!presentInWlcom || duplicateName) {
            qWarning() << "(Display) MON: Ignoring stale monitor object"
                << path << mon->name()
                << "present=" << presentInWlcom
                << "duplicate=" << duplicateName;
            delete inter;
            delete mon;
            return;
        }
    }

    inter->setSync(false);

    mon->setPath(path);
    mon->setX(inter->x());
    mon->setY(inter->y());
    mon->setW(inter->width());
    mon->setH(inter->height());
    mon->setRotate(inter->rotation());
    mon->setCurrentMode(inter->currentMode());
    mon->setModeList(inter->modes());
    mon->setRotateList(inter->rotations());
    mon->setPrimary(m_displayInter.primary());

    if (!m_model->brightnessMap().isEmpty()) {
        mon->setBrightness(m_model->brightnessMap()[mon->name()]);
    }

    m_model->monitorAdded(mon);
    m_monitors.insert(mon, inter);
}

void DisplayWorker::monitorRemoved(const QString &path)
{
    Monitor *monitor = nullptr;
    for (auto it(m_monitors.cbegin()); it != m_monitors.cend(); ++it)
    {
        if (it.key()->path() == path)
        {
            monitor = it.key();
            break;
        }
    }
    if (!monitor)
        return;

    m_model->monitorRemoved(monitor);

    m_monitors[monitor]->deleteLater();
    m_monitors.remove(monitor);

    monitor->deleteLater();
}

void DisplayWorker::refreshGxdeState()
{
    const QList<GxdeScreen::Output> outputs = GxdeScreen::outputs();
    if (outputs.isEmpty())
        return;

    BrightnessMap brightness;
    QString primary;
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    int enabledCount = 0;
    bool merged = true;

    for (const GxdeScreen::Output &output : outputs) {
        if (output.primary)
            primary = output.name;
        brightness.insert(output.name, output.brightness / 100.0);
        if (output.enabled) {
            ++enabledCount;
            merged = merged && output.x == 0 && output.y == 0;
            const double scale = output.scale > 0.0 ? output.scale : 1.0;
            const int logicalWidth = qRound(output.width / scale);
            const int logicalHeight = qRound(output.height / scale);
            minX = qMin(minX, output.x);
            minY = qMin(minY, output.y);
            maxX = qMax(maxX, output.x + logicalWidth);
            maxY = qMax(maxY, output.y + logicalHeight);
        }

        for (Monitor *monitor : m_model->monitorList()) {
            if (monitor->name() != output.name)
                continue;
            const double scale = output.scale > 0.0 ? output.scale : 1.0;
            const bool rotated = output.transform % 2 != 0;
            const int logicalWidth = qRound(
                (rotated ? output.height : output.width) / scale);
            const int logicalHeight = qRound(
                (rotated ? output.width : output.height) / scale);
            monitor->setX(output.x);
            monitor->setY(output.y);
            monitor->setW(logicalWidth);
            monitor->setH(logicalHeight);
            monitor->setScale(output.scale);
            monitor->setRotate(GxdeScreen::transformToRotation(output.transform));
            monitor->setBrightness(output.brightness / 100.0);

            int bestRefreshDelta = std::numeric_limits<int>::max();
            Resolution currentMode;
            bool foundCurrentMode = false;
            for (const Resolution &mode : monitor->modeList()) {
                if (mode.width() != output.width ||
                        mode.height() != output.height) {
                    continue;
                }

                const int refreshDelta =
                    qAbs(qRound(mode.rate() * 1000.0) - output.refresh);
                if (refreshDelta < bestRefreshDelta) {
                    bestRefreshDelta = refreshDelta;
                    currentMode = mode;
                    foundCurrentMode = true;
                }
            }
            if (foundCurrentMode)
                monitor->setCurrentMode(currentMode);
            break;
        }
    }

    if (primary.isEmpty()) {
        for (const GxdeScreen::Output &output : outputs) {
            if (output.enabled) {
                primary = output.name;
                break;
            }
        }
    }
    if (!primary.isEmpty()) {
        m_model->setPrimary(primary);
        for (Monitor *monitor : m_model->monitorList())
            monitor->setPrimary(primary);
    }

    m_model->setBrightnessMap(brightness);
    m_model->setScreenWidth(maxX - minX);
    m_model->setScreenHeight(maxY - minY);
    m_model->setIsMerge(enabledCount > 1 && merged);
    m_model->setDisplayMode(enabledCount <= 1 ? SINGLE_MODE
                                             : (merged ? MERGE_MODE : EXTEND_MODE));
}

void DisplayWorker::updateNightModeStatus()
{
    QProcess *process = new QProcess;

    connect(process, &QProcess::readyRead, this, [=] {
        m_model->setIsNightMode(process->readAll().replace("\n","") == "active");
        m_model->setRedshiftSetting(false);
        process->close();
        process->deleteLater();
    });

    process->start("systemctl", QStringList() << "--user" << "is-active" << "redshift.service");
}

void DisplayWorker::onGSettingsChanged(const QString &key)
{
    const QVariant &value = m_dccSettings->get(key);

    if (key == GSETTINGS_MINIMUM_BRIGHTNESS)
        m_model->setMinimumBrightnessScale(value.toDouble());
}

void DisplayWorker::record() {
    if (m_isGxde) {
        m_gxdeSnapshot = GxdeScreen::outputs();
        return;
    }

    const int displayMode { m_model->displayMode() };
    const QString config { displayMode == CUSTOM_MODE ? m_model->config() : m_model->primary() };

    m_model->setLastConfig(std::pair<int, QString>(displayMode, config));
}

void DisplayWorker::restore() {
    if (m_isGxde) {
        restoreGxdeSnapshot();
        return;
    }

    const std::pair<int, QString> lastConfig { m_model->lastConfig() };

    switch (lastConfig.first)
    {
        case CUSTOM_MODE: {
            discardChanges();
            switchMode(lastConfig.first, lastConfig.second);
            saveChanges();
            break;
        }
        case MERGE_MODE:
            mergeScreens();
            break;
        case EXTEND_MODE:
            extendMode();
            break;
        case SINGLE_MODE:
            onlyMonitor(lastConfig.second);
            break;
        default:
            break;
    }
}

void DisplayWorker::applyGxdeLayout() {
    if (!m_isGxde) {
        return;
    }

    QList<GxdeScreen::Layout> layout;
    const QList<GxdeScreen::Output> outputs = GxdeScreen::outputs();
    for (const GxdeScreen::Output& output : outputs) {
        if (!output.enabled)
            continue;

        for (Monitor *monitor : m_model->monitorList()) {
            if (monitor->name() != output.name)
                continue;
            layout.append({output.name, monitor->x(), monitor->y()});
            break;
        }
    }

    if (!GxdeScreen::setLayout(layout)) {
        qWarning() << "(Display) Layout: Failed to apply screen layout";
        refreshGxdeState();
        return;
    }
    refreshGxdeState();
}

bool DisplayWorker::restoreGxdeSnapshot() {
    if (!m_isGxde || m_gxdeSnapshot.isEmpty())
        return false;

    bool restored = true;

    // Enable the saved active outputs before disabling any others, so the
    // compositor is never asked to turn off its last active screen.
    for (const GxdeScreen::Output &output : m_gxdeSnapshot) {
        if (output.enabled)
            restored = GxdeScreen::setEnabled(output.name, true) && restored;
    }
    for (const GxdeScreen::Output &output : m_gxdeSnapshot) {
        if (!output.enabled)
            restored = GxdeScreen::setEnabled(output.name, false) && restored;
    }

    QList<GxdeScreen::Layout> layout;
    QString primary;
    for (const GxdeScreen::Output &output : m_gxdeSnapshot) {
        if (!output.enabled)
            continue;

        const int refreshHz = qMax(1, qRound(output.refresh / 1000.0));
        restored = GxdeScreen::setResolution(
                       output.name, output.width, output.height, refreshHz)
                   && restored;
        restored = GxdeScreen::setScale(output.name, output.scale) && restored;
        restored = GxdeScreen::setRotation(
                       output.name,
                       GxdeScreen::rotationToAngle(
                           GxdeScreen::transformToRotation(output.transform)))
                   && restored;
        layout.append({output.name, output.x, output.y});
        if (output.primary)
            primary = output.name;
    }

    restored = GxdeScreen::setLayout(layout) && restored;
    if (!primary.isEmpty())
        restored = GxdeScreen::setPrimary(primary) && restored;

    m_gxdeSnapshot.clear();
    refreshGxdeState();
    if (!restored)
        qWarning() << "(Display) Snapshot: Failed to restore complete output snapshot";
    return restored;
}
