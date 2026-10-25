// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.TextField {
    id: control

    implicitHeight: Theme.controlHeight
    leftPadding: 12
    rightPadding: 12
    color: Theme.text
    placeholderTextColor: Theme.textTertiary
    selectionColor: Theme.accentSoft
    selectedTextColor: Theme.text
    font.pointSize: Theme.fontBody
    selectByMouse: true
    hoverEnabled: true
    renderType: Text.NativeRendering
    opacity: enabled ? 1 : 0.45

    Accessible.role: Accessible.EditableText

    background: Rectangle {
        radius: Theme.radiusMedium
        color: control.activeFocus ? Theme.input : control.hovered ? Theme.controlHover : Theme.control
        border.width: control.activeFocus ? 1.5 : 0
        border.color: Theme.accentText

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }
    }
}
