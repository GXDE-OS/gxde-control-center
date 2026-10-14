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

#include "personalizationwork.h"
#include "model/thememodel.h"
#include "model/fontmodel.h"
#include "model/fontsizemodel.h"
#include "userfontconfig.h"
#include "fontfamilies.h"
#include <QSysInfo>
#include <QGSettings>
#include <QGuiApplication>
#include <QLocale>
#include <QSet>
#include <QDBusVariant>
#include <QApplication>
#include <QFont>
#include <cmath>

using namespace dcc;
using namespace dcc::personalization;

const QString Service = "com.deepin.daemon.Appearance";
const QString Path    = "/com/deepin/daemon/Appearance";

static const std::vector<int> OPACITY_SLIDER {
    0,
    25,
    40,
    55,
    70,
    85,
    100
};

PersonalizationWork::PersonalizationWork(PersonalizationModel *model, QObject *parent)
    : QObject(parent),
      m_model(model),
      m_dbus(new Appearance(Service, Path, QDBusConnection::sessionBus(), this)),
      m_wmSwitcher(new WMSwitcher("com.deepin.WMSwitcher", "/com/deepin/WMSwitcher", QDBusConnection::sessionBus(), this))
{
    ThemeModel *cursorTheme      = m_model->getMouseModel();
    ThemeModel *windowTheme      = m_model->getWindowModel();
    ThemeModel *iconTheme        = m_model->getIconModel();
    FontModel *fontMono          = m_model->getMonoFontModel();
    FontModel *fontStand         = m_model->getStandFontModel();

    connect(m_dbus, &Appearance::GtkThemeChanged,      windowTheme,   &ThemeModel::setDefault);
    connect(m_dbus, &Appearance::CursorThemeChanged,   cursorTheme,   &ThemeModel::setDefault);
    connect(m_dbus, &Appearance::IconThemeChanged,     iconTheme,     &ThemeModel::setDefault);
    connect(m_dbus, &Appearance::MonospaceFontChanged, fontMono,      &FontModel::setFontName);
    connect(m_dbus, &Appearance::StandardFontChanged,  fontStand,     &FontModel::setFontName);
    connect(m_dbus, &Appearance::FontSizeChanged, this, &PersonalizationWork::FontSizeChanged);
    connect(m_dbus, &Appearance::Refreshed, this, &PersonalizationWork::onRefreshedChanged);

    connect(m_wmSwitcher, &WMSwitcher::WMChanged, this, &PersonalizationWork::onToggleWM);
    connect(m_dbus, &Appearance::OpacityChanged, this, &PersonalizationWork::refreshOpacity);

    m_themeModels["gtk"]           = windowTheme;
    m_themeModels["icon"]          = iconTheme;
    m_themeModels["cursor"]        = cursorTheme;
    m_fontModels["standardfont"]   = fontStand;
    m_fontModels["monospacefont"]  = fontMono;

    connect(qGuiApp, &QGuiApplication::fontDatabaseChanged, this, [this] {
        m_localFontsLoaded = false;
        scanLocalFonts();
    });

    m_dbus->setSync(false);
    m_wmSwitcher->setSync(false);
    QMetaObject::invokeMethod(this, &PersonalizationWork::refreshFontSize, Qt::QueuedConnection);
}

void PersonalizationWork::active()
{
    m_dbus->blockSignals(false);
    m_wmSwitcher->blockSignals(false);

    refreshFontSize();
    refreshWMState();
    refreshOpacity(m_dbus->opacity());

    m_model->getWindowModel()->setDefault(m_dbus->gtkTheme());
    m_model->getIconModel()->setDefault(m_dbus->iconTheme());
    m_model->getMouseModel()->setDefault(m_dbus->cursorTheme());
    m_model->getMonoFontModel()->setFontName(m_dbus->monospaceFont());
    m_model->getStandFontModel()->setFontName(m_dbus->standardFont());
}

void PersonalizationWork::deactive()
{
    m_dbus->blockSignals(true);
    m_wmSwitcher->blockSignals(true);
}

QList<QJsonObject> PersonalizationWork::converToList(const QString &type, const QJsonArray &array)
{
    QList<QJsonObject> list;
    for (int i = 0; i != array.size(); i++) {
        QJsonObject object = array.at(i).toObject();
        object.insert("type", QJsonValue(type));
        list.append(object);
    }
    return list;
}

void PersonalizationWork::addList(ThemeModel *model, const QString &type, const QJsonArray &array)
{
    QList<QString> list;
    QList<QJsonObject> objList;
    for (int i = 0; i != array.size(); i++) {
        QJsonObject object = array.at(i).toObject();
        object.insert("type", QJsonValue(type));
        objList << object;
        list.append(object["Id"].toString());

        QDBusPendingReply<QString> pic = m_dbus->Thumbnail(type, object["Id"].toString());
        QDBusPendingCallWatcher *picWatcher = new QDBusPendingCallWatcher(pic, this);
        picWatcher->setProperty("category", type);
        picWatcher->setProperty("id", object["Id"].toString());
        connect(picWatcher, &QDBusPendingCallWatcher::finished, this, &PersonalizationWork::onGetPicFinished);
    }

    // sort for display name
    std::sort(objList.begin(), objList.end(), [=] (const QJsonObject &obj1, const QJsonObject &obj2) {
        QCollator qc;
        return qc.compare(obj1["Id"].toString(), obj2["Id"].toString()) < 0;
    });

    for (const QJsonObject &obj : objList) {
        model->addItem(obj["Id"].toString(), obj);
    }

    for (const QString &id : model->getList().keys()) {
        if (!list.contains(id)) {
            model->removeItem(id);
        }
    }
}

void PersonalizationWork::refreshWMState()
{
    QDBusPendingCallWatcher *wmWatcher = new QDBusPendingCallWatcher(m_wmSwitcher->CurrentWM(), this);
    connect(wmWatcher, &QDBusPendingCallWatcher::finished, this, &PersonalizationWork::onGetCurrentWMFinished);
}

void PersonalizationWork::FontSizeChanged(const double value) const
{
    if (!std::isfinite(value) || value <= 0)
        return;
    FontSizeModel *fontSizeModel = m_model->getFontSizeModel();
    fontSizeModel->setFontSize(sizeToSliderValue(value));
    QFont font = QApplication::font();
    if (!qFuzzyCompare(font.pointSizeF(), value)) {
        font.setPointSizeF(value);
        QApplication::setFont(font);
    }
}

void PersonalizationWork::onGetFontFinished(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<QString> reply = *w;

    if (!reply.isError()) {
        const QString &category = w->property("category").toString();

        setFontList(m_fontModels[category], category, reply.value());
    }
    else {
        qWarning() << reply.error();
    }

    w->deleteLater();
}

void PersonalizationWork::onGetThemeFinished(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<QString> reply = *w;

    if (!reply.isError()) {
        const QString &category = w->property("category").toString();
        const QJsonArray &array = QJsonDocument::fromJson(reply.value().toUtf8()).array();

        addList(m_themeModels[category], category, array);
    }
    else {
        qWarning() << reply.error();
    }

    w->deleteLater();
}

void PersonalizationWork::onGetPicFinished(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<QString> reply = *w;

    if (!reply.isError()) {
        const QString &category = w->property("category").toString();
        const QString &id = w->property("id").toString();

        m_themeModels[category]->addPic(id, reply.value());
    }
    else {
        qWarning() << reply.error();
    }

    w->deleteLater();
}

void PersonalizationWork::onRefreshedChanged(const QString &type)
{
    if (m_themeModels.keys().contains(type)) {
        refreshThemeByType(type);
    }

    if (m_fontModels.keys().contains(type)) {
        refreshFontByType(type);
    }
}

void PersonalizationWork::onToggleWM(const QString &wm)
{
    m_model->setIs3DWm(wm == "deepin wm");
}

void PersonalizationWork::onGetCurrentWMFinished(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<QString> reply = w->reply();

    if (!reply.isError()) {
        onToggleWM(reply.value());
    }
    else {
        qWarning() << reply.error();
    }

    w->deleteLater();
}

void PersonalizationWork::setFontList(FontModel *model, const QString &type, const QString &list)
{
    QJsonArray array = QJsonDocument::fromJson(list.toLocal8Bit().data()).array();

    QStringList l;

    for (int i = 0; i != array.size(); i++)
        l << array.at(i).toString();

    QDBusPendingCallWatcher *watcher  = new QDBusPendingCallWatcher(m_dbus->Show(type, l), this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [=] (QDBusPendingCallWatcher *w) {
        if (!w->isError()) {
            QDBusPendingReply<QString> r = w->reply();

            QJsonArray array = QJsonDocument::fromJson(r.value().toLocal8Bit().data()).array();

            updateFontList(model, type, converToList(type, array));
        }
        else {
            qWarning() << w->error();
        }

        watcher->deleteLater();
    });
}

void PersonalizationWork::refreshTheme()
{
    for (QMap<QString, ThemeModel*>::ConstIterator it = m_themeModels.begin(); it != m_themeModels.end(); it++) {
        refreshThemeByType(it.key());
    }
}

void PersonalizationWork::updateFontList(FontModel *model, const QString &type, QList<QJsonObject> fonts)
{
    if (type == QStringLiteral("standardfont")) {
        m_serviceStandardFonts = fonts;
        QSet<QString> ids;
        for (const QJsonObject &font : fonts)
            ids.insert(font["Id"].toString());
        for (const QJsonObject &font : m_localStandardFonts) {
            const QString id = font["Id"].toString();
            if (!ids.contains(id)) {
                fonts.append(font);
                ids.insert(id);
            }
        }
    }

    QCollator collator;
    std::sort(fonts.begin(), fonts.end(), [&collator](const QJsonObject &left, const QJsonObject &right) {
        return collator.compare(left["Name"].toString(), right["Name"].toString()) < 0;
    });
    model->setFontList(fonts);
}

void PersonalizationWork::refreshThemeByType(const QString &type)
{
    QDBusPendingReply<QString> theme = m_dbus->List(type);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(theme, this);
    watcher->setProperty("category", type);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &PersonalizationWork::onGetThemeFinished);
}

void PersonalizationWork::refreshFont()
{
    for (QMap<QString, FontModel*>::const_iterator it = m_fontModels.begin(); it != m_fontModels.end(); it++) {
        refreshFontByType(it.key());
    }

    refreshFontSize();
}

void PersonalizationWork::refreshFontSize()
{
    QDBusMessage request = QDBusMessage::createMethodCall(Service, Path,
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    request << QStringLiteral("com.deepin.daemon.Appearance") << QStringLiteral("FontSize");
    const quint64 requestId = m_fontSizeRequest;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, requestId] {
        const QDBusPendingReply<QDBusVariant> reply = *watcher;
        watcher->deleteLater();
        if (requestId != m_fontSizeRequest)
            return;
        if (reply.isError()) {
            qWarning() << "Failed to read font size:" << reply.error();
            return;
        }

        bool ok = false;
        const double points = reply.value().variant().toDouble(&ok);
        if (ok && std::isfinite(points) && points > 0) {
            FontSizeChanged(points);
            syncGtkFontSize(points);
        }
    });
}

void PersonalizationWork::refreshFontByType(const QString &type) {
    if (type == QStringLiteral("standardfont")) {
        scanLocalFonts();
        if (!m_fontModels.value(type)->getFontList().isEmpty())
            return;
    }

    QDBusPendingReply<QString> font = m_dbus->List(type);
    QDBusPendingCallWatcher *fontWatcher = new QDBusPendingCallWatcher(font, this);
    fontWatcher->setProperty("category", type);
    connect(fontWatcher, &QDBusPendingCallWatcher::finished, this, &PersonalizationWork::onGetFontFinished);
}

void PersonalizationWork::scanLocalFonts()
{
    if (m_localFontsLoaded || m_fontScanProcess)
        return;
    auto *process = new QProcess(this);
    m_fontScanProcess = process;
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        m_fontScanProcess = nullptr;
        if (status == QProcess::NormalExit && code == 0) {
            m_localStandardFonts = parseFontFamilies(process->readAllStandardOutput(), QLocale::system());
            m_localFontsLoaded = true;
            updateFontList(m_model->getStandFontModel(), QStringLiteral("standardfont"), m_serviceStandardFonts);
        } else {
            qWarning() << "Failed to list local fonts:" << process->readAllStandardError();
        }
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_fontScanProcess = nullptr;
            qWarning() << "Failed to start font listing:" << process->errorString();
            process->deleteLater();
        }
    });
    process->start(QStringLiteral("fc-list"),
                   {QStringLiteral("--format"), QStringLiteral("%{family}\t%{familylang}\n")});
}

void PersonalizationWork::refreshOpacity(double opacity)
{
    int slider { toSliderValue<int>(OPACITY_SLIDER, static_cast<int>(opacity * 100)) };
    qDebug() << QString("opacity: %1, slider: %2").arg(opacity).arg(slider);
    m_model->setOpacity(std::pair<int, double>(slider, opacity));
}

int PersonalizationWork::sizeToSliderValue(const double value) const
{
    if (!std::isfinite(value))
        return 1;
    int closest = 0;
    for (int index = 1; index <= 6; ++index) {
        if (std::abs(value - sliderValueToSize(index)) < std::abs(value - sliderValueToSize(closest)))
            closest = index;
    }
    return closest;
}

double PersonalizationWork::sliderValueToSize(const int value) const
{
    switch (value) {
    case 0:
        return 8.2;
    case 1:
        return 9;
    case 2:
        return 9.7;
    case 3:
        return 11.2;
    case 4:
        return 12;
    case 5:
        return 13.5;
    case 6:
        return 14.5;
    default:
        return 9;
    }
}

double PersonalizationWork::sliderValutToOpacity(const int value) const
{
    return static_cast<double>(OPACITY_SLIDER[value]) / static_cast<double>(100);
}

void PersonalizationWork::setDefault(const QJsonObject &value) {
    //使用type去调用
    const QString type = value["type"].toString();
    const QString theme = value["Id"].toString();
    const QDBusPendingCall call = m_dbus->Set(type, theme);

    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, type, theme] {
        const QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            qWarning() << "Failed to set theme:" << type << reply.error();
            return;
        }

        // Manually refresh dark/light theme page after setting.
        if (ThemeModel *model = m_themeModels.value(type, nullptr)) {
            model->setDefault(theme);
        }

        if (FontModel *model = m_fontModels.value(type, nullptr)) {
            model->setFontName(theme);
            if (QSysInfo::productType().compare(QStringLiteral("arch"), Qt::CaseInsensitive) == 0) {
                QString error;
                if (!saveUserFont(type, theme, &error)) {
                    qWarning() << "Failed to save user fontconfig:" << error;
                    notifyFontCache(tr("Failed to save font settings."));
                } else {
                    refreshFontCache();
                }
            }
        }

        if (type != QStringLiteral("gtk")) {
            return;
        }

        // Persist the color preference too
        const QByteArray schema("org.gnome.desktop.interface");
        if (!QGSettings::isSchemaInstalled(schema)) {
            return;
        }

        QGSettings settings(schema);
        if (!settings.keys().contains(QStringLiteral("colorScheme"))) {
            return;
        }

        const QString scheme = theme.endsWith(QStringLiteral("-dark"),
            Qt::CaseInsensitive)
            ? QStringLiteral("prefer-dark") : QStringLiteral("prefer-light");

        if (!settings.trySet(QStringLiteral("colorScheme"), scheme)) {
            qWarning() << "Failed to save color scheme:" << scheme;
        }
    });
}

void PersonalizationWork::notifyFontCache(const QString &message) {
    QDBusMessage notification = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    notification << QStringLiteral("GXDE Control Center") << uint(0)
        << QStringLiteral("preferences-desktop-font") << tr("Font Settings")
        << message << QStringList() << QVariantMap() << 5000;
    QDBusConnection::sessionBus().asyncCall(notification);
}

void PersonalizationWork::refreshFontCache() {
    if (m_fontCacheProcess) {
        m_fontCacheRefreshPending = true;
        return;
    }

    notifyFontCache(tr("Refreshing font cache…"));
    auto *process = new QProcess(this);
    m_fontCacheProcess = process;
    process->setStandardOutputFile(QProcess::nullDevice());
    const auto complete = [this, process](bool success) {
        m_fontCacheProcess = nullptr;
        if (!success)
            qWarning() << "Font cache refresh failed:" << process->errorString()
                << process->readAllStandardError();
        process->deleteLater();
        if (m_fontCacheRefreshPending) {
            m_fontCacheRefreshPending = false;
            refreshFontCache();
            return;
        }
        notifyFontCache(success ? tr("Font cache refreshed.")
            : tr("Failed to refresh font cache."));
    };
    connect(process, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(status == QProcess::NormalExit && code == 0);
    });
    connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            complete(false);
        }
    });
    process->start(QStringLiteral("fc-cache"), {QStringLiteral("-f")});
}

void PersonalizationWork::setFontSize(const int value)
{
    if (value < 0 || value > 6)
        return;
    const double points = sliderValueToSize(value);
    const quint64 requestId = ++m_fontSizeRequest;
    QDBusMessage request = QDBusMessage::createMethodCall(Service, Path,
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Set"));
    request << QStringLiteral("com.deepin.daemon.Appearance") << QStringLiteral("FontSize")
            << QVariant::fromValue(QDBusVariant(points));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, requestId, points] {
        const QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (requestId != m_fontSizeRequest)
            return;
        FontSizeModel *model = m_model->getFontSizeModel();
        if (reply.isError()) {
            qWarning() << "Failed to set font size:" << reply.error();
            model->setFontSize(model->getFontSize());
            return;
        }
        FontSizeChanged(points);
        syncGtkFontSize(points);
        qInfo() << "Font size applied:" << points << "pt; application font:"
                << QApplication::font().pointSizeF() << "pt";
    });
}

void PersonalizationWork::syncGtkFontSize(double points)
{
    if (QSysInfo::productType().compare(QStringLiteral("arch"), Qt::CaseInsensitive) != 0
        || !QGSettings::isSchemaInstalled("com.deepin.dde.appearance")
        || !QGSettings::isSchemaInstalled("org.gnome.desktop.interface"))
        return;

    QGSettings appearance("com.deepin.dde.appearance");
    QGSettings gtk("org.gnome.desktop.interface");
    const auto sync = [&appearance, &gtk, points](const QString &source, const QString &target) {
        if (!appearance.keys().contains(source) || !gtk.keys().contains(target))
            return;
        const QString family = appearance.get(source).toString();
        if (!family.isEmpty() && !gtk.trySet(target, family + ' ' + QString::number(points, 'g', 6)))
            qWarning() << "Failed to sync GTK font size:" << target;
    };
    sync(QStringLiteral("fontStandard"), QStringLiteral("fontName"));
    sync(QStringLiteral("fontMonospace"), QStringLiteral("monospaceFontName"));
}

void PersonalizationWork::switchWM()
{
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(m_wmSwitcher->RequestSwitchWM(), this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [=] {
        if (watcher->isError()) {
            qDebug() << watcher->error();
        }
        watcher->deleteLater();
    });
}

void PersonalizationWork::setOpacity(int opacity)
{
    m_dbus->setOpacity(sliderValutToOpacity(opacity));
}

void PersonalizationWork::setTopPanel(const int value)
{
    m_model->setTopPanel(value);
}

void PersonalizationWork::setTopPanelGlobalMenu(const int value)
{
    m_model->setTopPanelGlobalMenu(value);
}

void PersonalizationWork::setBottomPanel(const int value)
{
    m_model->setBottomPanel(value);
}

void PersonalizationWork::set20Launcher(const int value)
{
    m_model->set20Launcher(value);
}

void PersonalizationWork::setHideDDEDock(const int value)
{
    m_model->setHideDDEDock(value);
}

void PersonalizationWork::setDockUseMacMode(const int value)
{
    m_model->setDockUseMacMode(value);
}

void PersonalizationWork::setSizeMode(const int value){
    m_model->setSizeMode(value);
}

template<typename T>
T PersonalizationWork::toSliderValue(std::vector<T> list, T value)
{
    for (auto it = list.cbegin(); it != list.cend(); ++it) {
        if (value < *it) {
            return (--it) - list.begin();
        }
    }

    return list.end() - list.begin();
}
