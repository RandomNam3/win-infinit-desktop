// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A single keyboard key.
import QtQuick

Rectangle {
    id: cap

    property string text
    property bool small: false
    property bool highlighted: false

    implicitWidth: Math.max(implicitHeight, label.implicitWidth + (small ? 10 : 14))
    implicitHeight: small ? 20 : 24
    radius: small ? 4 : 5
    color: highlighted ? Theme.accentSoft : (Theme.dark ? "#2c3038" : "#ffffff")
    border.width: 1
    border.color: highlighted ? Theme.alpha(Theme.accentText, 0.5) : Theme.borderStrong

    // Key "depth"
    Rectangle {
        z: -1
        anchors.fill: parent
        anchors.topMargin: 1.5
        anchors.bottomMargin: -1.5
        radius: parent.radius
        color: cap.highlighted ? Theme.alpha(Theme.accentText, 0.35) : Theme.borderStrong
    }

    UiText {
        id: label
        anchors.centerIn: parent
        text: cap.text
        font.pointSize: cap.small ? Theme.fontCaption : Theme.fontSmall
        font.weight: Font.Medium
        color: cap.highlighted ? Theme.accentText : Theme.text
    }
}
