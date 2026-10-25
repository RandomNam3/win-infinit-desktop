// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Rounded surface that groups rows.
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: card

    default property alias content: column.data
    property int padding: 0
    property alias spacing: column.spacing

    implicitHeight: column.implicitHeight + 2 * padding
    radius: Theme.radiusLarge
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    Shadow {
        radius: card.radius
        visible: !Theme.dark
    }

    ColumnLayout {
        id: column
        x: card.padding
        y: card.padding
        width: card.width - 2 * card.padding
        spacing: 0
    }
}
