/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
 * Copyright (C) 2026 shenmo <jifengshenmo@outlook.com>
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
import QtQuick.Templates as T
import QtQuick.Controls.Gxde
import org.gxde.controlcenter

// One module in the window, like a SettingsSurface showing one module:
// the module widget, then the pages the module pushes on top of it.
Item {
    id: page

    required property ModuleHost moduleHost
    required property string module
    property QtObject widget: moduleHost.moduleWidget(module)

    // Modules refresh their data only while active.
    readonly property bool active: widget !== null && visible
                                   && Window.visibility !== Window.Hidden
                                   && T.StackView.status === T.StackView.Active

    signal homeRequested()

    onActiveChanged: moduleHost.setModuleActive(module, active)

    BusyIndicator {
        anchors.centerIn: parent
        visible: page.widget === null
        running: visible
    }

    Flickable {
        id: flickable

        // Same margins as the ContentWidget around the old settings page.
        y: 8
        width: parent.width - 8
        height: parent.height - y
        contentWidth: width
        contentHeight: column.height
        clip: true
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar { }

        Column {
            id: column

            width: flickable.width

            LegacyWidget {
                width: column.width
                height: implicitHeight
                visible: widgetVisible
                widget: page.widget
            }
        }
    }

    // Mouse back button goes back, like on All Settings.
    MouseArea {
        anchors.fill: parent
        z: -1
        acceptedButtons: Qt.BackButton
        onReleased: page.homeRequested()
    }

    Component {
        id: contentComponent

        LegacyContentPage {
            moduleHost: page.moduleHost
            T.StackView.onRemoved: page.moduleHost.contentPopped(content)
        }
    }

    Connections {
        target: page.moduleHost

        function onModuleReady(name) {
            if (name === page.module)
                page.widget = page.moduleHost.moduleWidget(name)
        }
        function onContentPushed(content, module) {
            if (module === page.module)
                page.T.StackView.view.push(contentComponent, { content: content })
        }
        function onContentBackRequested(content) {
            // Only the top page can go back.
            const view = page.T.StackView.view
            if (view.depth > 1 && view.currentItem.content === content)
                view.pop()
        }
    }
}
