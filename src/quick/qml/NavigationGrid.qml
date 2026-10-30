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
import org.gxde.controlcenter

// Module grid on the home page, like the old NavWidget: two columns, rows
// share the height.
Item {
    id: grid

    required property ModuleHost moduleHost

    readonly property var modules: moduleHost.moduleNames.filter(
        name => !moduleHost.hiddenModules.includes(name))
    readonly property int rows: Math.ceil(modules.length / 2)
    // On screens up to 1080 px high, scale the icon and text gaps down.
    readonly property real keepRatio: Math.min(1, Screen.height / 1080)

    signal moduleClicked(string module)

    // Like QHeaderView::Stretch: extra pixels go to the first cells.
    function sectionStart(index, total, count) {
        const base = Math.floor(total / count)
        return index * base + Math.min(index, total - base * count)
    }

    Repeater {
        model: grid.modules

        delegate: MouseArea {
            id: cell

            required property string modelData
            required property int index

            readonly property int column: index % 2
            readonly property int row: Math.floor(index / 2)

            x: grid.sectionStart(column, grid.width, 2)
            y: grid.sectionStart(row, grid.height, grid.rows)
            width: grid.sectionStart(column + 1, grid.width, 2) - x
            height: grid.sectionStart(row + 1, grid.height, grid.rows) - y
            hoverEnabled: true
            onClicked: grid.moduleClicked(modelData)

            Rectangle {
                id: background

                x: 5
                y: 5
                width: parent.width - 10
                height: parent.height - 10
                radius: 5
                color: Qt.rgba(1, 1, 1, cell.containsMouse ? 25 / 255 : 7 / 255)
            }

            Image {
                id: icon

                x: background.x + Math.floor(20 * grid.keepRatio)
                y: background.y + Math.floor(26 * grid.keepRatio)
                width: 24
                height: 24
                sourceSize: Qt.size(24, 24)
                source: "qrc:/%1/themes/dark/icons/nav_%1.svg".arg(cell.modelData)
            }

            Text {
                // Put the text right of the icon when the cell is too short.
                readonly property bool beside: background.height
                    < 24 + fontMetrics.height + Math.floor(40 * grid.keepRatio)

                x: beside ? icon.x + 24 + Math.floor(20 * grid.keepRatio) : icon.x
                y: beside ? icon.y : icon.y + 24 + Math.floor(14 * grid.keepRatio)
                width: background.width - Math.floor(20 * grid.keepRatio)
                color: "white"
                wrapMode: Text.WordWrap
                text: grid.moduleHost.displayName(cell.modelData)

                FontMetrics {
                    id: fontMetrics
                }
            }
        }
    }
}
