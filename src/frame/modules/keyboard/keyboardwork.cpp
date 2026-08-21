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

#include "keyboardwork.h"
#include "shortcutitem.h"
#include "keyboardmodel.h"
#include "dapplication.h"
#include "wayland/gxdeinput.h"
#include <algorithm>
#include <QTime>
#include <QDebug>
#include <QLocale>
#include <QCollator>
#include <QDBusConnection>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QSettings>
#include <QSet>
#include <QXmlStreamReader>
#include <utility>


namespace dcc {
namespace keyboard{


bool caseInsensitiveLessThan(const MetaData &s1, const MetaData &s2);

// Convert a gxde-wlcom binding string ("Ctrl+Alt+a:no") to the display
// format used by the deepin shortcut UI ("<Control><Alt>a").
static QString wlcomBindingToDisplay(const QString &binding)
{
    QString b = binding;
    if (b.endsWith(QStringLiteral(":no")))
        b.chop(3);

    QString modifiers;
    QString key;
    const QStringList parts = b.split(QLatin1Char('+'));
    for (const QString &part : parts) {
        const QString up = part.toUpper();
        if (up == QLatin1String("CTRL") || up == QLatin1String("CONTROL"))
            modifiers += QStringLiteral("<Control>");
        else if (up == QLatin1String("ALT"))
            modifiers += QStringLiteral("<Alt>");
        else if (up == QLatin1String("WIN") || up == QLatin1String("SUPER") ||
                 up == QLatin1String("META"))
            modifiers += QStringLiteral("<Super>");
        else if (up == QLatin1String("SHIFT"))
            modifiers += QStringLiteral("<Shift>");
        else
            key = part;
    }
    return modifiers + key;
}

// Convert the shortcut display format to a gxde-wlcom binding string.
static QString displayToWlcomBinding(const QString &accels)
{
    QString a = accels;
    QStringList parts;
    if (a.contains(QLatin1Char('<'))) {
        // "<Control><Alt>a"
        a.replace(QLatin1Char('>'), QLatin1Char('+'));
        a.replace(QLatin1Char('<'), QLatin1Char(' '));
        a = a.simplified();
        a.replace(QLatin1Char(' '), QLatin1Char('+'));
        parts = a.split(QLatin1Char('+'));
    } else if (a.contains(QLatin1Char('+'))) {
        parts = a.split(QLatin1Char('+'));
    } else {
        parts = a.split(QLatin1Char('-'));
    }

    QStringList wlcom;
    for (const QString &part : parts) {
        const QString up = part.toUpper();
        if (up == QLatin1String("CONTROL") || up == QLatin1String("CTRL"))
            wlcom << QStringLiteral("Ctrl");
        else if (up == QLatin1String("ALT"))
            wlcom << QStringLiteral("Alt");
        else if (up == QLatin1String("SUPER") || up == QLatin1String("WIN") ||
                 up == QLatin1String("META"))
            wlcom << QStringLiteral("Win");
        else if (up == QLatin1String("SHIFT"))
            wlcom << QStringLiteral("Shift");
        else
            wlcom << part.toLower();
    }
    return wlcom.join(QLatin1Char('+')) + QStringLiteral(":no");
}

// Convert a gxde-wlcom action config (JSON) to the comma separated "bus
// string" accepted by com.kylin.Wlcom.InputAction.AddAction.
static QString actionJsonToBusString(const QJsonObject &obj)
{
    const QString type = obj.value(QStringLiteral("actiontype")).toString();
    if (type == QLatin1String("dbus")) {
        return QStringLiteral("dbus,%1,%2,%3,%4,%5")
            .arg(obj.value(QStringLiteral("bustype")).toString(),
                 obj.value(QStringLiteral("service")).toString(),
                 obj.value(QStringLiteral("path")).toString(),
                 obj.value(QStringLiteral("interface")).toString(),
                 obj.value(QStringLiteral("method")).toString());
    }
    if (type == QLatin1String("button"))
        return QStringLiteral("button,%1").arg(obj.value(QStringLiteral("button")).toString());
    if (type == QLatin1String("key"))
        return QStringLiteral("key,%1,%2").arg(obj.value(QStringLiteral("modifiers")).toString(),
                                                obj.value(QStringLiteral("keys")).toString());
    // command (default)
    return QStringLiteral("command,%1").arg(obj.value(QStringLiteral("command")).toString());
}

static QString actionJsonToBindingType(const QJsonObject &obj)
{
    const QString type = obj.value(QStringLiteral("type")).toString();
    return type.isEmpty() ? QStringLiteral("WLCOM_CUSTOM_DEF") : type;
}

static QString wlcomShortcutCategory(const QString &type)
{
    if (type == QLatin1String("WLCOM_CUSTOM_DEF"))
        return QStringLiteral("Custom");
    if (type == QLatin1String("WLCOM_SWITCH_WORKSPACE"))
        return QStringLiteral("Workspace");
    if (type.startsWith(QLatin1String("WLCOM_WINDOW_ACTION_")) ||
        type == QLatin1String("WLCOM_WINDOW_SWITCHER") ||
        type == QLatin1String("WLCOM_MAXIMIZED_VIEWS")) {
        return QStringLiteral("Window");
    }

    // System defaults predate the binding type field.  User-created
    // shortcuts are always persisted explicitly as WLCOM_CUSTOM_DEF.
    return QStringLiteral("System");
}

static QSet<QString> wlcomSystemActionBindings(bool *loaded)
{
    QSet<QString> bindings;
    QFile file(QStringLiteral("/etc/gxde-wlcom/config.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        *loaded = false;
        return bindings;
    }

    const QJsonObject keyboard =
        QJsonDocument::fromJson(file.readAll())
            .object()
            .value(QStringLiteral("InputAction"))
            .toObject()
            .value(QStringLiteral("keyboard"))
            .toObject();
    *loaded = !keyboard.isEmpty();
    for (auto it = keyboard.constBegin(); it != keyboard.constEnd(); ++it)
        bindings.insert(it.key().toLower());
    return bindings;
}

static QJsonObject wlcomShortcutItem(const QString &bindings, const QString &name,
                                    const QString &command, const QString &category)
{
    QJsonObject item;
    item[QStringLiteral("Type")] = category == QLatin1String("Custom") ? 1 : 0;
    item[QStringLiteral("Category")] = category;
    item[QStringLiteral("Id")] = bindings;
    item[QStringLiteral("Name")] = name;
    item[QStringLiteral("Exec")] = command;
    QJsonArray accels;
    accels.append(wlcomBindingToDisplay(bindings));
    item[QStringLiteral("Accels")] = accels;
    return item;
}

static QMap<QString, QString> systemXkbLayouts()
{
    QMap<QString, QString> layouts;
    QFile file(QStringLiteral("/usr/share/X11/xkb/rules/base.xml"));
    if (!file.open(QIODevice::ReadOnly))
        return layouts;

    QXmlStreamReader xml(&file);
    bool inLayout = false;
    bool inVariant = false;
    QString layoutName;
    QString layoutDescription;
    QString variantName;
    QString variantDescription;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QStringView name = xml.name();
            if (name == QLatin1String("layout")) {
                inLayout = true;
                layoutName.clear();
                layoutDescription.clear();
            } else if (inLayout && name == QLatin1String("variant")) {
                inVariant = true;
                variantName.clear();
                variantDescription.clear();
            } else if (inLayout && name == QLatin1String("name")) {
                if (inVariant)
                    variantName = xml.readElementText();
                else if (layoutName.isEmpty())
                    layoutName = xml.readElementText();
            } else if (inLayout && name == QLatin1String("description")) {
                if (inVariant)
                    variantDescription = xml.readElementText();
                else if (layoutDescription.isEmpty())
                    layoutDescription = xml.readElementText();
            }
        } else if (xml.isEndElement()) {
            if (xml.name() == QLatin1String("variant")) {
                if (!layoutName.isEmpty() && !variantName.isEmpty() &&
                    !variantDescription.isEmpty()) {
                    layouts.insert(layoutName + QLatin1Char(';') + variantName,
                                   variantDescription);
                }
                inVariant = false;
            } else if (xml.name() == QLatin1String("layout")) {
                if (!layoutName.isEmpty() && !layoutDescription.isEmpty())
                    layouts.insert(layoutName, layoutDescription);
                inLayout = false;
            }
        }
    }
    return layouts;
}

static int switchMaskFromOptions(const QString &options)
{
    int mask = 0;
    const QStringList values = options.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (values.contains(QStringLiteral("grp:ctrl_shift_toggle")))
        mask |= 1;
    if (values.contains(QStringLiteral("grp:alt_shift_toggle")))
        mask |= 2;
    if (values.contains(QStringLiteral("grp:win_space_toggle")))
        mask |= 4;
    return mask;
}

static QString withoutGroupOptions(const QString &options)
{
    QStringList values;
    for (const QString &option : options.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        if (!option.startsWith(QLatin1String("grp:")))
            values.append(option);
    }
    return values.join(QLatin1Char(','));
}

KeyboardWorker::KeyboardWorker(KeyboardModel *model, QObject *parent)
    : QObject(parent),
      m_model(model),
      m_keyboardInter(new KeyboardInter("com.deepin.daemon.InputDevices",
                                        "/com/deepin/daemon/InputDevice/Keyboard",
                                        QDBusConnection::sessionBus(), this)),
#ifndef DCC_DISABLE_LANGUAGE
      m_langSelector(new LangSelector("com.deepin.daemon.LangSelector",
                                      "/com/deepin/daemon/LangSelector",
                                      QDBusConnection::sessionBus(), this)),
#endif
      m_keybindInter(new KeybingdingInter("com.deepin.daemon.Keybinding",
                                          "/com/deepin/daemon/Keybinding",
                                          QDBusConnection::sessionBus(), this))
{
    const bool wayland = Dtk::Widget::DApplication::isWayland();
    if (!wayland) {
        connect(m_keybindInter, SIGNAL(Added(QString,int)), this,SLOT(onAdded(QString,int)));
        connect(m_keybindInter, &KeybingdingInter::Deleted, this, &KeyboardWorker::removed);
#ifndef DCC_DISABLE_KBLAYOUT
        connect(m_keyboardInter, &KeyboardInter::UserLayoutListChanged, this, &KeyboardWorker::onUserLayout);
        connect(m_keyboardInter, &KeyboardInter::CurrentLayoutChanged, this, &KeyboardWorker::onCurrentLayout);
#endif
        connect(m_keyboardInter, SIGNAL(CapslockToggleChanged(bool)), m_model, SLOT(setCapsLock(bool)));
        connect(m_keybindInter, &KeybingdingInter::NumLockStateChanged, m_model, &KeyboardModel::setNumLock);
        connect(m_keyboardInter, &KeyboardInter::RepeatDelayChanged, this, &KeyboardWorker::setModelRepeatDelay);
        connect(m_keyboardInter, &KeyboardInter::RepeatIntervalChanged, this, &KeyboardWorker::setModelRepeatInterval);
        connect(m_keybindInter, &KeybingdingInter::ShortcutSwitchLayoutChanged, m_model, &KeyboardModel::setKbSwitch);
        connect(m_keybindInter, &KeybingdingInter::Changed, this, &KeyboardWorker::onShortcutChanged);

        m_keyboardInter->setSync(false);
        m_keybindInter->setSync(false);
    }
#ifndef DCC_DISABLE_LANGUAGE
    connect(m_langSelector, &LangSelector::CurrentLocaleChanged, m_model, &KeyboardModel::setLang);
    connect(m_langSelector, &LangSelector::serviceStartFinished, this, [=] {
        QTimer::singleShot(100, this, &KeyboardWorker::onLangSelectorServiceFinished);
    });
#endif
#ifndef DCC_DISABLE_LANGUAGE
    m_langSelector->setSync(false, false);
#endif
}

void KeyboardWorker::resetAll() {
    if (Dtk::Widget::DApplication::isWayland())
        return;

    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(m_keybindInter->Reset(), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [=] (QDBusPendingCallWatcher *reply) {
        watcher->deleteLater();

        if (reply->isError()) {
            qWarning() << Q_FUNC_INFO << reply->error();
        }
    });
}

void KeyboardWorker::setShortcutModel(ShortcutModel *model)
{
    m_shortcutModel = model;

    connect(m_keybindInter, &KeybingdingInter::KeyEvent, model, &ShortcutModel::keyEvent);

    // Under Wayland the key capture is provided by gxde-wlcom's GrabNextKey,
    // deliver its KeyEvent signal in the format the shortcut UI expects.
    if (Dtk::Widget::DApplication::isWayland()) {
        QDBusConnection::sessionBus().connect(
            GxdeInput::Service, GxdeInput::ShortcutPath, GxdeInput::ShortcutInterface,
            QStringLiteral("KeyEvent"), this,
            SLOT(onWlcomKeyEvent(bool, QString)));
        QDBusConnection::sessionBus().connect(
            GxdeInput::Service, GxdeInput::Path, GxdeInput::Interface,
            QStringLiteral("KeymapGroupChanged"), this,
            SLOT(onWlcomKeymapGroupChanged(QString, uint)));
    }
}

void KeyboardWorker::onWlcomKeymapGroupChanged(const QString &device, uint group)
{
    Q_UNUSED(device)

    if (group >= static_cast<uint>(m_waylandLayouts.size())) {
        return;
    }

    m_waylandGroup = static_cast<int>(group);
    const QString id = m_waylandLayouts.at(m_waylandGroup);
    m_model->setLayout(m_model->kbLayout().value(id, id));
    saveWaylandLayouts();
}

void KeyboardWorker::onWlcomKeyEvent(bool pressed, const QString &shortcut)
{
    if (m_shortcutModel)
        m_shortcutModel->keyEvent(pressed, wlcomBindingToDisplay(shortcut));
}

void KeyboardWorker::refreshShortcut()
{
    // Under Wayland the X11 based com.deepin.daemon.Keybinding is disabled,
    // so enumerate the shortcuts held by gxde-wlcom instead.
    if (Dtk::Widget::DApplication::isWayland()) {
        QJsonArray array;
        QSet<QString> listedBindings;
        bool systemBindingsLoaded = false;
        const QSet<QString> systemBindings =
            wlcomSystemActionBindings(&systemBindingsLoaded);
        const QList<QPair<QString, QString>> actions = GxdeInput::listShortcuts();
        for (const auto &action : actions) {
            const QString &bindings = action.first;
            // skip gesture actions, only keyboard shortcuts are shown here
            if (bindings.startsWith(QLatin1String("hold:")) ||
                bindings.startsWith(QLatin1String("swipe:")) ||
                bindings.startsWith(QLatin1String("pinch:")))
                continue;

            const QJsonObject obj = QJsonDocument::fromJson(action.second.toUtf8()).object();
            if (obj.isEmpty() || !obj.value(QStringLiteral("enable")).toBool(true))
                continue;

            QString type = obj.value(QStringLiteral("type")).toString();
            if (type.isEmpty() && systemBindingsLoaded &&
                !systemBindings.contains(bindings.toLower())) {
                type = QStringLiteral("WLCOM_CUSTOM_DEF");
            }
            const QString category = wlcomShortcutCategory(type);
            array.append(wlcomShortcutItem(
                bindings, obj.value(QStringLiteral("desc")).toString(bindings),
                obj.value(QStringLiteral("command")).toString(), category));
            listedBindings.insert(bindings.toLower());
        }

        for (const GxdeInput::KeyBinding &binding : GxdeInput::listKeyBindings()) {
            if (listedBindings.contains(binding.bindings.toLower()) ||
                binding.type == QLatin1String("WLCOM_CUSTOM_DEF")) {
                continue;
            }
            array.append(wlcomShortcutItem(
                binding.bindings,
                binding.description.isEmpty() ? binding.bindings : binding.description, QString(),
                wlcomShortcutCategory(binding.type)));
        }
        m_shortcutModel->onParseInfo(QString::fromUtf8(QJsonDocument(array).toJson()));
        return;
    }

    QDBusPendingCallWatcher *result = new QDBusPendingCallWatcher(m_keybindInter->ListAllShortcuts(), this);
    connect(result, SIGNAL(finished(QDBusPendingCallWatcher*)), this,
            SLOT(onRequestShortcut(QDBusPendingCallWatcher*)));
}

#ifndef DCC_DISABLE_LANGUAGE
void KeyboardWorker::refreshLang()
{
    m_langSelector->blockSignals(false);
    if (!m_langSelector->isValid())
        m_langSelector->startServiceProcess();
    else
        onLangSelectorServiceFinished();
}
#endif

void KeyboardWorker::active()
{
    const bool wayland = Dtk::Widget::DApplication::isWayland();
    if (wayland) {
        int rate = 0, delay = 0;
        if (GxdeInput::getRepeatInfo(GxdeInput::keyboardDevices(), &rate, &delay)) {
            setModelRepeatDelay(delay);
            setModelRepeatInterval(rate > 0 ? qMax(1, 1000 / rate) : 50);
        }
    } else {
        m_keyboardInter->blockSignals(false);
        m_keybindInter->blockSignals(false);
        setModelRepeatDelay(m_keyboardInter->repeatDelay());
        setModelRepeatInterval(m_keyboardInter->repeatInterval());
    }

    m_metaDatas.clear();
    m_letters.clear();

    Q_EMIT onDatasChanged(m_metaDatas);
    Q_EMIT onLettersChanged(m_letters);

    if (!wayland) {
        m_model->setCapsLock(m_keyboardInter->capslockToggle());
        m_model->setNumLock(m_keybindInter->numLockState());
    }
#ifndef DCC_DISABLE_KBLAYOUT
    onRefreshKBLayout();
#endif
#ifndef DCC_DISABLE_LANGUAGE
    refreshLang();
#endif
}

void KeyboardWorker::deactive()
{
    if (!Dtk::Widget::DApplication::isWayland()) {
        m_keyboardInter->blockSignals(true);
        m_keybindInter->blockSignals(true);
    }
#ifndef DCC_DISABLE_LANGUAGE
    m_langSelector->blockSignals(true);
#endif
}

bool KeyboardWorker::keyOccupy(const QStringList &list)
{
    int bit = 0;
    for (QString t : list) {
        if (t == "Control")
            bit +=  Modifier::control;
        else if (t == "Alt")
            bit += Modifier::alt;
        else if (t == "Super")
            bit += Modifier::super;
        else if (t == "Shift")
            bit += Modifier::shift;
        else
            continue;
    }

    QMap<QStringList,int> keylist = m_model->allShortcut();
    QMap<QStringList, int>::iterator i;
    for (i = keylist.begin(); i != keylist.end(); ++i) {
        if (bit == i.value() && i.key().last() == list.last()) {
            return false;
        }
    }

    return true;
}

#ifndef DCC_DISABLE_KBLAYOUT
void KeyboardWorker::onRefreshKBLayout()
{
    if (Dtk::Widget::DApplication::isWayland()) {
        refreshWaylandLayouts();
        return;
    }

    m_model->setKbSwitch(m_keybindInter->shortcutSwitchLayout());

    QDBusPendingCallWatcher *layoutResult = new QDBusPendingCallWatcher(m_keyboardInter->LayoutList(), this);
    connect(layoutResult, &QDBusPendingCallWatcher::finished, this, &KeyboardWorker::onLayoutListsFinished);

    onCurrentLayout(m_keyboardInter->currentLayout());
    onUserLayout(m_keyboardInter->userLayoutList());
}
#endif

void KeyboardWorker::modifyShortcutEdit(ShortcutInfo *info)
{
    if (!info)
        return;

    if (Dtk::Widget::DApplication::isWayland()) {
        // gxde-wlcom has no "clear keystrokes" op; replace the action by
        // removing the old binding and registering the new one.
        QJsonObject old;
        const QList<QPair<QString, QString>> actions = GxdeInput::listShortcuts();
        for (const auto &action : actions) {
            if (action.first == info->id) {
                old = QJsonDocument::fromJson(action.second.toUtf8()).object();
                break;
            }
        }
        if (old.isEmpty()) {
            refreshShortcut();
            return;
        }
        if (info->replace)
            onDisableShortcut(info->replace);
        GxdeInput::controlShortcut(QStringLiteral("delete"), info->id);
        GxdeInput::addShortcut(displayToWlcomBinding(info->accels), info->name,
                               actionJsonToBusString(old), actionJsonToBindingType(old));
        refreshShortcut();
        return;
    }

    if (info->replace) {
        onDisableShortcut(info->replace);
    }

    const QString &shortcut = info->accels;
    const QString &result = m_keybindInter->LookupConflictingShortcut(shortcut);

    if (!result.isEmpty()) {
        const QJsonObject obj = QJsonDocument::fromJson(result.toLatin1()).object();
        QDBusPendingCall call = m_keybindInter->ClearShortcutKeystrokes(obj["Id"].toString(), obj["Type"].toInt());
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);

        watcher->setProperty("id", info->id);
        watcher->setProperty("type", info->type);
        watcher->setProperty("shortcut", shortcut);

        connect(watcher, &QDBusPendingCallWatcher::finished, this, &KeyboardWorker::onConflictShortcutCleanFinished);
    } else {
        cleanShortcutSlef(info->id, info->type, shortcut);
    }
}

void KeyboardWorker::addCustomShortcut(const QString &name, const QString &command, const QString &accels)
{
    if (Dtk::Widget::DApplication::isWayland()) {
        GxdeInput::addShortcut(displayToWlcomBinding(accels), name,
                               QStringLiteral("command,") + command,
                               QStringLiteral("WLCOM_CUSTOM_DEF"));
        refreshShortcut();
        return;
    }
    m_keybindInter->AddCustomShortcut(name, command, accels);
}

void KeyboardWorker::modifyCustomShortcut(ShortcutInfo *info)
{
    if (Dtk::Widget::DApplication::isWayland()) {
        // Replace the custom shortcut in gxde-wlcom: remove the old binding
        // and add it back with the new keystroke/name/command.
        if (info->replace)
            onDisableShortcut(info->replace);
        info->replace = nullptr;
        GxdeInput::controlShortcut(QStringLiteral("delete"), info->id);
        GxdeInput::addShortcut(displayToWlcomBinding(info->accels), info->name,
                               QStringLiteral("command,") + info->command,
                               QStringLiteral("WLCOM_CUSTOM_DEF"));
        refreshShortcut();
        return;
    }

    if (info->replace) {
        onDisableShortcut(info->replace);
    }

    // reset replace shortcut
    info->replace = nullptr;

    const QString &result = m_keybindInter->LookupConflictingShortcut(info->accels);

    if (!result.isEmpty()) {
        const QJsonObject obj = QJsonDocument::fromJson(result.toLatin1()).object();
        QDBusPendingCall call = m_keybindInter->ClearShortcutKeystrokes(obj["Id"].toString(), obj["Type"].toInt());
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);

        watcher->setProperty("id", info->id);
        watcher->setProperty("name", info->name);
        watcher->setProperty("command", info->command);
        watcher->setProperty("shortcut", info->accels);

        connect(watcher, &QDBusPendingCallWatcher::finished, this, &KeyboardWorker::onCustomConflictCleanFinished);
    } else {
        m_keybindInter->ModifyCustomShortcut(info->id, info->name, info->command, info->accels);
    }
}

void KeyboardWorker::grabScreen()
{
    if (Dtk::Widget::DApplication::isWayland()) {
        GxdeInput::grabNextKey();
        return;
    }
    m_keybindInter->GrabScreen();
}

bool KeyboardWorker::checkAvaliable(const QString &key)
{
   if (Dtk::Widget::DApplication::isWayland()) {
       const QString binding = displayToWlcomBinding(key);
       const QList<QPair<QString, QString>> actions = GxdeInput::listShortcuts();
       return std::none_of(actions.cbegin(), actions.cend(), [&binding](const auto &action) {
           return action.first == binding;
       });
   }

   const QString &value = m_keybindInter->LookupConflictingShortcut(key);

   return value.isEmpty();
}

void KeyboardWorker::delShortcut(ShortcutInfo* info)
{
    if (Dtk::Widget::DApplication::isWayland()) {
        GxdeInput::controlShortcut(QStringLiteral("delete"), info->id);
        m_shortcutModel->delInfo(info);
        return;
    }
    m_keybindInter->DeleteCustomShortcut(info->id);
    m_shortcutModel->delInfo(info);
}

void KeyboardWorker::setRepeatDelay(int value)
{
    if (Dtk::Widget::DApplication::isWayland()) {
        int rate = 0, delay = 0;
        GxdeInput::getRepeatInfo(GxdeInput::keyboardDevices(), &rate, &delay);
        GxdeInput::setRepeatInfo(GxdeInput::keyboardDevices(), rate, converToDBusDelay(value));
        return;
    }
    m_keyboardInter->setRepeatDelay(converToDBusDelay(value));
}

void KeyboardWorker::setRepeatInterval(int value)
{
    if (Dtk::Widget::DApplication::isWayland()) {
        int rate = 0, delay = 0;
        GxdeInput::getRepeatInfo(GxdeInput::keyboardDevices(), &rate, &delay);
        const int interval = converToDBusInterval(value);
        GxdeInput::setRepeatInfo(GxdeInput::keyboardDevices(), interval > 0 ? qMax(1, 1000 / interval) : 25, delay);
        return;
    }
    m_keyboardInter->setRepeatInterval(converToDBusInterval(value));
}

void KeyboardWorker::setModelRepeatDelay(int value)
{
    m_model->setRepeatDelay(converToModelDelay(value));
}

void KeyboardWorker::setModelRepeatInterval(int value)
{
    m_model->setRepeatInterval(converToModelInterval(value));
}

void KeyboardWorker::setNumLock(bool value)
{
    if (Dtk::Widget::DApplication::isWayland())
        return;
    m_keybindInter->SetNumLockState(value);
}

void KeyboardWorker::setCapsLock(bool value)
{
    if (Dtk::Widget::DApplication::isWayland())
        return;
    m_keyboardInter->setCapslockToggle(value);
}

void KeyboardWorker::addUserLayout(const QString &value)
{
    if (Dtk::Widget::DApplication::isWayland()) {
        const QString id = m_model->kbLayout().key(value);
        if (id.isEmpty() || m_waylandLayouts.contains(id))
            return;
        m_waylandLayouts.append(id);
        m_model->addUserLayout(id, value);
        saveWaylandLayouts();
        applyWaylandLayouts();
        return;
    }
    m_keyboardInter->AddUserLayout(m_model->kbLayout().key(value));
}

void KeyboardWorker::delUserLayout(const QString &value)
{
    if (Dtk::Widget::DApplication::isWayland()) {
        const QString id = m_model->userLayout().key(value);
        if (id.isEmpty() || m_waylandLayouts.size() <= 1)
            return;
        const int removedIndex = m_waylandLayouts.indexOf(id);
        m_waylandLayouts.removeAll(id);
        if (removedIndex < m_waylandGroup)
            --m_waylandGroup;
        else if (removedIndex == m_waylandGroup)
            m_waylandGroup = qMin(m_waylandGroup, m_waylandLayouts.size() - 1);
        saveWaylandLayouts();
        applyWaylandLayouts();
        QTimer::singleShot(0, this, [this] {
            m_model->cleanUserLayout();
            for (const QString &layout : std::as_const(m_waylandLayouts))
                m_model->addUserLayout(layout, m_model->kbLayout().value(layout, layout));
            const QString current = m_waylandLayouts.at(m_waylandGroup);
            m_model->setLayout(m_model->kbLayout().value(current, current));
        });
        return;
    }
    m_keyboardInter->DeleteUserLayout(m_model->userLayout().key(value));
}

bool caseInsensitiveLessThan(const MetaData &s1, const MetaData &s2)
{
    QCollator qc;
    int i = qc.compare(s1.text(), s2.text());
    if (i < 0)
        return true;
    else
        return false;
}

void KeyboardWorker::onRequestShortcut(QDBusPendingCallWatcher *watch)
{
    QDBusPendingReply<QString> reply = *watch;
    if(reply.isError())
    {
        watch->deleteLater();
        return;
    }

    QString info = reply.value();

    QMap<QStringList,int> map;
    QJsonArray array = QJsonDocument::fromJson(info.toStdString().c_str()).array();
    Q_FOREACH(QJsonValue value, array) {
        QJsonObject obj = value.toObject();
        if (obj.isEmpty())
            continue;
        if (obj["Accels"].toArray().isEmpty())
            continue;
        QString accels = obj["Accels"].toArray().at(0).toString();
        accels.replace("<", "");
        accels.replace(">", "-");
        //转换为list
        QStringList key;
        key = accels.split("-");
        int bit = 0;
        for (QString &t : key) {
            if (t == "Control")
                bit += Modifier::control;
            else if (t == "Alt")
                bit += Modifier::alt;
            else if (t == "Super")
                bit += Modifier::super;
            else if (t == "Shift")
                bit += Modifier::shift;
            else {
                QString s = t;
                s = ModelKeycode.value(s);
                if (!s.isEmpty())
                    t = s;
            }
        }
        if (bit == 0)
            continue;

        map.insert(key, bit);
    }
    m_model->setAllShortcut(map);
    m_shortcutModel->onParseInfo(info);
    watch->deleteLater();
}

void KeyboardWorker::onAdded(const QString &in0, int in1)
{
    QDBusPendingReply<QString> reply = m_keybindInter->GetShortcut(in0, in1);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &KeyboardWorker::onAddedFinished);
}

void KeyboardWorker::onDisableShortcut(ShortcutInfo *info)
{
    if (Dtk::Widget::DApplication::isWayland()) {
        GxdeInput::controlShortcut(QStringLiteral("disable"), info->id);
        info->accels.clear();
        return;
    }

    // disable shortcut need wait!
    m_keybindInter->ClearShortcutKeystrokes(info->id, info->type).waitForFinished();
    info->accels.clear();
}

void KeyboardWorker::onAddedFinished(QDBusPendingCallWatcher *watch)
{
    QDBusPendingReply<QString> reply = *watch;

    if (!watch->isError())
        m_shortcutModel->onCustomInfo(reply.value());

    watch->deleteLater();
}

#ifndef DCC_DISABLE_KBLAYOUT
void KeyboardWorker::onLayoutListsFinished(QDBusPendingCallWatcher *watch)
{
    QDBusPendingReply<QMap<QString,QString>> reply = *watch;

    QMap<QString,QString> tmp_map = reply.value();
    m_model->setLayoutLists(tmp_map);

    watch->deleteLater();
}
#endif

void KeyboardWorker::onLocalListsFinished(QDBusPendingCallWatcher *watch)
{
    QDBusPendingReply<LocaleList> reply = *watch;

    m_datas.clear();

    LocaleList list = reply.value();

    for (int i = 0; i!=list.size(); ++i) {
        MetaData md;
        md.setKey(list.at(i).id);
        md.setText(list.at(i).name);
        m_datas.append(md);
    }

    std::sort(m_datas.begin(), m_datas.end(), caseInsensitiveLessThan);

    m_model->setLocaleList(m_datas);

    watch->deleteLater();
}

void KeyboardWorker::onSetSwitchKBLayout(int value)
{
    if (Dtk::Widget::DApplication::isWayland()) {
        m_waylandSwitch = value;
        m_model->setKbSwitch(value);
        saveWaylandLayouts();
        applyWaylandLayouts();
        return;
    }
    m_keybindInter->setShortcutSwitchLayout(value);
}

#ifndef DCC_DISABLE_KBLAYOUT
void KeyboardWorker::onUserLayout(const QStringList &list)
{
    m_model->cleanUserLayout();

    for (const QString &data : list) {
        QDBusPendingCallWatcher *layoutResult = new QDBusPendingCallWatcher(m_keyboardInter->GetLayoutDesc(data), this);
        layoutResult->setProperty("id", data);
        connect(layoutResult, &QDBusPendingCallWatcher::finished, this, &KeyboardWorker::onUserLayoutFinished);
    }
}

void KeyboardWorker::onUserLayoutFinished(QDBusPendingCallWatcher *watch)
{
    QDBusPendingReply<QString> reply = *watch;

    m_model->addUserLayout(watch->property("id").toString(), reply.value());

    watch->deleteLater();
}

void KeyboardWorker::onCurrentLayout(const QString &value)
{
    QDBusPendingCallWatcher *layoutResult = new QDBusPendingCallWatcher(m_keyboardInter->GetLayoutDesc(value), this);
    connect(layoutResult, &QDBusPendingCallWatcher::finished, this, &KeyboardWorker::onCurrentLayoutFinished);
}

void KeyboardWorker::onCurrentLayoutFinished(QDBusPendingCallWatcher *watch)
{
    QDBusPendingReply<QString> reply = *watch;

    m_model->setLayout(reply.value());

    watch->deleteLater();
}

void KeyboardWorker::onPinyin()
{
    m_letters.clear();
    m_metaDatas.clear();
    QDBusInterface dbus_pinyin("com.deepin.api.Pinyin", "/com/deepin/api/Pinyin",
                               "com.deepin.api.Pinyin");

    Q_FOREACH(const QString & str, m_model->kbLayout().keys()) {
        MetaData md;
        QString title = m_model->kbLayout()[str];
        md.setText(title);
        md.setKey(str);
        QChar letterFirst = title[0];
        QStringList letterFirstList;
        if (letterFirst.isLower() || letterFirst.isUpper()) {
            letterFirstList << QString(letterFirst);
            md.setPinyin(title);
        } else {
            QDBusMessage message = dbus_pinyin.call("Query", title);
            letterFirstList = message.arguments()[0].toStringList();
            md.setPinyin(letterFirstList.at(0));
        }

        append(md);
    }

    QLocale locale;

    if (locale.language() == QLocale::Chinese) {
        QChar ch = '\0';
        for (int i(0); i != m_metaDatas.size(); ++i)
        {
            const QChar flag = m_metaDatas[i].pinyin().at(0).toUpper();
            if (flag == ch)
                continue;
            ch = flag;

            m_letters.append(ch);
            m_metaDatas.insert(i, MetaData(ch, true));
        }
    } else {
        std::sort(m_metaDatas.begin(), m_metaDatas.end(), caseInsensitiveLessThan);
    }

    Q_EMIT onDatasChanged(m_metaDatas);
    Q_EMIT onLettersChanged(m_letters);
}

void KeyboardWorker::append(const MetaData &md)
{
    if(m_metaDatas.count() == 0)
    {
        m_metaDatas.append(md);
        return;
    }

    int index = 0;
    for (int i = 0; i != m_metaDatas.size(); ++i) {
        if(m_metaDatas.at(i) > md)
        {
            m_metaDatas.insert(index,md);
            return;
        }
        index++;
    }

    m_metaDatas.append(md);
}
#endif

#ifndef DCC_DISABLE_LANGUAGE
void KeyboardWorker::onLangSelectorServiceFinished()
{
    QDBusPendingCallWatcher *localResult = new QDBusPendingCallWatcher(m_langSelector->GetLocaleList(), this);
    connect(localResult, &QDBusPendingCallWatcher::finished, this, &KeyboardWorker::onLocalListsFinished);
    m_langSelector->currentLocale();
}
#endif

void KeyboardWorker::onShortcutChanged(const QString &id, int type)
{
    QDBusPendingCallWatcher *result = new QDBusPendingCallWatcher(m_keybindInter->Query(id, type));
    connect(result, &QDBusPendingCallWatcher::finished, this, &KeyboardWorker::onGetShortcutFinished);
}

void KeyboardWorker::onGetShortcutFinished(QDBusPendingCallWatcher *watch)
{
    QDBusPendingReply<QString> reply = *watch;

    if (!watch->isError())
        m_shortcutModel->onKeyBindingChanged(reply.value());

    watch->deleteLater();
}

void KeyboardWorker::updateKey(ShortcutInfo *info)
{
    m_shortcutModel->setCurrentInfo(info);

    if (Dtk::Widget::DApplication::isWayland()) {
        GxdeInput::grabNextKey();
        return;
    }
    m_keybindInter->SelectKeystroke();
}

void KeyboardWorker::cleanShortcutSlef(const QString &id, const int type, const QString &shortcut)
{
    QDBusPendingCall call = m_keybindInter->ClearShortcutKeystrokes(id, type);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);

    watcher->setProperty("id", id);
    watcher->setProperty("type", type);
    watcher->setProperty("shortcut", shortcut);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, &KeyboardWorker::onShortcutCleanFinished);
}

void KeyboardWorker::setNewCustomShortcut(const QString &id, const QString &name, const QString &command, const QString &accles)
{
    m_keybindInter->ModifyCustomShortcut(id, name, command, accles);
}

void KeyboardWorker::onConflictShortcutCleanFinished(QDBusPendingCallWatcher *watch)
{
    if (!watch->isError()) {
        const QString &id = watch->property("id").toString();
        const int type = watch->property("type").toInt();
        const QString &shortcut = watch->property("shortcut").toString();

        cleanShortcutSlef(id, type, shortcut);
    }

    watch->deleteLater();
}

void KeyboardWorker::onShortcutCleanFinished(QDBusPendingCallWatcher *watch)
{
    if (!watch->isError()) {
        const QString &id = watch->property("id").toString();
        const int type = watch->property("type").toInt();
        const QString &shortcut = watch->property("shortcut").toString();

        m_keybindInter->AddShortcutKeystroke(id, type, shortcut);
    } else {
        qDebug() << watch->error();
    }

    watch->deleteLater();
}

void KeyboardWorker::onCustomConflictCleanFinished(QDBusPendingCallWatcher *w)
{
    if (!w->isError()) {
        const QString &id = w->property("id").toString();
        const QString name = w->property("name").toString();
        const QString &command = w->property("command").toString();
        const QString &accles = w->property("shortcut").toString();

        setNewCustomShortcut(id, name, command, accles);
    }

    w->deleteLater();
}

int KeyboardWorker::converToDBusDelay(int value)
{
    switch (value) {
    case 1:
        return 20;
    case 2:
        return 80;
    case 3:
        return 150;
    case 4:
        return 250;
    case 5:
        return 360;
    case 6:
        return 480;
    case 7:
        return 600;
    default:
        return 4;
    }
}

int KeyboardWorker::converToModelDelay(int value)
{
    if (value <= 20)
        return 1;
    else if (value <= 80)
        return 2;
    else if (value <= 150)
        return 3;
    else if (value <= 250)
        return 4;
    else if (value <= 360)
        return 5;
    else if (value <= 480)
        return 6;
    else
        return 7;
}

int KeyboardWorker::converToDBusInterval(int value)
{
    switch (value) {
    case 1:
        return 100;
    case 2:
        return 80;
    case 3:
        return 65;
    case 4:
        return 50;
    case 5:
        return 35;
    case 6:
        return 25;
    case 7:
        return 20;
    default:
        return 4;
    }
}

int KeyboardWorker::converToModelInterval(int value)
{
    if (value <= 20)
        return 7;
    else if (value <= 25)
        return 6;
    else if (value <= 35)
        return 5;
    else if (value <= 50)
        return 4;
    else if (value <= 65)
        return 3;
    else if (value <= 80)
        return 2;
    else
        return 1;
}

void KeyboardWorker::setLayout(const QString &value)
{
    if (Dtk::Widget::DApplication::isWayland()) {
        const int group = m_waylandLayouts.indexOf(value);
        if (group < 0)
            return;
        if (!GxdeInput::setKeymapGroup(GxdeInput::keyboardDevices(), group))
            return;
        m_waylandGroup = group;
        m_model->setLayout(m_model->kbLayout().value(value, value));
        saveWaylandLayouts();
        return;
    }
    m_keyboardInter->setCurrentLayout(value);
}

void KeyboardWorker::refreshWaylandLayouts()
{
    const QMap<QString, QString> layouts = systemXkbLayouts();
    m_model->setLayoutLists(layouts);

    const bool hasKeymap =
        GxdeInput::getKeymap(GxdeInput::keyboardDevices(), &m_waylandKeymap);
    if (!hasKeymap)
        m_waylandKeymap = GxdeInput::Keymap();

    QSettings settings;
    settings.beginGroup(QStringLiteral("WaylandKeyboard"));
    m_waylandLayouts = settings.value(QStringLiteral("layouts")).toStringList();

    if (m_waylandLayouts.isEmpty() && !m_waylandKeymap.layout.isEmpty()) {
        const QStringList names = m_waylandKeymap.layout.split(QLatin1Char(','));
        const QStringList variants = m_waylandKeymap.variant.split(QLatin1Char(','));
        for (int i = 0; i < names.size(); ++i) {
            QString id = names.at(i);
            if (i < variants.size() && !variants.at(i).isEmpty())
                id += QLatin1Char(';') + variants.at(i);
            if (layouts.contains(id) && !m_waylandLayouts.contains(id))
                m_waylandLayouts.append(id);
        }
    }

    for (auto it = m_waylandLayouts.begin(); it != m_waylandLayouts.end();) {
        if (!layouts.contains(*it))
            it = m_waylandLayouts.erase(it);
        else
            ++it;
    }
    if (m_waylandLayouts.isEmpty()) {
        m_waylandLayouts.append(layouts.isEmpty() || layouts.contains(QStringLiteral("us"))
                                    ? QStringLiteral("us")
                                    : layouts.firstKey());
    }

    quint32 activeGroup = 0;
    if (GxdeInput::getKeymapGroup(GxdeInput::keyboardDevices(), &activeGroup) &&
        activeGroup < static_cast<quint32>(m_waylandLayouts.size())) {
        m_waylandGroup = static_cast<int>(activeGroup);
    } else {
        m_waylandGroup = qMax(0, m_waylandLayouts.indexOf(
                                      settings.value(QStringLiteral("current")).toString()));
    }
    m_waylandSwitch = settings.contains(QStringLiteral("switchMask"))
                          ? settings.value(QStringLiteral("switchMask")).toInt()
                          : switchMaskFromOptions(m_waylandKeymap.options);
    settings.endGroup();

    m_waylandKeymap.options = withoutGroupOptions(m_waylandKeymap.options);
    m_model->cleanUserLayout();
    for (const QString &layout : std::as_const(m_waylandLayouts))
        m_model->addUserLayout(layout, layouts.value(layout, layout));
    const QString current = m_waylandLayouts.at(m_waylandGroup);
    m_model->setLayout(layouts.value(current, current));
    m_model->setKbSwitch(m_waylandSwitch);
    saveWaylandLayouts();
}

void KeyboardWorker::applyWaylandLayouts()
{
    if (m_waylandLayouts.isEmpty())
        return;

    QStringList names;
    QStringList variants;
    for (const QString &id : std::as_const(m_waylandLayouts)) {
        const int separator = id.indexOf(QLatin1Char(';'));
        names.append(separator < 0 ? id : id.left(separator));
        variants.append(separator < 0 ? QString() : id.mid(separator + 1));
    }

    GxdeInput::Keymap keymap = m_waylandKeymap;
    keymap.layout = names.join(QLatin1Char(','));
    keymap.variant = variants.join(QLatin1Char(','));
    QStringList options = keymap.options.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (m_waylandSwitch & 1)
        options.append(QStringLiteral("grp:ctrl_shift_toggle"));
    if (m_waylandSwitch & 2)
        options.append(QStringLiteral("grp:alt_shift_toggle"));
    if (m_waylandSwitch & 4)
        options.append(QStringLiteral("grp:win_space_toggle"));
    keymap.options = options.join(QLatin1Char(','));

    if (!GxdeInput::setKeymap(GxdeInput::keyboardDevices(), keymap))
        qWarning() << "Failed to apply the Wayland keyboard layout" << keymap.layout;
    else if (!GxdeInput::setKeymapGroup(GxdeInput::keyboardDevices(), m_waylandGroup))
        qWarning() << "Failed to restore the Wayland keyboard layout group" << m_waylandGroup;
}

void KeyboardWorker::saveWaylandLayouts()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("WaylandKeyboard"));
    settings.setValue(QStringLiteral("layouts"), m_waylandLayouts);
    settings.setValue(QStringLiteral("current"),
                      m_waylandLayouts.isEmpty() ? QString()
                                                 : m_waylandLayouts.at(m_waylandGroup));
    settings.setValue(QStringLiteral("switchMask"), m_waylandSwitch);
    settings.endGroup();
}

#ifndef DCC_DISABLE_LANGUAGE
void KeyboardWorker::setLang(const QString &value)
{
    Q_EMIT requestSetAutoHide(false);

    QDBusPendingCall call = m_langSelector->SetLocale(value);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [=] {
        if (call.isError())
            m_model->setLang(m_langSelector->currentLocale());

        Q_EMIT requestSetAutoHide(true);
        watcher->deleteLater();
    });
}
#endif

}
}
