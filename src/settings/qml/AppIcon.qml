// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Icon of an application from the system icon theme, with a neutral fallback.
import QtQuick

Item {
    id: root

    property string iconName
    property bool hasIcon: false
    property real size: 28

    implicitWidth: size
    implicitHeight: size
    Accessible.ignored: true

    Image {
        anchors.fill: parent
        visible: root.hasIcon
        source: root.hasIcon && root.iconName.length > 0 ? "image://themeicon/" + root.iconName : ""
        sourceSize: Qt.size(Math.ceil(root.size), Math.ceil(root.size))
        smooth: true
        asynchronous: true
    }

    Rectangle {
        anchors.fill: parent
        visible: !root.hasIcon
        radius: Math.round(root.size * 0.25)
        color: Theme.control

        Icon {
            anchors.centerIn: parent
            name: "windows"
            size: Math.round(root.size * 0.58)
            color: Theme.textSecondary
        }
    }
}
