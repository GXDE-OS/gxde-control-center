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

// All Settings, like the old SettingsWidget: module widgets in a column.
// When scrolling stops, visible modules are made active and the nav bar
// follows the current module.
Item {
    id: page

    required property ModuleHost moduleHost
    required property ControlCenter controlCenter

    // Module checked in the nav bar. The first one by default, like before.
    property string currentModule: moduleHost.moduleNames.length > 0 ? moduleHost.moduleNames[0] : ""
    // Module and page to show once the module is loaded.
    property string pendingModule
    property string pendingPage
    property var activeModules: ({})

    signal backRequested()

    function moduleItem(name) {
        for (let i = 0; i < repeater.count; ++i) {
            const item = repeater.itemAt(i)
            if (item && item.name === name)
                return item
        }
        return null
    }

    function maxContentY() {
        return Math.max(0, flickable.contentHeight - flickable.height)
    }

    function stopScroll() {
        wheelHandler.speedTime = wheelHandler.defaultSpeedTime
        scrollAnimation.stop()
        wheelAnimation.stop()
        flickable.cancelFlick()
    }

    function scrollTo(y, animation) {
        const value = Math.min(maxContentY(), y)
        if (animation) {
            scrollAnimation.from = flickable.contentY
            scrollAnimation.to = value
            scrollAnimation.start()
        } else {
            flickable.contentY = value
        }
    }

    function ensureModuleVisible(name, animation) {
        const item = moduleItem(name)
        if (!item)
            return

        currentModule = name
        scrollTo(item.y, animation)
    }

    function showModulePage(module, modulePage, animation) {
        stopScroll()
        refreshDelay.interval = animation ? scrollAnimation.duration : 1
        refreshDelay.restart()

        if (!moduleHost.isReady(module)) {
            pendingModule = module
            pendingPage = modulePage
            return
        }

        ensureModuleVisible(module, animation)
        if (modulePage !== "")
            moduleHost.showPage(module, modulePage)
    }

    // Make visible modules active, others inactive, and update the nav bar.
    function refreshNavigation() {
        const top = flickable.contentY
        const bottom = top + flickable.height
        let firstActive = ""
        let currentActive = false

        for (let i = 0; i < repeater.count; ++i) {
            const item = repeater.itemAt(i)
            if (!item)
                continue

            const shown = item.visible && item.height > 0
            const active = shown && item.y < bottom && item.y + item.height > top
            if (!firstActive && shown && item.y >= top)
                firstActive = item.name
            if (item.name === currentModule)
                currentActive = active

            if (activeModules[item.name] !== active) {
                activeModules[item.name] = active
                moduleHost.setModuleActive(item.name, active)
            }
        }

        if (currentActive)
            return

        if (!firstActive) {
            for (const name in activeModules) {
                if (activeModules[name]) {
                    firstActive = name
                    break
                }
            }
        }

        if (firstActive)
            currentModule = firstActive
    }

    ContentHeader {
        id: header

        width: parent.width
        title: qsTranslate("SettingsWidget", "All Settings")
        backVisible: page.controlCenter.mainPageEnabled
        onBackClicked: page.backRequested()
    }

    Flickable {
        id: flickable

        y: header.height
        width: parent.width - 8
        height: parent.height - y
        contentWidth: width
        contentHeight: column.height
        clip: true
        flickableDirection: Flickable.VerticalFlick

        // Stop the smooth scroll when the user drags.
        onMovementStarted: {
            scrollAnimation.stop()
            wheelAnimation.stop()
        }
        onContentYChanged: {
            refreshTimer.restart()
            if (wheelAnimation.running)
                page.refreshNavigation()
        }

        Column {
            id: column

            width: flickable.width
            topPadding: 10
            bottomPadding: 20
            spacing: 30

            Repeater {
                id: repeater

                model: page.moduleHost

                delegate: LegacyWidget {
                    required property var model
                    readonly property string name: model.name

                    width: column.width
                    height: implicitHeight
                    visible: widgetVisible
                    widget: model.widget
                }
            }
        }

        // Wheel events the widgets do not use scroll smoothly, like before.
        WheelHandler {
            id: wheelHandler

            readonly property real defaultSpeedTime: 2
            readonly property real maxSpeedTime: 14
            property real speedTime: defaultSpeedTime

            target: null
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

            onWheel: event => {
                // Go faster while the wheel keeps turning.
                speedTime = wheelAnimation.running ? speedTime + 0.02 : defaultSpeedTime

                scrollAnimation.stop()
                wheelAnimation.stop()
                wheelAnimation.from = flickable.contentY
                wheelAnimation.to = Math.max(0, Math.min(page.maxContentY(),
                    flickable.contentY - event.angleDelta.y * Math.min(speedTime, maxSpeedTime)))
                wheelAnimation.start()
            }
        }
    }

    NumberAnimation {
        id: scrollAnimation

        target: flickable
        property: "contentY"
        duration: 1400
        easing.type: Easing.OutQuint
    }

    NumberAnimation {
        id: wheelAnimation

        target: flickable
        property: "contentY"
        duration: 1400
        easing.type: Easing.OutQuint
    }

    // Refresh 500 ms after scrolling stops, not on every step.
    Timer {
        id: refreshTimer

        interval: 500
        onTriggered: {
            if (!page.visible)
                return

            if (flickable.moving) {
                restart()
                return
            }

            page.stopScroll()
            page.refreshNavigation()
        }
    }

    Timer {
        id: refreshDelay

        onTriggered: refreshTimer.restart()
    }

    // Show the waiting module again once it is loaded.
    Timer {
        id: pendingTimer

        interval: 10
        onTriggered: page.showModulePage(page.pendingModule, page.pendingPage, false)
    }

    Connections {
        target: page.moduleHost

        function onModuleReady(name) {
            if (page.moduleHost.allReady()) {
                refreshTimer.restart()
                if (page.pendingPage === "")
                    pendingTimer.restart()
            }

            if (name === page.pendingModule)
                pendingTimer.restart()
        }
    }

    // Mouse back button goes back.
    MouseArea {
        anchors.fill: parent
        z: -1
        acceptedButtons: Qt.BackButton
        onReleased: page.backRequested()
    }
}
