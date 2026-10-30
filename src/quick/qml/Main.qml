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

// The panel content. ControlCenter handles the window place, slides and auto
// hide. Same layout as the old Frame: nav bar on the left, page stack.
Window {
    id: root

    required property ControlCenter controlCenter
    required property ModuleHost moduleHost
    required property UserInfo userInfo
    required property HomeWidgets homeWidgets

    // Made when first needed, freed when the panel was hidden for a while.
    property AllSettingsPage allSettingsPage: null

    width: controlCenter.frameWidth
    height: Screen.height
    color: "transparent"

    function prepareAllSettingsPage() {
        if (allSettingsPage)
            return

        allSettingsPage = allSettingsComponent.createObject(pageStack, { visible: false })
        moduleHost.load()
    }

    function showAllSettings() {
        if (pageStack.depth !== 1)
            return

        prepareAllSettingsPage()
        pageStack.push(allSettingsPage)
    }

    function showSettingsPage(module, page, animation) {
        // Go back to home or All Settings first.
        while (pageStack.depth > 1 && pageStack.currentItem !== allSettingsPage)
            pageStack.pop()

        if (pageStack.depth === 1)
            prepareAllSettingsPage()

        if (page === "" && pageStack.depth === 1)
            showAllSettings()

        Qt.callLater(allSettingsPage.showModulePage, module, page, animation)
    }

    function backToHome() {
        while (pageStack.depth > 1)
            pageStack.pop()
    }

    function releaseAllSettingsPage() {
        // Keep it while a module page is open.
        if (!allSettingsPage || pageStack.depth > 1)
            return

        allSettingsPage.destroy()
        allSettingsPage = null
        moduleHost.unload()
    }

    function notifyTopPage(appeared) {
        const page = pageStack.currentItem
        if (page === homePage) {
            if (appeared)
                homeWidgets.appear()
            else
                homeWidgets.disappear()
        } else if (page && page.content !== undefined) {
            moduleHost.setContentAppeared(page.content, appeared)
        }
    }

    Component.onCompleted: GxdeTheme.isDark = true

    Rectangle {
        anchors.fill: parent
        color: root.controlCenter.backgroundColor
    }

    // Keep the content on the left. On X11 the window grows from width 0,
    // which looks like a slide from the right.
    Item {
        id: frameWrapper

        width: root.controlCenter.frameWidth
        height: parent.height

        NavigationBar {
            id: navigationBar

            width: root.controlCenter.navigationWidth
            height: parent.height
            visible: false
            moduleHost: root.moduleHost
            currentModule: root.allSettingsPage ? root.allSettingsPage.currentModule : ""
            onModuleClicked: module => root.showSettingsPage(module, "", true)
        }

        PageStack {
            id: pageStack

            x: navigationBar.visible ? navigationBar.width : 0
            width: parent.width - x
            height: parent.height
            duration: root.controlCenter.pageAnimationDuration
            opacityAnimation: root.controlCenter.pageOpacityAnimation
            opacityDistance: root.controlCenter.frameWidth

            // The nav bar shows or hides 100 ms after the page change starts.
            onDepthChanged: navigationBarTimer.restart()
            onPopped: page => {
                if (page === root.allSettingsPage)
                    return

                if (page.content !== undefined)
                    root.moduleHost.contentPopped(page.content)
                page.destroy()
            }
        }

        Timer {
            id: navigationBarTimer

            interval: 100
            onTriggered: navigationBar.visible = pageStack.depth > 1
        }
    }

    HomePage {
        id: homePage

        userInfo: root.userInfo
        homeWidgets: root.homeWidgets
        moduleHost: root.moduleHost
        onAvatarClicked: root.showSettingsPage("accounts", root.userInfo.userName, false)
        onModuleClicked: module => root.showSettingsPage(module, "", false)
        Component.onCompleted: pageStack.push(homePage)
    }

    Component {
        id: allSettingsComponent

        AllSettingsPage {
            moduleHost: root.moduleHost
            controlCenter: root.controlCenter
            onBackRequested: pageStack.pop()
        }
    }

    Component {
        id: legacyPageComponent

        LegacyContentPage {
            moduleHost: root.moduleHost
        }
    }

    Connections {
        target: root.controlCenter

        function onSettingsPageRequested(module, page, animation) {
            root.showSettingsPage(module, page, animation)
        }
        function onHomeRequested() {
            root.backToHome()
        }
        function onNotifyRequested() {
            root.backToHome()
            homePage.showNotify()
        }
        function onNavRequested() {
            root.backToHome()
            homePage.showNav()
        }
        function onAppeared() {
            root.notifyTopPage(true)
        }
        function onDisappeared() {
            root.notifyTopPage(false)
        }
        function onPrepareRequested() {
            root.prepareAllSettingsPage()
        }
        function onReleaseRequested() {
            root.releaseAllSettingsPage()
        }
    }

    Connections {
        target: root.moduleHost

        function onContentPushed(content) {
            pageStack.push(legacyPageComponent.createObject(pageStack, { content: content }))
        }
        function onContentBackRequested(content) {
            // Only the top page can go back.
            if (pageStack.depth > 1 && pageStack.currentItem.content === content)
                pageStack.pop()
        }
        function onModulePageRequested(module, page, animation) {
            if (root.allSettingsPage)
                root.allSettingsPage.showModulePage(module, page, animation)
        }
        function onAutoHideRequested(autoHide) {
            root.controlCenter.setAutoHide(autoHide)
        }
    }

    Connections {
        target: root.homeWidgets

        function onAllSettingsRequested() {
            root.showAllSettings()
        }
        function onSettingsPageRequested(module, page, animation) {
            root.showSettingsPage(module, page, animation)
        }
        function onScreenCaptureStarted() {
            root.controlCenter.startScreenCaptureGuard()
        }
    }
}
