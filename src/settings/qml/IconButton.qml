// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Small button with only an icon. The tooltip doubles as accessible name.
import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.AbstractButton {
    id: control

    property string iconName
    property real iconSize: 16
    property color iconColor: Theme.textSecondary
    property color hoverIconColor: Theme.text
    property string tooltip
    property bool round: true

    implicitWidth: 30
    implicitHeight: 30
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: enabled ? 1 : 0.4

    Accessible.role: Accessible.Button
    Accessible.name: tooltip
    Accessible.onPressAction: control.clicked()

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            color: control.hovered || control.visualFocus ? control.hoverIconColor : control.iconColor
        }
    }

    background: Rectangle {
        radius: control.round ? width / 2 : Theme.radiusMedium
        color: control.down ? Theme.pressed : control.hovered ? Theme.hover : Theme.alpha(Theme.hover, 0)

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }

        FocusRing {
            radius: parent.radius + offset
            visible: control.visualFocus
        }
    }

    UiToolTip {
        text: control.tooltip
        visible: control.hovered && control.tooltip.length > 0
    }
}
