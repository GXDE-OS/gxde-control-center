/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
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

import QtQuick
import QtQuick.Controls.Gxde
import org.gxde.controlcenter

// The home page, like the old MainWidget: header (avatar, time, date, notify
// button), update notice, module grid or notifications, and quick settings
// under the notifications.
Item {
    id: home

    required property UserInfo userInfo
    required property HomeWidgets homeWidgets
    required property ModuleHost moduleHost

    property bool notifyShown: false

    signal avatarClicked()
    signal moduleClicked(string module)

    function showNotify() {
        notifyShown = true
    }

    function showNav() {
        notifyShown = false
    }

    onNotifyShownChanged: homeWidgets.setQuickControlPanelVisible(notifyShown)

    Timer {
        interval: 1000
        repeat: true
        running: true
        triggeredOnStart: true
        onTriggered: {
            const now = new Date()
            timeLabel.text = Qt.formatTime(now, "HH:mm")
            dateLabel.text = now.toLocaleString(Qt.locale(), Locale.LongFormat)
        }
    }

    // 1 px margin on each side, like before.
    Column {
        id: central

        x: 1
        y: 1
        width: parent.width - 2
        height: parent.height - 2

        Item {
            id: header

            width: parent.width
            height: 140

            // Center the avatar below the 10 px top margin.
            Avatar {
                id: avatar

                x: 40
                y: 10 + (header.height - 10 - 10 - height) / 2
                width: 60
                height: 60
                visible: home.userInfo.available
                source: home.userInfo.avatarFor(Screen.devicePixelRatio)
                onClicked: home.avatarClicked()
            }

            // Center time and date above the 10 px bottom margin.
            Item {
                id: timeDate

                x: 40 + 60 + 30
                width: notifyToggle.x - 30 - x
                height: header.height - 10

                Column {
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: timeLabel

                        width: timeDate.width
                        color: "white"
                        font.pixelSize: 46
                        font.weight: Font.Light
                    }

                    Text {
                        id: dateLabel

                        // Minimum width of the old date label.
                        width: Math.max(timeDate.width, 408 - 150)
                        color: "white"
                        wrapMode: Text.WordWrap
                    }
                }
            }

            ImageButton {
                id: notifyToggle

                x: header.width - 20 - width
                y: (header.height - 10 - height) / 2
                width: 32
                height: 32
                normalSource: home.notifyShown
                              ? "qrc:/frame/themes/dark/icons/notifications_toggle_checked.svg"
                              : "qrc:/frame/themes/dark/icons/notifications_toggle_normal.svg"
                hoverSource: home.notifyShown
                             ? "qrc:/frame/themes/dark/icons/notifications_toggle_checkedhover.svg"
                             : "qrc:/frame/themes/dark/icons/notifications_toggle_hover.svg"
                pressSource: "qrc:/frame/themes/dark/icons/notifications_toggle_checked.svg"
                onClicked: home.notifyShown = !home.notifyShown
            }
        }

        LegacyWidget {
            id: updateNotifier

            width: parent.width
            height: implicitHeight
            visible: widget && widgetVisible
            widget: home.homeWidgets.updateNotifier
        }

        Item {
            width: parent.width
            height: 1
        }

        // Either the module grid or the notifications.
        Item {
            id: plugins

            width: parent.width
            height: central.height - header.height - 1 - 10
                    - (updateNotifier.visible ? updateNotifier.height : 0)
                    - (quickControlPanel.visible ? quickControlPanel.height : 0)

            NavigationGrid {
                anchors.fill: parent
                visible: !home.notifyShown
                moduleHost: home.moduleHost
                onModuleClicked: module => home.moduleClicked(module)
            }

            LegacyWidget {
                anchors.fill: parent
                visible: home.notifyShown
                widget: home.homeWidgets.notifyWidget
            }
        }

        LegacyWidget {
            id: quickControlPanel

            width: parent.width
            // The old layout gave it stretch 1 of 100, so in practice its
            // minimum height.
            height: implicitHeight
            visible: home.notifyShown && widgetVisible
            widget: home.homeWidgets.quickControlPanel
        }
    }
}
