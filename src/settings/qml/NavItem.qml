// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Entry of the sidebar navigation.
import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.AbstractButton {
    id: control

    property string iconName
    property bool selected: false
    property bool compact: false
    property bool keyboardCurrent: false

    implicitHeight: 38
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    Accessible.role: Accessible.PageTab
    Accessible.name: text
    Accessible.selected: selected
    Accessible.onPressAction: control.clicked()

    background: Rectangle {
        radius: Theme.radiusMedium
        color: control.selected ? Theme.accentSoft : control.down ? Theme.pressed : control.hovered ? Theme.hover : Theme.alpha(Theme.hover, 0)

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }

        FocusRing {
            offset: 0
            radius: parent.radius
            visible: control.keyboardCurrent
        }
    }

    contentItem: Item {
        Icon {
            id: icon
            x: control.compact ? (parent.width - width) / 2 : 10
            anchors.verticalCenter: parent.verticalCenter
            name: control.iconName
            size: 18
            color: control.selected ? Theme.accentText : Theme.textSecondary
        }
        UiText {
            anchors.left: icon.right
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            visible: !control.compact
            text: control.text
            elide: Text.ElideRight
            color: control.selected ? Theme.text : Theme.text
            font.weight: control.selected ? Font.DemiBold : Font.Normal
        }
    }

    UiToolTip {
        text: control.text
        visible: control.compact && control.hovered
        x: control.width + 8
        y: (control.height - height) / 2
    }
}
