// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Colored status dot with a soft halo; pulses gently while active.
import QtQuick

Item {
    id: root

    property string status

    readonly property color dotColor: StatusTexts.color(status)

    implicitWidth: 10
    implicitHeight: 10

    Rectangle {
        id: halo
        anchors.centerIn: parent
        width: 18
        height: 18
        radius: 9
        color: Theme.alpha(root.dotColor, 0.22)

        SequentialAnimation on opacity {
            running: root.status === "active" && Theme.animations
            loops: Animation.Infinite
            NumberAnimation { from: 1; to: 0.25; duration: 1400; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.25; to: 1; duration: 1400; easing.type: Easing.InOutSine }
        }
    }
    Rectangle {
        anchors.centerIn: parent
        width: 9
        height: 9
        radius: 4.5
        color: root.dotColor
    }
}
