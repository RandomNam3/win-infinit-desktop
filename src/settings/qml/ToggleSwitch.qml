// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// On/off switch.
import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Switch {
    id: control

    implicitWidth: 42
    implicitHeight: 24
    padding: 0
    spacing: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: enabled ? 1 : 0.45

    Accessible.role: Accessible.CheckBox
    Accessible.checkable: true
    Accessible.checked: checked

    indicator: Rectangle {
        id: track

        implicitWidth: 42
        implicitHeight: 24
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: height / 2
        color: control.checked ? (control.hovered ? Theme.accentFillHover : Theme.accentFill)
                               : (control.hovered ? Qt.darker(Theme.track, Theme.dark ? 0.9 : 1.04) : Theme.track)

        Behavior on color {
            ColorAnimation { duration: Theme.durationNormal }
        }

        Rectangle {
            id: knob

            readonly property real grow: control.pressed ? 4 : 0

            width: 18 + grow
            height: 18
            radius: 9
            y: 3
            x: control.checked ? track.width - width - 3 : 3
            color: "#ffffff"

            Behavior on x {
                NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic }
            }
            Behavior on width {
                NumberAnimation { duration: Theme.durationFast }
            }

            Rectangle {
                z: -1
                anchors.fill: parent
                anchors.topMargin: 1
                anchors.bottomMargin: -1
                radius: parent.radius
                color: Qt.rgba(0, 0, 0, Theme.dark ? 0.35 : 0.16)
            }
        }

        FocusRing {
            radius: track.radius + offset
            visible: control.visualFocus
        }
    }

    contentItem: Item {}
}
