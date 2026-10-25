// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Start page: plugin status, the update check, a short "getting around" guide that
// follows the current settings and shortcuts, and tiles for all categories.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import InfinitDesktop.Settings

PageScaffold {
    id: page

    signal navigate(string pageId)
    signal resetAllRequested()

    property int settingsRevision: 0
    property int shortcutRevision: 0

    function setting(key) {
        settingsRevision;
        return SettingsStore.value(key);
    }
    function keysOf(id) {
        shortcutRevision;
        return Shortcuts.currentKeys(id);
    }
    function choiceKeys(key, index) {
        return SettingsStore.formatValue(key, index).split("+").map(s => s.trim()).filter(s => s.length > 0);
    }

    readonly property var tips: {
        const result = [];
        if (setting("DragDesktopToPan")) {
            result.push({ icon: "move", text: i18n("Drag the empty desktop to move the canvas"), keys: [] });
        } else if (setting("MiddleButtonPan")) {
            result.push({ icon: "move", text: i18n("Drag the empty desktop with the middle mouse button to move the canvas"), keys: [] });
        }
        const panModifier = setting("PanModifier");
        if (panModifier > 0) {
            result.push({ icon: "mouse", text: i18n("Pan anywhere, even over windows: hold these keys and drag"), keys: choiceKeys("PanModifier", panModifier) });
        }
        const zoomModifier = setting("ZoomModifier");
        if (setting("WheelZoomOnDesktop") && zoomModifier > 0) {
            result.push({ icon: "zoom", text: i18n("Zoom with the mouse wheel on the empty desktop, or anywhere while holding"), keys: choiceKeys("ZoomModifier", zoomModifier) });
        } else if (zoomModifier > 0) {
            result.push({ icon: "zoom", text: i18n("Zoom anywhere: hold these keys and turn the mouse wheel"), keys: choiceKeys("ZoomModifier", zoomModifier) });
        } else if (setting("WheelZoomOnDesktop")) {
            result.push({ icon: "zoom", text: i18n("Turn the mouse wheel over the empty desktop to zoom"), keys: [] });
        }
        if (setting("TouchpadScrollPans")) {
            result.push({ icon: "touchpad", text: i18n("Scroll with two fingers on the touchpad over the empty desktop to pan"), keys: [] });
        }
        const shortcutTips = [
            ["overview", "InfinitDesktop Overview"],
            ["home", "InfinitDesktop Home"],
            ["pin", "InfinitDesktop TogglePin"],
            ["layers", "InfinitDesktop TogglePinAbove"]
        ];
        for (const tip of shortcutTips) {
            const title = Shortcuts.title(tip[1]);
            const keys = keysOf(tip[1]);
            if (title.length > 0 && keys.length > 0) {
                result.push({ icon: tip[0], text: title, keys: keys });
            }
        }
        return result;
    }

    Connections {
        target: SettingsStore
        function onValueChanged() {
            page.settingsRevision++;
        }
    }
    Connections {
        target: Shortcuts
        function onShortcutChanged() {
            page.shortcutRevision++;
        }
    }

    // --- Hero / status -------------------------------------------------------
    Rectangle {
        id: hero

        readonly property string status: Effect.state
        readonly property bool problem: StatusTexts.isProblem(status)

        Layout.fillWidth: true
        implicitHeight: heroRow.implicitHeight + 56
        radius: Theme.radiusXLarge
        color: Theme.surface
        border.width: 1
        border.color: problem ? Theme.alpha(StatusTexts.color(status), 0.45) : Theme.border

        Shadow {
            radius: hero.radius
            visible: !Theme.dark
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: hero.radius - 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.alpha(hero.problem ? StatusTexts.color(hero.status) : Theme.accent, 0) }
                GradientStop { position: 1.0; color: Theme.alpha(hero.problem ? StatusTexts.color(hero.status) : Theme.accent, Theme.dark ? 0.12 : 0.08) }
            }
        }

        RowLayout {
            id: heroRow

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            spacing: 28

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                // Status chip
                Rectangle {
                    Layout.preferredWidth: chipRow.implicitWidth + 22
                    Layout.preferredHeight: 26
                    radius: 13
                    color: Theme.alpha(StatusTexts.color(hero.status), Theme.dark ? 0.16 : 0.11)

                    Row {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: 8

                        StatusDot {
                            anchors.verticalCenter: parent.verticalCenter
                            status: hero.status
                        }
                        UiText {
                            anchors.verticalCenter: parent.verticalCenter
                            text: StatusTexts.title(hero.status)
                            color: hero.status === "disabled" ? Theme.textSecondary : StatusTexts.color(hero.status)
                            font.pointSize: Theme.fontSmall
                            font.weight: Font.DemiBold
                        }
                    }
                }

                UiText {
                    Layout.fillWidth: true
                    text: i18nc("@title application name", "WIN Infinit Desktop")
                    font.pointSize: Theme.fontDisplay
                    font.weight: Font.Bold
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }
                UiText {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 520
                    text: StatusTexts.description(hero.status)
                    color: Theme.textSecondary
                    wrapMode: Text.Wrap
                    lineHeight: 1.12
                }

                RowLayout {
                    Layout.topMargin: 6
                    spacing: 8

                    UiButton {
                        visible: hero.status === "disabled"
                        variant: "primary"
                        iconName: "power"
                        text: i18nc("@action:button turn the plugin on", "Turn On")
                        enabled: !Effect.busy
                        onClicked: Effect.setEnabled(true)
                    }
                    UiButton {
                        visible: hero.status === "safemode"
                        variant: "primary"
                        iconName: "shield"
                        text: i18nc("@action:button", "Leave Safe Mode")
                        enabled: !Effect.busy
                        onClicked: Effect.leaveSafeMode()
                    }
                    UiButton {
                        visible: hero.status === "paused"
                        variant: "primary"
                        iconName: "play"
                        text: i18nc("@action:button resume the paused canvas", "Resume")
                        onClicked: Effect.setPaused(false)
                    }
                    UiButton {
                        visible: hero.status === "active"
                        variant: "secondary"
                        iconName: "pause"
                        text: i18nc("@action:button pause the canvas", "Pause")
                        onClicked: Effect.setPaused(true)
                    }
                    UiButton {
                        visible: hero.status === "active" || hero.status === "paused" || hero.status === "safemode"
                        variant: "ghost"
                        iconName: "power"
                        text: i18nc("@action:button turn the plugin off", "Turn Off")
                        enabled: !Effect.busy
                        onClicked: Effect.setEnabled(false)
                    }
                    UiButton {
                        visible: hero.status === "notinstalled" || hero.status === "unknown"
                        variant: "secondary"
                        iconName: "reset"
                        text: i18nc("@action:button check the plugin state again", "Check Again")
                        onClicked: Effect.refresh()
                    }
                }

                UiText {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    visible: Effect.effectVersion.length > 0 && Effect.effectVersion !== Effect.appVersion
                    text: i18n("Version %1 of the canvas service is running, these settings belong to version %2. Quit WIN Infinit Desktop from its icon in the notification area and start it again.", Effect.effectVersion, Effect.appVersion)
                    color: Theme.warning
                    font.pointSize: Theme.fontSmall
                    wrapMode: Text.Wrap
                }
            }

            CanvasIllustration {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 272
                Layout.preferredHeight: 172
                visible: hero.width > 660
                live: page.visible
            }
        }
    }

    // --- Updates ----------------------------------------------------------------
    UpdatesCard {
        Layout.fillWidth: true
    }

    // --- Getting around -------------------------------------------------------
    SectionLabel {
        Layout.topMargin: 12
        text: i18nc("@title quick guide on the start page", "Getting around")
    }

    Card {
        Layout.fillWidth: true

        Repeater {
            model: page.tips

            Item {
                id: tip

                required property int index
                required property var modelData

                Layout.fillWidth: true
                implicitHeight: Math.max(52, tipRow.implicitHeight + 20)

                RowLayout {
                    id: tipRow

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                    anchors.rightMargin: 18
                    spacing: 14

                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: Theme.radiusMedium
                        color: Theme.accentSofter

                        Icon {
                            anchors.centerIn: parent
                            name: tip.modelData.icon
                            size: 18
                            color: Theme.accentText
                        }
                    }
                    UiText {
                        Layout.fillWidth: true
                        text: tip.modelData.text
                        wrapMode: Text.Wrap
                    }
                    Keycaps {
                        visible: tip.modelData.keys.length > 0
                        keys: tip.modelData.keys
                    }
                }

                Rectangle {
                    visible: tip.index < page.tips.length - 1
                    anchors.bottom: parent.bottom
                    x: 62
                    width: parent.width - x - 18
                    height: 1
                    color: Theme.divider
                }
            }
        }
    }

    // --- Categories ------------------------------------------------------------
    SectionLabel {
        Layout.topMargin: 12
        text: i18nc("@title", "All settings")
    }

    GridLayout {
        id: tiles

        Layout.fillWidth: true
        columns: width > 700 ? 3 : width > 420 ? 2 : 1
        columnSpacing: 12
        rowSpacing: 12

        Repeater {
            model: Categories

            Basic.AbstractButton {
                id: tile

                required property string categoryId
                required property string title
                required property string subtitle
                required property string iconName
                required property int entryCount
                required property bool isShortcuts

                Layout.fillWidth: true
                Layout.preferredWidth: 100
                Layout.preferredHeight: tileColumn.implicitHeight + 32
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus

                Accessible.role: Accessible.Button
                Accessible.name: title
                Accessible.description: subtitle

                onClicked: page.navigate(categoryId)

                background: Rectangle {
                    radius: Theme.radiusLarge
                    color: tile.down ? Theme.surfaceSunken : Theme.surface
                    border.width: 1
                    border.color: tile.hovered ? Theme.alpha(Theme.accentText, 0.45) : Theme.border

                    Behavior on border.color {
                        ColorAnimation { duration: Theme.durationFast }
                    }

                    Shadow {
                        radius: parent.radius
                        visible: !Theme.dark
                    }

                    FocusRing {
                        radius: parent.radius + offset
                        visible: tile.visualFocus
                    }
                }

                contentItem: Item {
                    ColumnLayout {
                        id: tileColumn

                        x: 16
                        y: 16
                        width: parent.width - 32
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.bottomMargin: 8
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                radius: Theme.radiusMedium
                                color: tile.hovered ? Theme.accentSoft : Theme.accentSofter

                                Icon {
                                    anchors.centerIn: parent
                                    name: tile.iconName
                                    size: 20
                                    color: Theme.accentText
                                }
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Icon {
                                name: "chevron-right"
                                size: 16
                                color: tile.hovered ? Theme.accentText : Theme.textTertiary
                            }
                        }
                        UiText {
                            Layout.fillWidth: true
                            text: tile.title
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        UiText {
                            Layout.fillWidth: true
                            text: tile.subtitle
                            color: Theme.textSecondary
                            font.pointSize: Theme.fontSmall
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    // --- Footer ------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 8

        UiText {
            Layout.fillWidth: true
            text: i18nc("@info %1 is a version number", "Version %1", Effect.appVersion)
            color: Theme.textTertiary
            font.pointSize: Theme.fontSmall
        }
        UiButton {
            variant: "ghost"
            compact: true
            iconName: "reset"
            text: i18nc("@action:button", "Reset All Settings…")
            enabled: !SettingsStore.allDefault
            onClicked: page.resetAllRequested()
        }
    }
}
