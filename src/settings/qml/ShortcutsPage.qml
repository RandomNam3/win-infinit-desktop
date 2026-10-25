// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shortcuts: the plugin's own global shortcuts (grouped like in shortcutdefs.h)
// and, in a second tab, every global shortcut of the session with conflict detection.
import QtQuick
import QtQuick.Layouts
import InfinitDesktop.Settings

Item {
    id: page

    property string categoryId: "shortcuts"
    property bool isSearchPage: false
    property int currentTab: 0
    property bool allTabUsed: false

    signal resetAllRequested()

    onCurrentTabChanged: {
        if (currentTab === 1) {
            allTabUsed = true;
        }
    }
    Component.onCompleted: allTabUsed = currentTab === 1

    readonly property real sideMargin: width < 600 ? 16 : 32
    readonly property real contentWidth: Math.min(width - 2 * sideMargin, Theme.contentMaxWidth)

    function startFirstCapture() {
        // Used for screenshots only.
        currentTab = 0;
        const group = groups.itemAt(0);
        if (group) {
            group.firstRow().startCapture();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Sticky header with the tab switch
        Item {
            Layout.fillWidth: true
            implicitHeight: headerColumn.implicitHeight + 28 + 14

            ColumnLayout {
                id: headerColumn

                x: Math.round((page.width - width) / 2)
                y: 28
                width: page.contentWidth
                spacing: 18

                PageHeader {
                    Layout.bottomMargin: 0
                    iconName: Categories.icon(page.categoryId)
                    title: Categories.title(page.categoryId)
                    subtitle: Categories.subtitle(page.categoryId)

                    UiButton {
                        visible: page.currentTab === 0
                        variant: "secondary"
                        iconName: "reset"
                        text: i18nc("@action:button", "Reset All…")
                        enabled: !Shortcuts.allDefault
                        onClicked: page.resetAllRequested()
                    }
                    UiButton {
                        visible: page.currentTab === 1
                        variant: "secondary"
                        iconName: "external"
                        text: i18nc("@action:button open the Game Bar page of the Windows settings", "Game Bar Settings")
                        onClicked: GlobalShortcuts.openSystemSettings()
                    }
                }

                SegmentedControl {
                    model: [i18nc("@title:tab shortcuts of this plugin", "Infinit Desktop"),
                            GlobalShortcuts.conflictCount > 0
                            ? i18ncp("@title:tab %1 is the number of conflicts", "With Windows · %1 conflict", "With Windows · %1 conflicts", GlobalShortcuts.conflictCount)
                            : i18nc("@title:tab", "With Windows")]
                    currentIndex: page.currentTab
                    accessibleName: i18nc("@label", "Shortcut list")
                    onActivated: index => page.currentTab = index
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: page.currentTab

            PageScaffold {
                topPadding: 4

                // How recording works
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: hintRow.implicitHeight + 24
                    radius: Theme.radiusLarge
                    color: Theme.accentSofter

                    RowLayout {
                        id: hintRow

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12

                        Icon {
                            Layout.alignment: Qt.AlignTop
                            name: "info"
                            size: 18
                            color: Theme.accentText
                        }
                        UiText {
                            Layout.fillWidth: true
                            text: Shortcuts.available || SystemTheme.screenshotMode
                                  ? i18n("Click a shortcut and press the new key combination. Backspace removes it, Esc cancels. Changes are applied immediately.")
                                  : i18n("The settings file cannot be written, so shortcuts cannot be changed right now. The defaults are shown.")
                            wrapMode: Text.Wrap
                            font.pointSize: Theme.fontSmall
                        }
                    }
                }

                Repeater {
                    id: groups

                    model: Shortcuts.groups()

                    ColumnLayout {
                        id: group

                        required property string modelData

                        function firstRow() {
                            return rowRepeater.itemAt(0);
                        }

                        Layout.fillWidth: true
                        spacing: 8

                        SectionLabel {
                            text: Shortcuts.groupTitle(group.modelData)
                        }

                        Card {
                            Layout.fillWidth: true

                            Repeater {
                                id: rowRepeater

                                model: ShortcutFilter {
                                    id: filter
                                    group: group.modelData
                                }

                                ShortcutRow {
                                    Layout.fillWidth: true
                                    enabled: Shortcuts.available || SystemTheme.screenshotMode
                                    last: index === filter.count - 1
                                }
                            }
                        }
                    }
                }
            }

            Loader {
                // Created on first use: reading all shortcuts takes a moment.
                active: page.allTabUsed
                sourceComponent: AllShortcutsView {}
            }
        }
    }
}
