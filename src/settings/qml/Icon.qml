// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A line icon from the bundled icon set, rendered in any color at device resolution.
import QtQuick

Item {
    id: root

    property string name
    property color color: Theme.text
    property real size: 20

    implicitWidth: size
    implicitHeight: size
    Accessible.ignored: true

    Image {
        anchors.fill: parent
        source: root.name.length > 0 ? "image://idicon/" + root.name + "/" + root.color.toString().substring(1) : ""
        sourceSize: Qt.size(Math.ceil(root.size), Math.ceil(root.size))
        fillMode: Image.PreserveAspectFit
        smooth: true
        cache: true
    }
}
