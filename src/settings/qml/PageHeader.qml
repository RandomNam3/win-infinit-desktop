// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Title area of a page: icon tile, title, subtitle and optional actions.
import QtQuick
import QtQuick.Layouts

RowLayout {
    id: header

    property string iconName
    property string title
    property string subtitle
    default property alias actions: actionRow.data

    Layout.fillWidth: true
    Layout.bottomMargin: 6
    spacing: 16

    Rectangle {
        Layout.alignment: Qt.AlignTop
        Layout.preferredWidth: 48
        Layout.preferredHeight: 48
        radius: Theme.radiusLarge
        color: Theme.accentSoft
        visible: header.iconName.length > 0

        Icon {
            anchors.centerIn: parent
            name: header.iconName
            size: 24
            color: Theme.accentText
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        spacing: 2

        UiText {
            Layout.fillWidth: true
            text: header.title
            font.pointSize: Theme.fontHeadline
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }
        UiText {
            Layout.fillWidth: true
            visible: text.length > 0
            text: header.subtitle
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }
    }

    RowLayout {
        id: actionRow
        Layout.alignment: Qt.AlignVCenter
        spacing: 8
    }
}
