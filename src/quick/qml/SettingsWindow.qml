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
import QtQuick.Controls.Gxde
import org.gxde.controlcenter

// The standalone window from shenmo's GXDE CC Ultimate (FrameWindow): module
// cards on the home page, then the module list and one module. It has its
// own ModuleHost, so the panel and the window can be open at the same time.
ApplicationWindow {
    id: root

    required property ControlCenter controlCenter
    required property ModuleHost moduleHost

    // Page to show once its module is loaded.
    property string pendingModule
    property string pendingPage

    width: 960
    height: 640
    minimumWidth: 720
    minimumHeight: 480
    title: qsTr("Control Center")
    // Same as the WindowPages background in the fork.
    color: "#1d1d1d"

    function showHome() {
        settingsView.showHome()
    }

    function showModule(module, page) {
        moduleHost.load()
        settingsView.showModule(module, page)
    }

    // A module asked for one of its pages.
    function showPage(module, page) {
        if (moduleHost.isReady(module)) {
            Qt.callLater(moduleHost.showPage, module, page)
            return
        }

        pendingModule = module
        pendingPage = page
    }

    onVisibleChanged: {
        if (visible)
            moduleHost.load()
    }

    Component.onCompleted: {
        GxdeTheme.isDark = true
        settingsView.modules = moduleHost.moduleNames.map(
            name => moduleComponent.createObject(root, { name: name }))
    }

    // DMainWindow's title bar in the fork. On X11 the window manager frame
    // stays, so only the menu button is shown, like DTK without dxcb.
    header: GXTitleBar {
        iconName: "preferences-system"
        embedMode: Qt.platform.pluginName === "xcb"
    }

    SettingsView {
        id: settingsView

        anchors.fill: parent
        homeTitle: qsTranslate("SettingsWidget", "All Settings")
    }

    Component {
        id: moduleComponent

        SettingsModule {
            id: settingsModule

            title: root.moduleHost.displayName(name)
            iconSource: "qrc:/%1/themes/dark/icons/nav_%1.svg".arg(name)
            available: !root.moduleHost.hiddenModules.includes(name)
            page: Component {
                WindowModulePage {
                    moduleHost: root.moduleHost
                    module: settingsModule.name
                    onHomeRequested: root.showHome()
                }
            }

            onPageRequested: page => root.showPage(name, page)
        }
    }

    Connections {
        target: root.controlCenter

        function onWindowPageRequested(module, page) {
            if (module === "")
                root.showHome()
            else
                root.showModule(module, page)
        }
    }

    Connections {
        target: root.moduleHost

        function onModuleReady(name) {
            if (name !== root.pendingModule)
                return

            root.pendingModule = ""
            Qt.callLater(root.moduleHost.showPage, name, root.pendingPage)
        }
        // A module opens another module, like the old showModulePage.
        function onModulePageRequested(module, page, animation) {
            root.showModule(module, page)
        }
    }
}
