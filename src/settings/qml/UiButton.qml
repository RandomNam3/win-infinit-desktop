// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Push button. variant: "primary", "secondary", "ghost", "link", "danger", "dangerSoft".
import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Button {
    id: control

    property string variant: "secondary"
    property string iconName: ""
    property bool compact: false

    readonly property color foreground: {
        switch (variant) {
        case "primary":
            return Theme.onAccent;
        case "danger":
            return "#ffffff";
        case "dangerSoft":
            return Theme.danger;
        case "link":
            return Theme.accentText;
        default:
            return Theme.text;
        }
    }
    readonly property color fill: {
        const isDown = control.down;
        const isHovered = control.hovered;
        switch (variant) {
        case "primary":
            return isDown ? Theme.accentFillPressed : isHovered ? Theme.accentFillHover : Theme.accentFill;
        case "danger":
            return isDown ? Qt.darker(Theme.dangerFill, 1.15) : isHovered ? Qt.darker(Theme.dangerFill, 1.07) : Theme.dangerFill;
        case "dangerSoft":
            return Theme.alpha(Theme.danger, isDown ? 0.22 : isHovered ? 0.16 : 0.1);
        case "ghost":
        case "link":
            return isDown ? Theme.pressed : isHovered ? Theme.hover : Theme.alpha(Theme.hover, 0);
        default:
            return isDown ? Theme.controlPressed : isHovered ? Theme.controlHover : Theme.control;
        }
    }

    implicitHeight: compact ? 28 : 34
    leftPadding: iconName.length > 0 && text.length > 0 ? (compact ? 10 : 12) : (compact ? 12 : 16)
    rightPadding: iconName.length > 0 && text.length === 0 ? leftPadding : (compact ? 12 : 16)
    spacing: 7
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font.pointSize: compact ? Theme.fontSmall : Theme.fontBody
    font.weight: Font.Medium
    opacity: enabled ? 1 : 0.45

    Accessible.role: Accessible.Button
    Accessible.name: text

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Row {
            id: row
            anchors.centerIn: parent
            spacing: control.spacing

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.iconName.length > 0
                name: control.iconName
                size: control.compact ? 14 : 16
                color: control.foreground
            }
            UiText {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.text.length > 0
                text: control.text
                font: control.font
                color: control.foreground
            }
        }
    }

    background: Rectangle {
        implicitWidth: control.compact ? 28 : 34
        radius: Theme.radiusMedium
        color: control.fill

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }

        FocusRing {
            radius: parent.radius + offset
            visible: control.visualFocus
        }
    }
}
