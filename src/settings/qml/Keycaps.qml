// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A key combination as a row of key caps.
import QtQuick

Row {
    id: root

    property var keys: []
    property bool small: false
    property bool highlighted: false

    spacing: 4
    Accessible.role: Accessible.StaticText
    Accessible.name: keys.join("+")

    Repeater {
        model: root.keys

        Keycap {
            required property string modelData
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            text: modelData
            small: root.small
            highlighted: root.highlighted
        }
    }
}
