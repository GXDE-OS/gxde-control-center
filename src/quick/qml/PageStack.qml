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

// Page stack, like the FrameWidget stack in the old Frame. A new page comes
// in from the right and the old one goes left. Going back does the opposite.
Item {
    id: stack

    property int duration: 300
    // Fade with the slide distance.
    property bool opacityAnimation: true
    property real opacityDistance: width

    property var frames: []
    readonly property int depth: frames.length
    readonly property Item currentItem: depth > 0 ? frames[depth - 1].page : null

    // A page left the stack and its slide is done. The user decides to
    // delete or keep it.
    signal popped(Item page)

    function push(page) {
        const previous = depth > 0 ? frames[depth - 1] : null
        const frame = frameComponent.createObject(stack, { page: page })

        frames = frames.concat([frame])

        // Show the first page without a slide.
        if (!previous)
            return

        frame.slide(width, 0, false)
        previous.slide(0, -width, true)
    }

    function pop() {
        if (depth <= 1)
            return

        const frame = frames[depth - 1]
        frames = frames.slice(0, depth - 1)

        frame.destroyAfterSlide = true
        frame.slide(0, width, true)
        frames[depth - 1].slide(-width, 0, false)
    }

    clip: true

    Component {
        id: frameComponent

        // Like FrameWidget.
        Item {
            id: frame

            property Item page
            property bool hideAfterSlide: false
            property bool destroyAfterSlide: false

            function slide(from, to, hide) {
                hideAfterSlide = hide
                visible = true
                animation.stop()
                animation.from = from
                animation.to = to
                animation.start()
            }

            width: stack.width
            height: stack.height
            // A page that slides out gets no more mouse events.
            enabled: !animation.running || !hideAfterSlide
            opacity: stack.opacityAnimation && animation.running
                     ? Math.max(0, 1 - Math.abs(x) / stack.opacityDistance) : 1

            Component.onCompleted: {
                page.parent = frame
                page.x = 0
                page.y = 0
                page.width = Qt.binding(() => frame.width)
                page.height = Qt.binding(() => frame.height)
                page.visible = true
            }

            NumberAnimation {
                id: animation

                target: frame
                property: "x"
                duration: stack.duration
                easing.type: Easing.InOutCubic

                onFinished: {
                    if (!frame.hideAfterSlide)
                        return

                    frame.visible = false
                    if (!frame.destroyAfterSlide)
                        return

                    // Give the page back; the popped handler decides.
                    const page = frame.page
                    page.visible = false
                    page.parent = stack
                    frame.page = null
                    frame.destroy()
                    stack.popped(page)
                }
            }
        }
    }
}
