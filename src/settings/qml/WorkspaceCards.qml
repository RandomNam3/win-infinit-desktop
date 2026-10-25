// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// "Notes & Groups" page: quick actions, the groups with their applications and the areas of
// the canvas. Everything is stored by the effect; edits are sent to it right away.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import InfinitDesktop.Settings

ColumnLayout {
    id: root

    Layout.fillWidth: true
    spacing: 8

    Component.onCompleted: Workspace.refresh()
    property bool effectLoaded: Effect.loaded
    onEffectLoadedChanged: Workspace.refresh()

    // ---- Quick actions ---------------------------------------------------------
    SectionLabel {
        text: i18nc("@title", "On the canvas")
    }

    Card {
        Layout.fillWidth: true

        GridLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            columns: width > 560 ? 4 : 2
            columnSpacing: 8
            rowSpacing: 8

            UiButton {
                Layout.fillWidth: true
                iconName: "note"
                text: i18nc("@action:button", "New Note")
                enabled: Workspace.available
                onClicked: Workspace.newNote()
            }
            UiButton {
                Layout.fillWidth: true
                iconName: "map"
                text: i18nc("@action:button", "Show Map")
                enabled: Workspace.available
                onClicked: Workspace.showMap()
            }
            UiButton {
                Layout.fillWidth: true
                iconName: "windows"
                text: i18nc("@action:button", "Group from Selection")
                enabled: Workspace.available
                onClicked: Workspace.newGroupFromSelection()
            }
            UiButton {
                Layout.fillWidth: true
                iconName: "layers"
                text: i18nc("@action:button", "Draw an Area")
                enabled: Workspace.available
                onClicked: Workspace.newAreaFromSelection()
            }
        }
        UiText {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.bottomMargin: 14
            text: !Workspace.available ? i18n("Available while the canvas service is running.")
                  : i18np("%1 note. Right click on the empty canvas in the overview (Win + right click on the desktop at 100 %) for the canvas menu.",
                          "%1 notes. Right click on the empty canvas in the overview (Win + right click on the desktop at 100 %) for the canvas menu.", Workspace.noteCount)
            color: Theme.textSecondary
            font.pointSize: Theme.fontSmall
            wrapMode: Text.Wrap
        }
    }

    // ---- Groups ----------------------------------------------------------------
    SectionLabel {
        Layout.topMargin: 8
        text: i18nc("@title", "Groups")

        UiButton {
            variant: "ghost"
            compact: true
            iconName: "plus"
            text: i18nc("@action:button", "New Group")
            enabled: Workspace.available
            onClicked: Workspace.addGroup()
        }
    }

    Card {
        Layout.fillWidth: true
        visible: Workspace.groups.length === 0

        UiText {
            Layout.fillWidth: true
            Layout.margins: 16
            text: i18n("A group opens its applications together, always arranged the same way, at the place where it was last – and the view jumps there. Draw a rectangle around open windows with Win+Alt+G, or create a group here and add applications.")
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }
    }

    Repeater {
        model: Workspace.groups

        Card {
            id: groupCard

            required property var modelData
            required property int index

            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: 16
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    UiTextField {
                        Layout.fillWidth: true
                        text: groupCard.modelData.name
                        Accessible.name: i18nc("@label", "Group name")
                        onEditingFinished: Workspace.renameGroup(groupCard.modelData.id, text)
                    }
                    Keycaps {
                        visible: groupCard.modelData.slot > 0 && keys.length > 0
                        small: true
                        keys: Shortcuts.currentKeys("InfinitDesktop OpenGroup" + groupCard.modelData.slot)
                    }
                    IconButton {
                        iconName: "chevron-down"
                        rotation: 180
                        enabled: groupCard.index > 0
                        tooltip: i18nc("@action:button", "Move up (changes the shortcut)")
                        onClicked: Workspace.moveGroup(groupCard.modelData.id, -1)
                    }
                    IconButton {
                        iconName: "chevron-down"
                        enabled: groupCard.index < Workspace.groups.length - 1
                        tooltip: i18nc("@action:button", "Move down (changes the shortcut)")
                        onClicked: Workspace.moveGroup(groupCard.modelData.id, 1)
                    }
                    IconButton {
                        iconName: "trash"
                        tooltip: i18nc("@action:button", "Delete group")
                        onClicked: deleteGroup.open()
                    }
                }

                // The applications
                Flow {
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: groupCard.modelData.members

                        Rectangle {
                            id: chip

                            required property var modelData
                            required property int index

                            implicitWidth: chipRow.implicitWidth + 16
                            implicitHeight: 34
                            radius: Theme.radiusMedium
                            color: Theme.surfaceSunken
                            border.width: 1
                            border.color: Theme.border

                            Row {
                                id: chipRow

                                anchors.verticalCenter: parent.verticalCenter
                                x: 8
                                spacing: 6

                                AppIcon {
                                    anchors.verticalCenter: parent.verticalCenter
                                    iconName: chip.modelData.iconName
                                    hasIcon: chip.modelData.hasIcon
                                    size: 20
                                }
                                UiText {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: chip.modelData.name
                                    font.pointSize: Theme.fontSmall
                                }
                                IconButton {
                                    anchors.verticalCenter: parent.verticalCenter
                                    iconName: "close"
                                    iconSize: 12
                                    width: 20
                                    height: 20
                                    tooltip: i18nc("@action:button %1 is an application", "Remove “%1” from the group", chip.modelData.name)
                                    onClicked: Workspace.removeGroupApplication(groupCard.modelData.id, chip.index)
                                }
                            }
                        }
                    }

                    UiButton {
                        variant: "ghost"
                        compact: true
                        iconName: "plus"
                        text: i18nc("@action:button", "Application…")
                        onClicked: picker.open()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    UiText {
                        text: i18nc("@label", "Arrangement is saved")
                        color: Theme.textSecondary
                        font.pointSize: Theme.fontSmall
                    }
                    SegmentedControl {
                        model: [i18nc("@option group arrangement", "When saved"), i18nc("@option group arrangement", "Automatically")]
                        currentIndex: groupCard.modelData.automatic ? 1 : 0
                        accessibleName: i18nc("@label", "Arrangement is saved")
                        onActivated: index => Workspace.setGroupAutomatic(groupCard.modelData.id, index === 1)
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    UiText {
                        visible: groupCard.modelData.openWindows > 0
                        text: i18ncp("@info:status", "%1 window open", "%1 windows open", groupCard.modelData.openWindows)
                        color: Theme.textTertiary
                        font.pointSize: Theme.fontSmall
                    }
                    UiButton {
                        compact: true
                        variant: "ghost"
                        iconName: "check"
                        visible: !groupCard.modelData.automatic
                        enabled: groupCard.modelData.openWindows > 0
                        text: i18nc("@action:button", "Save Arrangement")
                        onClicked: Workspace.saveGroupLayout(groupCard.modelData.id)
                    }
                    UiButton {
                        compact: true
                        variant: "primary"
                        iconName: "arrow-right"
                        enabled: groupCard.modelData.members.length > 0
                        text: i18nc("@action:button", "Open")
                        onClicked: Workspace.openGroup(groupCard.modelData.id)
                    }
                }
            }

            AppPicker {
                id: picker

                title: i18nc("@title:window", "Add Application to the Group")
                onPicked: id => Workspace.addGroupApplication(groupCard.modelData.id, id)
            }

            ConfirmDialog {
                id: deleteGroup

                title: i18nc("@title:window", "Delete group?")
                message: i18n("The group “%1” is deleted. Its windows stay open where they are.", groupCard.modelData.name)
                confirmText: i18nc("@action:button", "Delete")
                confirmCallback: function() {
                    Workspace.removeGroup(groupCard.modelData.id);
                }
            }
        }
    }

    // ---- Areas -----------------------------------------------------------------
    SectionLabel {
        Layout.topMargin: 8
        text: i18nc("@title", "Areas")

        UiButton {
            variant: "ghost"
            compact: true
            iconName: "plus"
            text: i18nc("@action:button", "Draw an Area")
            enabled: Workspace.available
            onClicked: Workspace.newAreaFromSelection()
        }
    }

    Card {
        Layout.fillWidth: true

        UiText {
            Layout.fillWidth: true
            Layout.margins: 16
            visible: Workspace.areas.length === 0
            text: i18n("Areas divide the canvas into named parts, for example “Project” or “Reading”. Draw one with Win+Alt+B; it grows and shrinks with the windows you drag into it.")
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }

        Repeater {
            model: Workspace.areas

            Item {
                id: areaRow

                required property var modelData
                required property int index

                Layout.fillWidth: true
                implicitHeight: 56

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 12
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                        radius: 4
                        color: ["#3daee9", "#27ae60", "#f67400", "#9b59b6", "#da4453", "#16a085", "#c9ce3b", "#e93d8f"][Math.max(0, Math.min(7, areaRow.modelData.color))]
                    }
                    UiTextField {
                        Layout.fillWidth: true
                        text: areaRow.modelData.name
                        Accessible.name: i18nc("@label", "Area name")
                        onEditingFinished: Workspace.renameArea(areaRow.modelData.id, text)
                    }
                    UiText {
                        text: i18nc("@label", "Grows with its windows")
                        color: Theme.textSecondary
                        font.pointSize: Theme.fontSmall
                    }
                    ToggleSwitch {
                        checked: areaRow.modelData.dynamic
                        Accessible.name: i18nc("@label", "Grows with its windows")
                        onToggled: Workspace.setAreaDynamic(areaRow.modelData.id, checked)
                    }
                    IconButton {
                        iconName: "trash"
                        tooltip: i18nc("@action:button", "Remove area")
                        onClicked: Workspace.removeArea(areaRow.modelData.id)
                    }
                }

                Rectangle {
                    visible: areaRow.index < Workspace.areas.length - 1
                    anchors.bottom: parent.bottom
                    x: 16
                    width: parent.width - 32
                    height: 1
                    color: Theme.divider
                }
            }
        }
    }
}
