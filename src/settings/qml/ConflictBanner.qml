// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Warning shown before a key combination is taken away from another action.
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: banner

    // { keys: [...], conflicts: [{ action, component }, ...] } or null
    property var pending: null

    signal reassign()
    signal cancel()

    readonly property string message: {
        if (!pending || !pending.conflicts || pending.conflicts.length === 0) {
            return "";
        }
        const first = pending.conflicts[0];
        if (pending.conflicts.length === 1) {
            return i18nc("@info %1 is an action, %2 the application or component", "Already used by “%1” (%2). Reassign it?", first.action, first.component);
        }
        return i18ncp("@info %2 is an action, %3 the application or component", "Already used by “%2” (%3) and one more action. Reassign it?",
                      "Already used by “%2” (%3) and %1 more actions. Reassign it?", pending.conflicts.length - 1, first.action, first.component);
    }

    visible: pending !== null
    implicitHeight: layout.implicitHeight + 20
    radius: Theme.radiusMedium
    color: Theme.alpha(Theme.warning, Theme.dark ? 0.14 : 0.1)
    border.width: 1
    border.color: Theme.alpha(Theme.warning, 0.35)

    Accessible.role: Accessible.AlertMessage
    Accessible.name: message

    GridLayout {
        id: layout

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 12
        anchors.rightMargin: 10
        columns: banner.width > 520 ? 4 : 2
        columnSpacing: 10
        rowSpacing: 8

        Icon {
            name: "warning"
            size: 18
            color: Theme.warning
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Keycaps {
                keys: banner.pending ? banner.pending.keys : []
                small: true
            }
            UiText {
                Layout.fillWidth: true
                text: banner.message
                wrapMode: Text.Wrap
                font.pointSize: Theme.fontSmall
            }
        }
        Item {
            visible: banner.width <= 520
            implicitWidth: 1
        }
        RowLayout {
            spacing: 6

            UiButton {
                compact: true
                variant: "secondary"
                text: i18nc("@action:button keep the old shortcut", "Cancel")
                onClicked: banner.cancel()
            }
            UiButton {
                compact: true
                variant: "primary"
                text: i18nc("@action:button take the shortcut away from the other action", "Reassign")
                onClicked: banner.reassign()
            }
        }
    }
}
