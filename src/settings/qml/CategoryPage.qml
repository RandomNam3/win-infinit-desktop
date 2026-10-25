// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// All settings of one category, generated from the kcfg items.
import QtQuick
import QtQuick.Layouts
import InfinitDesktop.Settings

PageScaffold {
    id: page

    property string categoryId

    signal resetAllRequested()
    signal forgetPlacementsRequested()

    function showRememberedWindows() {
        // Used for screenshots only.
        if (rememberedLoader.item) {
            rememberedLoader.item.expanded = true;
            scrollToEnd.restart();
        }
    }

    Timer {
        id: scrollToEnd
        // Wait until the expanded list is laid out.
        interval: 150
        onTriggered: page.contentY = Math.max(0, page.contentHeight - page.height)
    }

    function showAppList(withPicker) {
        // Used for screenshots only: scrolls to the last application list of the page.
        let target = null;
        for (let i = 0; i < rows.count; ++i) {
            const row = rows.itemAt(i);
            if (row && row.type === "stringlist") {
                target = row;
            }
        }
        if (!target) {
            return;
        }
        page.contentY = Math.max(0, Math.min(page.contentHeight - page.height, target.mapToItem(page.contentItem, 0, 0).y - 24));
        if (withPicker) {
            const editor = findAppListEditor(target);
            if (editor) {
                editor.openPicker();
            }
        }
    }

    function findAppListEditor(item) {
        for (let i = 0; i < item.children.length; ++i) {
            const child = item.children[i];
            if (child.openPicker !== undefined && child.shownItems !== undefined) {
                return child;
            }
            const found = findAppListEditor(child);
            if (found) {
                return found;
            }
        }
        return null;
    }

    function openFirstInfo() {
        const first = rows.itemAt(0);
        if (first) {
            first.openInfo();
        }
    }

    function openFirstDropdown() {
        // Used for screenshots only.
        for (let i = 0; i < rows.count; ++i) {
            const row = rows.itemAt(i);
            if (row && row.type === "enum") {
                const dropdown = findDropdown(row);
                if (dropdown) {
                    dropdown.popup.open();
                    return;
                }
            }
        }
    }

    function findDropdown(item) {
        for (let i = 0; i < item.children.length; ++i) {
            const child = item.children[i];
            if (child.popup !== undefined && child.delegateModel !== undefined && child.visible) {
                return child;
            }
            const found = findDropdown(child);
            if (found) {
                return found;
            }
        }
        return null;
    }

    function openColorPicker() {
        for (let i = 0; i < rows.count; ++i) {
            const row = rows.itemAt(i);
            if (row && row.type === "color") {
                const button = findColorButton(row);
                if (button) {
                    button.openPicker();
                }
                return;
            }
        }
    }

    function findColorButton(item) {
        for (let i = 0; i < item.children.length; ++i) {
            const child = item.children[i];
            if (child.openPicker !== undefined) {
                return child;
            }
            const found = findColorButton(child);
            if (found) {
                return found;
            }
        }
        return null;
    }

    PageHeader {
        iconName: Categories.icon(page.categoryId)
        title: Categories.title(page.categoryId)
        subtitle: Categories.subtitle(page.categoryId)
    }

    Card {
        Layout.fillWidth: true

        Repeater {
            id: rows

            model: SettingsFilter {
                id: filter
                category: page.categoryId
                highlightColor: Theme.accentText
            }

            SettingRow {
                Layout.fillWidth: true
                last: index === filter.count - 1
            }
        }
    }

    // Remembered window positions on the windows page.
    Loader {
        id: rememberedLoader
        Layout.fillWidth: true
        Layout.topMargin: 8
        active: page.categoryId === "windows"
        visible: active

        sourceComponent: RememberedWindowsCard {
            onForgetRequested: page.forgetPlacementsRequested()
        }
    }

    // Groups, areas and notes on the "Notes & Groups" page.
    Loader {
        Layout.fillWidth: true
        Layout.topMargin: 8
        active: page.categoryId === "organize"
        visible: active

        sourceComponent: WorkspaceCards {}
    }

    // Danger zone on the advanced page.
    Loader {
        Layout.fillWidth: true
        Layout.topMargin: 8
        active: page.categoryId === "advanced"
        visible: active

        sourceComponent: Card {
            RowLayout {
                id: resetRow

                Layout.fillWidth: true
                Layout.margins: 18
                spacing: 16

                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: Theme.radiusMedium
                    color: Theme.alpha(Theme.danger, 0.12)

                    Icon {
                        anchors.centerIn: parent
                        name: "reset"
                        size: 18
                        color: Theme.danger
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    UiText {
                        Layout.fillWidth: true
                        text: i18nc("@title", "Reset all settings")
                        font.weight: Font.Medium
                        wrapMode: Text.Wrap
                    }
                    UiText {
                        Layout.fillWidth: true
                        text: i18n("Restore the defaults of every setting on every page. Keyboard shortcuts are not changed.")
                        color: Theme.textSecondary
                        font.pointSize: Theme.fontSmall
                        wrapMode: Text.Wrap
                    }
                }
                UiButton {
                    variant: "dangerSoft"
                    text: i18nc("@action:button", "Reset All…")
                    enabled: !SettingsStore.allDefault
                    onClicked: page.resetAllRequested()
                }
            }
        }
    }
}
