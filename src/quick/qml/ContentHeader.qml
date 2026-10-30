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

// Page header, like the top of the old ContentWidget: back button, centered
// title and a line. 8 px on the right, like the ContentWidget margin.
Item {
    id: header

    property alias title: titleLabel.text
    property bool backVisible: true

    signal backClicked()

    height: 48

    ImageButton {
        id: backButton

        y: 8
        width: 24
        height: 24
        visible: header.backVisible
        normalSource: "qrc:/widgets/themes/dark/icons/back_normal.png"
        hoverSource: "qrc:/widgets/themes/dark/icons/back_hover.png"
        pressSource: "qrc:/widgets/themes/dark/icons/back_press.png"
        onClicked: header.backClicked()
    }

    // Space as wide as the back button, so the title is centered.
    Text {
        id: titleLabel

        x: header.backVisible ? backButton.width : 0
        y: 8
        width: header.width - 8 - backButton.width - x
        height: 24
        color: "white"
        font.pixelSize: 14
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Rectangle {
        y: 47
        width: header.width - 8
        height: 1
        color: Qt.rgba(1, 1, 1, 0.15)
    }
}
