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

// A ContentWidget page a module pushed with FrameProxyInterface::pushWidget().
Item {
    id: page

    required property ModuleHost moduleHost
    required property QtObject content

    // Mouse back button goes back, like ContentWidget::mouseReleaseEvent.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.BackButton
        onReleased: page.moduleHost.requestBack(page.content)
    }

    LegacyWidget {
        anchors.fill: parent
        widget: page.content
    }
}
