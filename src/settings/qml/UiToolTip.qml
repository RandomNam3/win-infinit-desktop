// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.ToolTip {
    id: tip

    delay: 450
    timeout: 6000
    topPadding: 6
    bottomPadding: 6
    leftPadding: 10
    rightPadding: 10
    font.pointSize: Theme.fontSmall
    margins: 8

    contentItem: UiText {
        text: tip.text
        font: tip.font
        color: Theme.onInverse
        wrapMode: Text.Wrap
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.inverse
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationFast }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durationFast }
    }
}
