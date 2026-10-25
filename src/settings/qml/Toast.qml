// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Short confirmation at the bottom of the window.
import QtQuick

Rectangle {
    id: toast

    property string text
    property string iconName: "check"
    property color iconColor: Theme.dark ? "#178a56" : "#5fe0a0"
    property bool shown: false

    function show(message, icon, error) {
        text = message;
        iconName = icon || "check";
        iconColor = error ? (Theme.dark ? "#cc3a31" : "#ff8a80") : (Theme.dark ? "#178a56" : "#5fe0a0");
        shown = true;
        hideTimer.restart();
    }

    anchors.horizontalCenter: parent.horizontalCenter
    y: parent.height - height - (shown ? 24 : 8)
    z: 1000
    width: row.implicitWidth + 32
    height: 38
    radius: height / 2
    color: Theme.inverse
    opacity: shown ? 1 : 0
    visible: opacity > 0

    Accessible.role: Accessible.AlertMessage
    Accessible.name: text

    Behavior on opacity {
        NumberAnimation { duration: Theme.durationNormal }
    }
    Behavior on y {
        NumberAnimation { duration: Theme.durationSlow; easing.type: Easing.OutCubic }
    }

    Timer {
        id: hideTimer
        interval: 1800
        onTriggered: toast.shown = false
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 8

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: toast.iconName
            size: 16
            color: toast.iconColor
        }
        UiText {
            anchors.verticalCenter: parent.verticalCenter
            text: toast.text
            color: Theme.onInverse
            font.weight: Font.Medium
        }
    }
}
