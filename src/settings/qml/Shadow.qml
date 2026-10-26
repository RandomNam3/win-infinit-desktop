// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Soft drop shadow made of a few translucent layers. Works with every scene
// graph backend (including the software renderer), no shaders involved.
import QtQuick

Item {
    id: root

    property real radius: Theme.radiusLarge
    // 1: resting card, 2: popover / dialog
    property int elevation: 1
    property color color: Theme.shadow

    readonly property int layers: elevation >= 2 ? 7 : 3
    readonly property real spreadStep: elevation >= 2 ? 2.4 : 1.2
    readonly property real offsetY: elevation >= 2 ? 6 : 3

    anchors.fill: parent
    z: -1

    Repeater {
        model: root.layers

        Rectangle {
            required property int index
            readonly property real spread: (index + 1) * root.spreadStep

            x: -spread
            y: -spread + root.offsetY
            width: root.width + 2 * spread
            height: root.height + 2 * spread
            radius: root.radius + spread
            color: Qt.rgba(root.color.r, root.color.g, root.color.b, root.color.a / root.layers)
        }
    }
}
