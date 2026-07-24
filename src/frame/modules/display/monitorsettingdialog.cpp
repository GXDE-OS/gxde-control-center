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

#include "monitorsettingdialog.h"
#include "monitorcontrolwidget.h"
#include "displaymodel.h"
#include "frame.h"
#include "settingslistwidget.h"
#include "wayland/gxdescreen.h"
#include "widgets/basiclistmodel.h"
#include "widgets/basiclistview.h"
#include "widgets/basiclistdelegate.h"
#include "dthememanager.h"
#include <QVBoxLayout>
#include <QTimer>
#include <QMouseEvent>
#include <QScreen>
#include <QWindow>
#include <DSuggestButton>
#include <algorithm>
#include <limits>

DWIDGET_USE_NAMESPACE

using namespace dcc::widgets;

namespace dcc {

namespace display {

MonitorSettingDialog::MonitorSettingDialog(DisplayModel *model, QWidget *parent)
    : DAbstractDialog(parent),

      m_primary(true),

      m_model(model),
      m_monitor(nullptr),

      m_positionWatcher(new QTimer(this))
{
    init();
    Monitor* primary = model->primaryMonitor();
    if (!primary && !model->monitorList().isEmpty())
        primary = model->monitorList().first();
    reloadMonitorObject(primary);
    initPrimary();
#ifdef HAS_LAYER_SHELL
    configureLayerShell();
#endif
}

MonitorSettingDialog::MonitorSettingDialog(Monitor *monitor, QWidget *parent)
    : DAbstractDialog(parent),

      m_primary(false),
      m_monitor(nullptr),

      m_positionWatcher(new QTimer(this))
{
    init();
    reloadMonitorObject(monitor);
#ifdef HAS_LAYER_SHELL
    configureLayerShell();
#endif
}

MonitorSettingDialog::~MonitorSettingDialog()
{
    qDeleteAll(m_otherDialogs);
}

void MonitorSettingDialog::mouseMoveEvent(QMouseEvent *e)
{
    e->ignore();
}

void MonitorSettingDialog::init()
{
    setWindowFlags(windowFlags() | Qt::X11BypassWindowManagerHint);

    DThemeManager::instance()->setTheme(this, "light");

    m_fullIndication = std::unique_ptr<MonitorIndicator>(new MonitorIndicator(this));

    setPalette(QPalette(QPalette::Window, Qt::white));
    //setBackgroundColor(QColor(Qt::white));
    //setBorderColor(QColor(0, 0, 0, 0.2 * 255));

    m_resolutionsModel = new BasicListModel;

    m_resolutionView = new BasicListView;
    m_resolutionView->setModel(m_resolutionsModel);
    m_resolutionView->setItemDelegate(new BasicListDelegate);

    connect(m_resolutionView, &BasicListView::entered,
            m_resolutionsModel, &BasicListModel::setHoveredIndex);

    if (m_primary)
    {
        m_resolutionView->setAutoFitHeight(false);
        m_resolutionView->setFixedHeight(36 * 3);
    }

    m_resolutionView->setMinimumWidth(448);

    QLabel *resoLabel = new QLabel;
    resoLabel->setObjectName("Resolution");
    resoLabel->setText(tr("Resolution"));

    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setSpacing(0);
    hlayout->addSpacing(30);
    hlayout->addWidget(resoLabel);

    QVBoxLayout *resoLayout = new QVBoxLayout;
    resoLayout->addLayout(hlayout);
    resoLayout->addWidget(m_resolutionView);
    resoLayout->setSpacing(5);
    resoLayout->setContentsMargins(10, 0, 10, 0);

#ifndef DCC_DISABLE_ROTATE
    m_rotateBtn = new DImageButton;
    m_rotateBtn->setNormalPic(":/display/themes/dark/icons/rotate_normal.png");
    m_rotateBtn->setHoverPic(":/display/themes/dark/icons/rotate_hover.png");
    m_rotateBtn->setPressPic(":/display/themes/dark/icons/rotate_press.png");
#endif

    m_btnsLayout = new QHBoxLayout;
    m_btnsLayout->addSpacing(15);
#ifndef DCC_DISABLE_ROTATE
    m_btnsLayout->addWidget(m_rotateBtn);
#endif
    m_btnsLayout->addStretch();
    m_btnsLayout->setSpacing(10);
    m_btnsLayout->setContentsMargins(10, 0, 10, 0);

    m_mainLayout = new QVBoxLayout;
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(10);

    m_mainLayout->addSpacing(40);
    m_mainLayout->addLayout(resoLayout);
    m_mainLayout->addSpacing(10);
    m_mainLayout->addLayout(m_btnsLayout);
    m_mainLayout->addSpacing(10);

    setContentsMargins(0, 0, 0, 0);

    setLayout(m_mainLayout);

    m_positionWatcher->setSingleShot(false);
    m_positionWatcher->setInterval(1000);
    m_positionWatcher->start();

    connect(m_resolutionView, &BasicListView::clicked,
            [=](const QModelIndex &index) {
                onMonitorResolutionSelected(index.row());
            });
#ifndef DCC_DISABLE_ROTATE
    connect(m_rotateBtn, &DImageButton::clicked, this, &MonitorSettingDialog::onRotateBtnClicked);
#endif
    connect(m_positionWatcher, &QTimer::timeout, this, &MonitorSettingDialog::onMonitorRectChanged);
}

void MonitorSettingDialog::initPrimary()
{
    Q_ASSERT(m_primary);

    QPushButton *cancelBtn = new QPushButton;
    cancelBtn->setText(tr("Cancel"));
    DSuggestButton *applySaveBtn = new DSuggestButton;
    applySaveBtn->setText(tr("Save"));

    m_btnsLayout->addWidget(cancelBtn);
    m_btnsLayout->addWidget(applySaveBtn);

    // add primary screen settings widget
    m_primarySettingsWidget = new SettingsListWidget;
    m_primarySettingsWidget->setTitle(tr("Primary"));
    m_mainLayout->insertWidget(1, m_primarySettingsWidget);

    m_ctrlWidget = new MonitorControlWidget;
    m_ctrlWidget->setDisplayModel(m_model);

    m_mainLayout->insertWidget(1, m_ctrlWidget);

    // add primary settings
    for (auto mon : m_model->monitorList())
        m_primarySettingsWidget->appendOption(mon->name());

    connect(m_ctrlWidget, &MonitorControlWidget::requestMonitorPress, this, &MonitorSettingDialog::onMonitorPress);
    connect(m_ctrlWidget, &MonitorControlWidget::requestMonitorRelease, this, &MonitorSettingDialog::onMonitorRelease);
    connect(m_ctrlWidget, &MonitorControlWidget::requestRecognize, this, &MonitorSettingDialog::requestRecognize);
    connect(m_ctrlWidget, &MonitorControlWidget::requestMerge, this, &MonitorSettingDialog::requestMerge);
    connect(m_ctrlWidget, &MonitorControlWidget::requestSplit, this, &MonitorSettingDialog::requestSplit);
    connect(m_ctrlWidget, &MonitorControlWidget::requestSetMonitorPosition, this, &MonitorSettingDialog::requestSetMonitorPosition);
    connect(m_primarySettingsWidget, &SettingsListWidget::clicked, this, &MonitorSettingDialog::requestSetPrimary);
    connect(m_model, &DisplayModel::primaryScreenChanged, this, &MonitorSettingDialog::onPrimaryChanged);
    connect(m_model, &DisplayModel::screenHeightChanged, this, &MonitorSettingDialog::updateScreensRelation, Qt::QueuedConnection);
    connect(m_model, &DisplayModel::screenWidthChanged, this, &MonitorSettingDialog::updateScreensRelation, Qt::QueuedConnection);
    connect(m_model, &DisplayModel::displayModeChanged, this, &MonitorSettingDialog::reject);
    connect(cancelBtn, &QPushButton::clicked, this, &MonitorSettingDialog::reject);
    connect(applySaveBtn, &DSuggestButton::clicked, this, &MonitorSettingDialog::requestApplySave);
    connect(applySaveBtn, &DSuggestButton::clicked, this, &MonitorSettingDialog::accept);
    reloadOtherScreensDialog();

    onPrimaryChanged();
    QTimer::singleShot(1, this, &MonitorSettingDialog::updateScreensRelation);

    applySaveBtn->setFocus();
}

void MonitorSettingDialog::reloadMonitorObject(Monitor *monitor)
{
    // only refersh mode list
    if (m_monitor == monitor)
        return onMonitorModeChanged();

    if (m_monitor)
    {
        disconnect(m_monitor, &Monitor::currentModeChanged, this, &MonitorSettingDialog::onMonitorModeChanged);
        disconnect(m_monitor, &Monitor::geometryChanged, m_positionWatcher, static_cast<void (QTimer::*)()>(&QTimer::start));
    }

    m_monitor = monitor;
    if (!m_monitor) {
        setWindowTitle(tr("Display"));
        updateModeList({});
        return;
    }

    connect(m_monitor, &Monitor::currentModeChanged, this, &MonitorSettingDialog::onMonitorModeChanged, Qt::QueuedConnection);
    connect(m_monitor, &Monitor::geometryChanged, m_positionWatcher, static_cast<void (QTimer::*)()>(&QTimer::start));

    setWindowTitle(m_monitor->name());
    onMonitorModeChanged();

    QTimer::singleShot(10, this, &MonitorSettingDialog::onMonitorRectChanged);
}

void MonitorSettingDialog::reloadOtherScreensDialog()
{
    qDeleteAll(m_otherDialogs);
    m_otherDialogs.clear();

    // load other non-primary dialogs
    for (auto mon : m_model->monitorList())
    {
        if (mon == m_monitor)
            continue;

        // A layer-shell surface must be independent for each output. Giving
        // the secondary dialog this dialog as its transient parent can make
        // the compositor place both surfaces on the primary output.
        MonitorSettingDialog *dialog = new MonitorSettingDialog(mon);

        connect(dialog, &MonitorSettingDialog::requestSetPrimary, this, &MonitorSettingDialog::requestSetPrimary);
        connect(dialog, &MonitorSettingDialog::requestSetMonitorResolution, this, &MonitorSettingDialog::requestSetMonitorResolution);
#ifndef DCC_DISABLE_ROTATE
        connect(dialog, &MonitorSettingDialog::requestMonitorRotate, this, &MonitorSettingDialog::requestMonitorRotate);
#endif
        dialog->show();
        m_otherDialogs.append(dialog);
    }
}

void MonitorSettingDialog::updateScreensRelation()
{
    const bool merged = m_model->monitorsIsIntersect();

    m_ctrlWidget->setScreensMerged(merged);
    m_primarySettingsWidget->setVisible(!merged);

    for (auto d : m_otherDialogs)
        d->setVisible(!merged);

    onMonitorModeChanged();

    adjustSize();
}

void MonitorSettingDialog::onPrimaryChanged()
{
    Q_ASSERT(m_primary);

    // update current index
    const QString primaryName = m_model->primary();
    for (int i(0); i != m_model->monitorList().size(); ++i)
    {
        if (m_model->monitorList()[i]->name() == primaryName)
        {
            m_primarySettingsWidget->setSelectedIndex(i);
            break;
        }
    }

    Monitor* primary = m_model->primaryMonitor();
    if (!primary) {
        return;
    }

    if (m_monitor == primary) {
        return;
    }

    reloadMonitorObject(primary);
    reloadOtherScreensDialog();
}

void MonitorSettingDialog::onMonitorRectChanged()
{
    if (!m_monitor)
        return;

#ifdef HAS_LAYER_SHELL
    configureLayerShell();
    if (m_layerShellWindow) {
        return;
    }
#endif

    const QRect area = popupArea();
    DAbstractDialog::move(area.center() - rect().center());
}

QScreen* MonitorSettingDialog::monitorScreen() const {
    if (m_monitor) {
        for (QScreen* screen : qApp->screens()) {
            if (screen->name() == m_monitor->name()) {
                return screen;
            }
        }
    }
    return qApp->primaryScreen();
}

QRect MonitorSettingDialog::popupArea() const {
    QScreen* screen = monitorScreen();
    if (!screen) {
        return m_monitor ? m_monitor->rect() : QRect();
    }

    QRect area = screen->geometry();
    if (m_monitor && m_monitor->isPrimary()) {
        area.setRight(area.right() - FRAME_WIDTH);
    }

    return area;
}

#ifdef HAS_LAYER_SHELL
void MonitorSettingDialog::configureLayerShell() {
    if (!Wayland::isWaylandSession() || !m_monitor)
        return;

    QScreen* screen = monitorScreen();
    if (!screen) {
        return;
    }

    adjustSize();
    const QRect area = popupArea();
    const QRect screenRect = screen->geometry();
    const QPoint globalTopLeft = area.center() - rect().center();

    if (!m_layerShellWindow) {
        // QT_WAYLAND_SHELL_INTEGRATION creates the layer surface together
        // with the native window. Put the widget inside the target screen
        // first so Qt selects that screen's wl_output at creation time.
        DAbstractDialog::move(globalTopLeft);
        setAttribute(Qt::WA_NativeWindow, true);
        createWinId();
    }

    QWindow* window = windowHandle();
    if (!window) {
        return;
    }

    if (!m_layerShellWindow) {
        m_layerShellWindow = LayerShellQt::Window::get(window);
    }

    if (!m_layerShellWindow) {
        return;
    }

    m_layerShellWindow->setScope(
        QStringLiteral("control-center-monitor-settings"));
    m_layerShellWindow->setScreenConfiguration(
        LayerShellQt::Window::ScreenFromQWindow);
    m_layerShellWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    m_layerShellWindow->setAnchors(LayerShellQt::Window::Anchors(
        LayerShellQt::Window::AnchorTop |
        LayerShellQt::Window::AnchorLeft));
    m_layerShellWindow->setExclusiveZone(-1);
    m_layerShellWindow->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityOnDemand);
    m_layerShellWindow->setCloseOnDismissed(false);

    const QPoint localCenter =
        area.center() - screenRect.topLeft() - rect().center();
    m_layerShellWindow->setMargins(QMargins(
        qMax(0, localCenter.x()),
        qMax(0, localCenter.y()),
        0,
        0));
    qInfo() << "(Display popup) configured"
            << m_monitor->name()
            << "screen=" << window->screen()->name()
            << "geometry=" << screenRect
            << "margins=" << m_layerShellWindow->margins();
}
#endif

void MonitorSettingDialog::onMonitorModeChanged() {
    const bool intersect = m_primary && m_model->monitorsIsIntersect();
    if (intersect) {
        updateModeList(commonModes());
    } else {
        updateModeList(availableModes(m_monitor));
    }

    for (int index = 0; index < m_modeOptions.size(); ++index) {
        if (m_modeOptions.at(index).current) {
            const QModelIndex currentIndex =
                m_resolutionsModel->index(index);
            m_resolutionsModel->setSelectedIndex(currentIndex);
            QTimer::singleShot(0, m_resolutionView,
                               [this, currentIndex] {
                m_resolutionView->scrollTo(
                    currentIndex,
                    QAbstractItemView::PositionAtCenter);
            });
            break;
        }
    }
}

QList<MonitorSettingDialog::ResolutionOption>
MonitorSettingDialog::availableModes(Monitor* monitor) const {
    QList<ResolutionOption> result;
    if (!monitor) {
        return result;
    }

    const ResolutionList legacyModes = monitor->modeList();
    const QList<GxdeScreen::Mode> gxdeModes =
        GxdeScreen::outputModes(monitor->name());
    if (gxdeModes.isEmpty()) {
        bool first = true;
        for (const Resolution &mode : legacyModes) {
            ResolutionOption option;
            option.mode = mode.id();
            option.width = mode.width();
            option.height = mode.height();
            option.refresh = qRound(mode.rate() * 1000.0);
            option.preferred = first;
            option.current = mode == monitor->currentMode();
            result.append(option);
            first = false;
        }
        return result;
    }

    for (const GxdeScreen::Mode &mode : gxdeModes) {
        ResolutionOption option;
        option.width = mode.width;
        option.height = mode.height;
        option.refresh = mode.refresh;
        option.preferred = mode.preferred;
        option.current = mode.current;

        int bestRefreshDelta = std::numeric_limits<int>::max();
        for (const Resolution &legacyMode : legacyModes) {
            if (legacyMode.width() != mode.width ||
                    legacyMode.height() != mode.height) {
                continue;
            }
            const int refreshDelta =
                qAbs(qRound(legacyMode.rate() * 1000.0) - mode.refresh);
            if (refreshDelta < bestRefreshDelta) {
                bestRefreshDelta = refreshDelta;
                option.mode = legacyMode.id();
            }
        }
        result.append(option);
    }
    return result;
}

QList<MonitorSettingDialog::ResolutionOption>
MonitorSettingDialog::commonModes() const {
    if (!m_model || m_model->monitorList().isEmpty())
        return {};

    QList<ResolutionOption> result =
        availableModes(m_model->monitorList().first());
    for (int index = 1; index < m_model->monitorList().size(); ++index) {
        const QList<ResolutionOption> modes =
            availableModes(m_model->monitorList().at(index));
        for (auto it = result.begin(); it != result.end();) {
            const auto match = std::find_if(
                modes.cbegin(), modes.cend(),
                [it](const ResolutionOption &mode) {
                    return mode.width == it->width &&
                           mode.height == it->height;
                });
            if (match == modes.cend())
                it = result.erase(it);
            else
                ++it;
        }
    }
    return result;
}

void MonitorSettingDialog::updateModeList(
        const QList<ResolutionOption> &modeList)
{
    m_resolutionsModel->clear();
    m_modeOptions = modeList;

    for (const ResolutionOption& mode : modeList) {
        const QString option = QString::number(mode.width) + "×" +
            QString::number(mode.height) + "+" +
            QString::number(qRound(mode.refresh / 1000.0)) + "Hz";

        if (mode.preferred)
            m_resolutionsModel->appendOption(option + tr(" (Recommended)"));
        else
            m_resolutionsModel->appendOption(option);
    }

    Q_EMIT m_resolutionsModel->layoutChanged();
    adjustSize();
#ifdef HAS_LAYER_SHELL
    if (m_layerShellWindow)
        configureLayerShell();
#endif
}

void MonitorSettingDialog::onMonitorResolutionSelected(const int index)
{
    if (index < 0 || index >= m_modeOptions.size() || !m_monitor)
        return;

    const bool intersect = m_primary && m_model->monitorsIsIntersect();

    if (intersect)
    {
        const ResolutionOption selected = m_modeOptions.at(index);

        for (Monitor* mon : m_model->monitorList()) {
            const QList<ResolutionOption> modes = availableModes(mon);
            const auto match = std::find_if(
                modes.cbegin(), modes.cend(),
                [&selected](const ResolutionOption &mode) {
                    return mode.width == selected.width &&
                           mode.height == selected.height;
                });
            if (match != modes.cend()) {
                Q_EMIT requestSetMonitorResolution(
                    mon, match->mode, match->width, match->height,
                    match->refresh);
            }
        }
    } else {
        const ResolutionOption selected = m_modeOptions.at(index);
        Q_EMIT requestSetMonitorResolution(
            m_monitor, selected.mode, selected.width, selected.height,
            selected.refresh);
    }
    onMonitorModeChanged();
}

#ifndef DCC_DISABLE_ROTATE
void MonitorSettingDialog::onRotateBtnClicked()
{
    const bool intersect = m_primary && m_model->monitorsIsIntersect();

    if (intersect)
        Q_EMIT requestMonitorRotate(nullptr);
    else
        Q_EMIT requestMonitorRotate(m_monitor);
}
#endif

void MonitorSettingDialog::onMonitorPress(Monitor* mon) {
    if (Wayland::isWaylandSession()) {
        return;
    }

    m_fullIndication->setGeometry(mon->rect());
    m_fullIndication->show();
}

void MonitorSettingDialog::onMonitorRelease(Monitor *mon)
{
    // FIXME: I don't know why indicator not hide at new for monitorproxywidget
    m_fullIndication->hide();
}

} // namespace display

} // namespace dcc
