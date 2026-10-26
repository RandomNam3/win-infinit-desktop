// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Visible keyboard focus indicator drawn around its parent.
import QtQuick

Rectangle {
    property real offset: 3

    anchors.fill: parent
    anchors.margins: -offset
    color: "transparent"
    border.width: 2
    border.color: Theme.focusRing
    visible: false
    z: 100
}
