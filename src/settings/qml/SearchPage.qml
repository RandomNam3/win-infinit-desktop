// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Search results across all settings and shortcuts, grouped by category.
import QtQuick
import QtQuick.Layouts
import InfiniteDesktop.Settings

PageScaffold {
    id: page

    property string query

    signal navigate(string pageId)

    readonly property int total: allSettings.count + shortcutMatches.count + kdeMatches.count

    isSearchPage: true

    Component.onCompleted: {
        if (GlobalShortcuts.count === 0 && !GlobalShortcuts.loading) {
            GlobalShortcuts.reload();
        }
    }

    SettingsFilter {
        id: allSettings
        query: page.query
    }
    ShortcutFilter {
        id: shortcutMatches
        query: page.query
        highlightColor: Theme.accentText
    }
    GlobalShortcutsFilter {
        id: kdeMatches
        query: page.query
        excludeOwn: true
        highlightColor: Theme.accentText
    }

    PageHeader {
        iconName: "search"
        title: i18nc("@title search results page", "Search")
        subtitle: page.total > 0
                  ? i18ncp("@info %2 is the search text", "One result for “%2”", "%1 results for “%2”", page.total, page.query.trim())
                  : i18nc("@info %1 is the search text", "No results for “%1”", page.query.trim())
    }

    Repeater {
        model: Categories

        ColumnLayout {
            id: section

            required property string categoryId
            required property string title
            required property string iconName
            required property bool isShortcuts

            readonly property int matches: isShortcuts ? shortcutMatches.count : categoryFilter.count

            Layout.fillWidth: true
            visible: matches > 0
            spacing: 8

            SettingsFilter {
                id: categoryFilter
                category: section.isShortcuts ? "-" : section.categoryId
                query: page.query
                highlightColor: Theme.accentText
            }

            SectionLabel {
                text: section.title
                iconName: section.iconName

                UiButton {
                    variant: "link"
                    compact: true
                    text: i18nc("@action:button go to the page of a category", "Open page")
                    iconName: "arrow-right"
                    onClicked: page.navigate(section.categoryId)
                }
            }

            Card {
                Layout.fillWidth: true

                Repeater {
                    model: section.isShortcuts ? null : categoryFilter

                    SettingRow {
                        Layout.fillWidth: true
                        searchMode: true
                        last: index === categoryFilter.count - 1
                    }
                }
                Repeater {
                    model: section.isShortcuts ? shortcutMatches : null

                    ShortcutRow {
                        Layout.fillWidth: true
                        searchMode: true
                        enabled: Shortcuts.available || SystemTheme.screenshotMode
                        last: index === shortcutMatches.count - 1
                    }
                }
            }
        }
    }

    // Global shortcuts of other applications
    ColumnLayout {
        Layout.fillWidth: true
        visible: kdeMatches.count > 0
        spacing: 8

        SectionLabel {
            text: i18nc("@title search result section", "Shortcuts of Windows")
            iconName: "keyboard"

            UiButton {
                variant: "link"
                compact: true
                text: i18nc("@action:button go to the list of all shortcuts", "Show all")
                iconName: "arrow-right"
                onClicked: page.navigate("shortcuts-all")
            }
        }

        Card {
            Layout.fillWidth: true

            Repeater {
                model: kdeMatches

                AllShortcutRow {
                    Layout.fillWidth: true
                    inCard: true
                    searchMode: true
                    lastRow: index === kdeMatches.count - 1
                }
            }
        }
    }

    // Nothing found
    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: 48
        visible: page.total === 0
        spacing: 12

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 72
            Layout.preferredHeight: 72
            radius: 36
            color: Theme.accentSofter

            Icon {
                anchors.centerIn: parent
                name: "search"
                size: 32
                color: Theme.accentText
            }
        }
        UiText {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 4
            text: i18nc("@info:status", "No matching settings")
            font.pointSize: Theme.fontTitle
            font.weight: Font.DemiBold
        }
        UiText {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 420
            horizontalAlignment: Text.AlignHCenter
            text: i18n("Try a different word, for example “zoom”, “touchpad”, “grid” or “fullscreen”.")
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }
    }
}
