// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Editor for lists of applications (window classes or desktop file names):
// one row per entry with icon, name and id, a remove button per row and a
// picker for installed applications.
import QtQuick
import QtQuick.Layouts
import InfiniteDesktop.Settings

ColumnLayout {
    id: root

    property var items: []
    property string accessibleName
    property bool showAll: false

    signal edited(var items)

    // Long lists start collapsed; showing one more row than the limit never pays off.
    readonly property int collapsedLimit: 5
    readonly property bool collapsible: items.length > collapsedLimit + 1
    readonly property var shownItems: collapsible && !showAll ? items.slice(0, collapsedLimit) : items

    function add(id) {
        const value = String(id).trim();
        if (value.length === 0 || items.indexOf(value) >= 0) {
            return;
        }
        const result = items.slice();
        result.push(value);
        edited(result);
    }

    function remove(index) {
        const result = items.slice();
        result.splice(index, 1);
        edited(result);
    }

    function openPicker() {
        picker.open();
    }

    spacing: 10
    opacity: enabled ? 1 : 0.45

    Accessible.role: Accessible.List
    Accessible.name: accessibleName

    // Empty state
    Rectangle {
        Layout.fillWidth: true
        visible: root.items.length === 0
        implicitHeight: emptyRow.implicitHeight + 28
        radius: Theme.radiusMedium
        color: Theme.surfaceSunken
        border.width: 1
        border.color: Theme.border

        RowLayout {
            id: emptyRow

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                radius: Theme.radiusMedium
                color: Theme.control

                Icon {
                    anchors.centerIn: parent
                    name: "windows"
                    size: 18
                    color: Theme.textTertiary
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                UiText {
                    Layout.fillWidth: true
                    text: i18nc("@info:status", "No applications in this list")
                    color: Theme.textSecondary
                    font.weight: Font.Medium
                    wrapMode: Text.Wrap
                }
                UiText {
                    Layout.fillWidth: true
                    text: i18n("Add an application to treat it differently.")
                    color: Theme.textTertiary
                    font.pointSize: Theme.fontSmall
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    // The entries
    Rectangle {
        Layout.fillWidth: true
        visible: root.items.length > 0
        implicitHeight: listColumn.implicitHeight
        radius: Theme.radiusMedium
        color: Theme.surfaceSunken
        border.width: 1
        border.color: Theme.border

        ColumnLayout {
            id: listColumn

            width: parent.width
            spacing: 0

            Repeater {
                model: root.shownItems

                Item {
                    id: entry

                    required property int index
                    required property string modelData

                    readonly property var info: Applications.describe(modelData)
                    readonly property bool lastShown: index === root.shownItems.length - 1

                    Layout.fillWidth: true
                    implicitHeight: 50

                    Accessible.role: Accessible.ListItem
                    Accessible.name: info.known ? info.name + ", " + modelData : modelData

                    HoverHandler {
                        id: hover
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: Theme.radiusSmall
                        color: hover.hovered ? Theme.hover : Theme.alpha(Theme.hover, 0)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        spacing: 12

                        AppIcon {
                            iconName: entry.info.iconName
                            hasIcon: entry.info.hasIcon
                            size: 28
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            UiText {
                                Layout.fillWidth: true
                                text: entry.info.name
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }
                            UiText {
                                Layout.fillWidth: true
                                text: entry.info.known ? entry.modelData : i18nc("@info an entry without a desktop file", "Window class")
                                color: Theme.textSecondary
                                font.pointSize: Theme.fontCaption
                                elide: Text.ElideMiddle
                            }
                        }
                        IconButton {
                            iconName: "trash"
                            iconSize: 15
                            tooltip: i18nc("@action:button %1 is an application", "Remove “%1”", entry.info.name)
                            onClicked: root.remove(entry.index)
                        }
                    }

                    Rectangle {
                        visible: !entry.lastShown || root.collapsible
                        anchors.bottom: parent.bottom
                        x: 52
                        width: parent.width - 60
                        height: 1
                        color: Theme.divider
                    }
                }
            }

            UiButton {
                Layout.fillWidth: true
                Layout.margins: 4
                visible: root.collapsible
                variant: "ghost"
                compact: true
                iconName: root.showAll ? "chevron-down" : "chevron-right"
                text: root.showAll ? i18nc("@action:button", "Show fewer")
                                   : i18ncp("@action:button", "Show %1 more", "Show %1 more", root.items.length - root.shownItems.length)
                onClicked: root.showAll = !root.showAll
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        UiButton {
            iconName: "plus"
            text: i18nc("@action:button", "Add Application…")
            onClicked: picker.open()
        }
        Item {
            Layout.fillWidth: true
        }
        UiText {
            visible: root.items.length > 0
            text: i18ncp("@info number of entries in a list", "%1 entry", "%1 entries", root.items.length)
            color: Theme.textTertiary
            font.pointSize: Theme.fontSmall
        }
    }

    AppPicker {
        id: picker
        existing: root.items
        onPicked: id => root.add(id)
    }
}
