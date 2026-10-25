// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Slider with a value bubble above the handle and a tick at the default value.
import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Slider {
    id: control

    property string valueText: Math.round(value).toString()
    property real defaultValue: NaN
    readonly property bool showBubble: pressed || hovered || visualFocus

    implicitWidth: 200
    implicitHeight: 32
    leftPadding: 10
    rightPadding: 10
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    snapMode: Basic.Slider.SnapAlways
    live: true
    opacity: enabled ? 1 : 0.45

    Accessible.role: Accessible.Slider

    background: Item {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: 4

        Rectangle {
            anchors.fill: parent
            radius: 2
            color: Theme.track
        }
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: Theme.accentFill
        }
        Rectangle {
            visible: !isNaN(control.defaultValue) && control.to > control.from
            readonly property real position: (control.defaultValue - control.from) / (control.to - control.from)
            x: Math.round(position * parent.width - width / 2)
            y: -3
            width: 2
            height: 10
            radius: 1
            color: position <= control.visualPosition ? Theme.alpha(Theme.onAccent, 0.75) : Theme.textTertiary
        }
    }

    handle: Rectangle {
        id: handle

        x: control.leftPadding + control.visualPosition * control.availableWidth - width / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 20
        height: 20
        radius: 10
        color: "#ffffff"
        border.width: Theme.dark ? 0 : 1
        border.color: Theme.borderStrong
        scale: control.pressed ? 1.1 : 1

        Behavior on scale {
            NumberAnimation { duration: Theme.durationFast }
        }

        Rectangle {
            z: -1
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.bottomMargin: -1.5
            radius: parent.radius
            color: Qt.rgba(0, 0, 0, Theme.dark ? 0.4 : 0.12)
        }

        FocusRing {
            radius: handle.radius + offset
            visible: control.visualFocus
        }

        Rectangle {
            id: bubble

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.top
            anchors.bottomMargin: 9
            width: bubbleText.implicitWidth + 16
            height: 24
            radius: Theme.radiusSmall
            color: Theme.inverse
            opacity: control.showBubble ? 1 : 0
            scale: control.showBubble ? 1 : 0.9
            transformOrigin: Item.Bottom
            visible: opacity > 0

            Behavior on opacity {
                NumberAnimation { duration: Theme.durationFast }
            }
            Behavior on scale {
                NumberAnimation { duration: Theme.durationFast }
            }

            Rectangle {
                z: -1
                width: 8
                height: 8
                rotation: 45
                color: bubble.color
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height - 5
            }

            UiText {
                id: bubbleText
                anchors.centerIn: parent
                text: control.valueText
                color: Theme.onInverse
                font.pointSize: Theme.fontSmall
                font.weight: Font.DemiBold
            }
        }
    }
}
