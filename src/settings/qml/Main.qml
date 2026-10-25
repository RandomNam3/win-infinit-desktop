// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Main window: sidebar navigation, search and the page stack.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import InfinitDesktop.Settings

Basic.ApplicationWindow {
    id: window

    property string initialPage
    property string currentPage: "overview"
    property string query: ""
    readonly property bool searching: query.trim().length > 0
    readonly property bool compact: width < Theme.compactBreakpoint

    property int shortcutsTab: 0

    function navigate(pageId) {
        if (pageId === "shortcuts-all") {
            pageId = "shortcuts";
            shortcutsTab = 1;
            if (stack.currentItem && stack.currentItem.pageId === "shortcuts") {
                stack.currentItem.currentTab = 1;
            }
        } else if (pageId === "shortcuts") {
            shortcutsTab = 0;
        }
        if (searchField.text.length > 0) {
            searchField.text = "";
        }
        currentPage = pageId;
        Qt.callLater(showCurrentPage);
    }

    function showCurrentPage() {
        let component = null;
        let properties = {};
        if (searching) {
            if (stack.currentItem && stack.currentItem.isSearchPage) {
                return;
            }
            component = searchPageComponent;
        } else if (currentPage === "overview") {
            component = overviewComponent;
        } else if (currentPage === "shortcuts") {
            component = shortcutsComponent;
        } else {
            component = categoryComponent;
            properties = { "categoryId": currentPage };
        }
        if (stack.currentItem && !searching && stack.currentItem.pageId === currentPage && !stack.currentItem.isSearchPage) {
            return;
        }
        if (stack.depth === 0) {
            stack.push(component, properties, Basic.StackView.Immediate);
        } else {
            stack.replace(component, properties);
        }
    }

    function confirmResetSettings() {
        confirmDialog.ask(i18nc("@title:window", "Reset all settings?"),
                          i18n("Every setting on every page goes back to its default value. Keyboard shortcuts are not changed."),
                          i18nc("@action:button", "Reset All"),
                          () => {
                              SettingsStore.resetAll();
                              toast.show(i18n("All settings were reset"), "reset");
                          });
    }

    function confirmForgetPlacements() {
        confirmDialog.ask(i18nc("@title:window", "Forget all remembered positions?"),
                          i18n("Applications will open at their normal place again until their windows are closed once more."),
                          i18nc("@action:button", "Forget All"),
                          () => RememberedWindows.forgetAll());
    }

    function confirmResetShortcuts() {
        confirmDialog.ask(i18nc("@title:window", "Reset all shortcuts?"),
                          i18n("All Infinit Desktop shortcuts go back to their default key combinations. Shortcuts of other applications are not changed."),
                          i18nc("@action:button", "Reset All"),
                          () => {
                              Shortcuts.resetAll();
                              toast.show(i18n("All shortcuts were reset"), "reset");
                          });
    }

    // Called by the --screenshot runner: "<light|dark>:<page>[:<argument>]".
    // A second start of the app asks the running one to show a page (main.cpp).
    function openPage(page) {
        const ids = Categories.ids();
        if (page === "overview" || ids.indexOf(page) >= 0) {
            currentPage = page;
            showCurrentPage();
        }
    }

    function screenshotPrepare(command) {
        const parts = String(command).split(":");
        SystemTheme.forcedScheme = parts[0];
        confirmDialog.close();
        const what = parts[1];
        if (what === "search") {
            searchField.text = parts[2];
            return;
        }
        let target = what;
        let action = null;
        if (what === "info") {
            target = parts[2];
            action = page => page.openFirstInfo();
        } else if (what === "dialog") {
            target = "advanced";
            action = () => window.confirmResetSettings();
        } else if (what === "capture") {
            target = "shortcuts";
            action = page => page.startFirstCapture();
        } else if (what === "color") {
            target = "appearance";
            action = page => page.openColorPicker();
        } else if (what === "dropdown") {
            target = "mouse";
            action = page => page.openFirstDropdown();
        } else if (what === "allshortcuts") {
            target = "shortcuts-all";
        } else if (what === "remembered") {
            target = "windows";
            action = page => page.showRememberedWindows();
        } else if (what === "applist") {
            target = "windows";
            action = page => page.showAppList(false);
        } else if (what === "picker") {
            target = "windows";
            action = page => page.showAppList(true);
        }
        navigate(target);
        if (action) {
            screenshotAction.action = action;
            screenshotAction.target = target === "shortcuts-all" ? "shortcuts" : target;
            screenshotAction.attempts = 0;
            screenshotAction.restart();
        }
    }

    width: 1120
    height: 760
    minimumWidth: 480
    minimumHeight: 420
    visible: true
    title: i18nc("@title:window", "WIN Infinit Desktop")
    color: Theme.background
    font.pointSize: Theme.fontBody

    onSearchingChanged: Qt.callLater(showCurrentPage)
    onClosing: SettingsStore.flush()

    Component.onCompleted: {
        pagesModel.append({ "pageId": "overview", "pageTitle": i18nc("@title start page", "Overview"), "pageIcon": "home", "section": "" });
        const ids = Categories.ids();
        for (const id of ids) {
            pagesModel.append({ "pageId": id, "pageTitle": Categories.title(id), "pageIcon": Categories.icon(id), "section": i18nc("@title sidebar section", "Settings") });
        }
        if (initialPage.length > 0 && (initialPage === "overview" || ids.indexOf(initialPage) >= 0)) {
            currentPage = initialPage;
        }
        if (SystemTheme.screenshotMode) {
            // Show the reset button of at least one row in the screenshots.
            SettingsStore.setValue("ZoomStep", 20);
            SettingsStore.setValue("GridSpacing", 48);
        }
        showCurrentPage();
    }

    ListModel {
        id: pagesModel
    }

    Timer {
        id: screenshotAction
        property var action: null
        property string target
        property int attempts: 0
        interval: 100
        onTriggered: {
            const page = stack.currentItem;
            if (!page || page.pageId !== target || stack.busy) {
                if (++attempts < 20) {
                    restart();
                }
                return;
            }
            if (action) {
                action(page);
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: searchField.focusField()
    }
    Shortcut {
        sequences: [StandardKey.Quit]
        onActivated: window.close()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar

            Layout.fillHeight: true
            Layout.preferredWidth: window.compact ? Theme.sidebarCompactWidth : Theme.sidebarWidth
            compact: window.compact
            pages: pagesModel
            currentPage: window.searching ? "" : window.currentPage
            onNavigate: pageId => window.navigate(pageId)
            onSearchRequested: searchField.focusField()

            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: window.compact ? 62 : 0
                visible: window.compact

                Item {
                    id: compactSearchSlot
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 14
                    anchors.bottomMargin: 10
                }
            }

            Basic.StackView {
                id: stack

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                replaceEnter: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationNormal; easing.type: Easing.OutCubic }
                        NumberAnimation { property: "y"; from: 12; to: 0; duration: Theme.durationSlow; easing.type: Easing.OutCubic }
                    }
                }
                replaceExit: Transition {
                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durationFast }
                }
                pushEnter: Transition {}
                pushExit: Transition {}
            }
        }
    }

    SearchField {
        id: searchField

        parent: window.compact ? compactSearchSlot : sidebar.searchSlot
        anchors.fill: parent
        shortcutHint: i18nc("@info keyboard shortcut of the search field", "Ctrl+F")
        onTextChanged: window.query = text
        onAccepted: {
            if (stack.currentItem) {
                stack.currentItem.forceActiveFocus(Qt.TabFocusReason);
            }
        }
    }

    Component {
        id: overviewComponent

        OverviewPage {
            property string pageId: "overview"
            onNavigate: pageId => window.navigate(pageId)
            onResetAllRequested: window.confirmResetSettings()
        }
    }
    Component {
        id: categoryComponent

        CategoryPage {
            readonly property string pageId: categoryId
            onResetAllRequested: window.confirmResetSettings()
            onForgetPlacementsRequested: window.confirmForgetPlacements()
        }
    }
    Component {
        id: shortcutsComponent

        ShortcutsPage {
            readonly property string pageId: "shortcuts"
            currentTab: window.shortcutsTab
            onResetAllRequested: window.confirmResetShortcuts()
        }
    }
    Component {
        id: searchPageComponent

        SearchPage {
            readonly property string pageId: ""
            query: window.query
            onNavigate: pageId => window.navigate(pageId)
        }
    }

    Toast {
        id: toast
    }

    ConfirmDialog {
        id: confirmDialog
    }

    Connections {
        target: RememberedWindows
        function onForgotten() {
            toast.show(i18n("Remembered positions were removed"), "check");
        }
        function onApplicationForgotten(name) {
            toast.show(i18nc("@info %1 is an application", "Forgot the position of “%1”", name), "check");
        }
        function onFailed(message) {
            toast.show(message, "alert", true);
        }
    }

    Connections {
        target: Effect
        function onActionFailed(message) {
            toast.show(message, "alert", true);
        }
    }
}
