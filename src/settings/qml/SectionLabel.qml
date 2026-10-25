// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Heading above a card, with optional icon and trailing content.
import QtQuick
import QtQuick.Layouts

RowLayout {
    id: label

    property string text
    property string iconName
    default property alias trailing: trailingRow.data

    Layout.fillWidth: true
    Layout.topMargin: 8
    Layout.leftMargin: 4
    Layout.rightMargin: 4
    spacing: 8

    Icon {
        visible: label.iconName.length > 0
        name: label.iconName
        size: 16
        color: Theme.textSecondary
    }
    UiText {
        Layout.fillWidth: true
        text: label.text
        font.weight: Font.DemiBold
        color: Theme.textSecondary
        elide: Text.ElideRight
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }
    RowLayout {
        id: trailingRow
        spacing: 6
    }
}
