// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.ScrollBar {
    id: bar

    padding: 3
    minimumSize: 0.08
    hoverEnabled: true

    contentItem: Rectangle {
        implicitWidth: bar.hovered || bar.pressed ? 8 : 6
        implicitHeight: 40
        radius: width / 2
        color: bar.pressed ? Theme.textSecondary : Theme.textTertiary
        opacity: bar.policy === Basic.ScrollBar.AlwaysOn || (bar.active && bar.size < 1.0) ? (bar.hovered ? 0.8 : 0.5) : 0

        Behavior on opacity {
            NumberAnimation { duration: Theme.durationNormal }
        }
        Behavior on implicitWidth {
            NumberAnimation { duration: Theme.durationFast }
        }
    }

    background: Item {}
}
