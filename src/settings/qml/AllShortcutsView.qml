// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Every global shortcut of the session, grouped by application, with search,
// a conflict filter and editing.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import InfinitDesktop.Settings

Item {
    id: view

    property real maxContentWidth: Theme.contentMaxWidth
    readonly property real sideMargin: width < 600 ? 16 : 32
    readonly property real contentWidth: Math.min(width - 2 * sideMargin, maxContentWidth)
    readonly property real contentX: Math.round((width - contentWidth) / 2)
    property string filterText
    property bool onlyConflicts: false

    Component.onCompleted: {
        if (GlobalShortcuts.count === 0 && !GlobalShortcuts.loading) {
            GlobalShortcuts.reload();
        }
    }

    GlobalShortcutsFilter {
        id: filter
        query: view.filterText
        onlyConflicts: view.onlyConflicts
        highlightColor: Theme.accentText
    }

    ListView {
        id: list

        anchors.fill: parent
        model: filter
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: false
        cacheBuffer: 600

        header: Item {
            width: list.width
            height: toolbar.implicitHeight + 12

            ColumnLayout {
                id: toolbar

                x: view.contentX
                width: view.contentWidth
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    UiTextField {
                        id: filterField

                        Layout.fillWidth: true
                        leftPadding: 34
                        placeholderText: i18nc("@info:placeholder", "Filter by action, application or key…")
                        Accessible.name: i18nc("@label", "Filter shortcuts")
                        text: view.filterText
                        onTextChanged: view.filterText = text
                        Keys.onEscapePressed: text = ""

                        Icon {
                            x: 11
                            anchors.verticalCenter: parent.verticalCenter
                            name: "search"
                            size: 16
                            color: Theme.textTertiary
                        }
                    }

                    Basic.AbstractButton {
                        id: conflictToggle

                        checkable: true
                        checked: view.onlyConflicts
                        onToggled: view.onlyConflicts = checked
                        implicitHeight: Theme.controlHeight
                        implicitWidth: conflictRow.implicitWidth + 24
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        enabled: GlobalShortcuts.conflictCount > 0 || checked

                        Accessible.role: Accessible.CheckBox
                        Accessible.checkable: true
                        Accessible.checked: checked
                        Accessible.name: i18nc("@action:button filter", "Only conflicts")

                        background: Rectangle {
                            radius: Theme.radiusMedium
                            color: conflictToggle.checked ? Theme.alpha(Theme.warning, 0.18)
                                                          : conflictToggle.hovered ? Theme.controlHover : Theme.control
                            border.width: conflictToggle.checked ? 1 : 0
                            border.color: Theme.alpha(Theme.warning, 0.5)

                            FocusRing {
                                radius: parent.radius + offset
                                visible: conflictToggle.visualFocus
                            }
                        }
                        contentItem: Item {
                            RowLayout {
                                id: conflictRow
                                anchors.centerIn: parent
                                spacing: 8

                                Icon {
                                    name: "warning"
                                    size: 15
                                    color: GlobalShortcuts.conflictCount > 0 ? Theme.warning : Theme.textTertiary
                                }
                                UiText {
                                    text: i18nc("@action:button filter", "Only conflicts")
                                    font.weight: Font.Medium
                                    color: conflictToggle.enabled ? Theme.text : Theme.textTertiary
                                }
                                Rectangle {
                                    visible: GlobalShortcuts.conflictCount > 0
                                    Layout.preferredWidth: Math.max(20, countText.implicitWidth + 10)
                                    Layout.preferredHeight: 20
                                    radius: 10
                                    color: Theme.warning

                                    UiText {
                                        id: countText
                                        anchors.centerIn: parent
                                        text: GlobalShortcuts.conflictCount
                                        color: Theme.dark ? "#1b1300" : "#ffffff"
                                        font.pointSize: Theme.fontCaption
                                        font.weight: Font.Bold
                                    }
                                }
                            }
                        }
                    }

                    IconButton {
                        iconName: "reset"
                        tooltip: i18nc("@action:button", "Reload")
                        round: false
                        implicitWidth: Theme.controlHeight
                        implicitHeight: Theme.controlHeight
                        enabled: !GlobalShortcuts.loading
                        onClicked: GlobalShortcuts.reload()
                    }
                }

                UiText {
                    Layout.fillWidth: true
                    Layout.leftMargin: 4
                    text: GlobalShortcuts.loading ? i18nc("@info:status", "Loading shortcuts…")
                          : filter.count === GlobalShortcuts.count
                            ? i18ncp("@info:status %1 shortcuts of Infinit Desktop and Windows", "%1 shortcut of Infinit Desktop and Windows. Windows' own shortcuts cannot be changed; while the canvas service runs, a shortcut of Infinit Desktop wins.", "%1 shortcuts of Infinit Desktop and Windows. Windows' own shortcuts cannot be changed; while the canvas service runs, a shortcut of Infinit Desktop wins.", GlobalShortcuts.count)
                            : i18ncp("@info:status", "Showing %1 of %2 shortcuts", "Showing %1 of %2 shortcuts", filter.count, GlobalShortcuts.count)
                    color: Theme.textSecondary
                    font.pointSize: Theme.fontSmall
                    wrapMode: Text.Wrap
                }
            }
        }

        section.property: "componentName"
        section.criteria: ViewSection.FullString
        section.delegate: Item {
            required property string section

            width: list.width
            height: 44

            UiText {
                x: view.contentX + 4
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                width: view.contentWidth - 8
                text: parent.section
                font.weight: Font.DemiBold
                color: Theme.textSecondary
                elide: Text.ElideRight
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }
        }

        delegate: AllShortcutRow {
            width: list.width
            inset: view.contentX
            searchMode: view.filterText.length > 0
        }

        footer: Item {
            width: list.width
            height: 48
        }

        Basic.ScrollBar.vertical: UiScrollBar {}
    }

    // Empty and error states
    ColumnLayout {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 40
        width: Math.min(view.contentWidth, 420)
        visible: !GlobalShortcuts.loading && (filter.count === 0)
        spacing: 12

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
            radius: 32
            color: GlobalShortcuts.available ? Theme.accentSofter : Theme.alpha(Theme.warning, 0.12)

            Icon {
                anchors.centerIn: parent
                name: GlobalShortcuts.available ? (view.onlyConflicts ? "check" : "search") : "warning"
                size: 28
                color: GlobalShortcuts.available ? Theme.accentText : Theme.warning
            }
        }
        UiText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: !GlobalShortcuts.available ? i18nc("@info:status", "Shortcuts are not available")
                  : view.onlyConflicts ? i18nc("@info:status", "No conflicts")
                  : i18nc("@info:status", "No matching shortcuts")
            font.pointSize: Theme.fontTitle
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
        }
        UiText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: !GlobalShortcuts.available ? i18n("The list of shortcuts could not be loaded.")
                  : view.onlyConflicts ? i18n("No key combination is used by more than one action.")
                  : i18n("Try another name or key.")
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }
    }
}
