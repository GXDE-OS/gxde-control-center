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

// Module bar on the left of All Settings, like the old NavigationBar.
Item {
    id: bar

    required property ModuleHost moduleHost
    property string currentModule

    signal moduleClicked(string module)

    Column {
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        spacing: 20

        Repeater {
            model: bar.moduleHost.moduleNames

            delegate: ImageButton {
                id: button

                required property string modelData

                width: bar.width
                height: 34
                visible: !bar.moduleHost.hiddenModules.includes(modelData)
                checked: bar.currentModule === modelData
                normalSource: "qrc:/%1/themes/dark/icons/nav_%1_normal.svg".arg(modelData)
                checkedSource: "qrc:/%1/themes/dark/icons/nav_%1.svg".arg(modelData)

                // 10 px on each side, like the old qss.
                background: Rectangle {
                    x: 10
                    width: button.width - 20
                    height: button.height
                    radius: 4
                    color: button.checked ? Qt.rgba(1, 1, 1, 0.3)
                         : button.hovered ? Qt.rgba(1, 1, 1, 0.2) : "transparent"
                }

                onClicked: bar.moduleClicked(modelData)
                onHoveredChanged: {
                    if (hovered)
                        tip.showFor(button)
                    else if (tip.target === button)
                        tip.close()
                }
            }
        }
    }

    // On hover, show the module name left of the panel.
    Popup {
        id: tip

        property Item target

        function showFor(button) {
            target = button
            label.text = bar.moduleHost.displayName(button.modelData)
            open()
        }

        popupType: Popup.Window
        x: -width
        y: target ? target.mapToItem(bar, 0, 0).y + (target.height - height) / 2 : 0
        padding: 0
        focus: false
        closePolicy: Popup.NoAutoClose
        background: null
        onClosed: target = null

        contentItem: ArrowRectangle {
            arrowDirection: ArrowRectangle.ArrowRight
            backgroundColor: Qt.rgba(0, 0, 0, 0.8)
            borderColor: Qt.rgba(1, 1, 1, 0.1)

            contentItem: Text {
                id: label

                color: "white"
            }
        }
    }
}
