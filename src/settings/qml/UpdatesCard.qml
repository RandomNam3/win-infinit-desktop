// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Update check on the start page: newer versions with their release notes. The
// update downloads the package of the new release (with progress), unpacks it and
// runs its installer without a window; the installer closes this app and opens it
// again, which then shows the result.
import QtQuick
import QtQuick.Layouts
import InfinitDesktop.Settings

Card {
    id: card

    readonly property string status: Updates.status
    readonly property bool available: status === "available"
    // The newest release has a package for this computer (win-infinit-desktop-X.Y.Z-x64.zip).
    readonly property bool hasPackage: Updates.releases.length > 0 && Updates.updateCommand().length > 0
    readonly property string install: Updates.installStatus
    readonly property bool installing: install === "downloading" || install === "unpacking" || install === "installing"

    readonly property string title: {
        switch (status) {
        case "checking":
            return i18n("Looking for updates…");
        case "uptodate":
            return i18nc("@info %1 is a version number", "Version %1 is up to date", Updates.currentVersion);
        case "available":
            return i18nc("@info %1 is a version number", "Version %1 is available", Updates.latestVersion);
        case "error":
            return i18n("Could not look for updates");
        default:
            return i18nc("@info %1 is a version number", "Version %1", Updates.currentVersion);
        }
    }
    readonly property string subtitle: {
        switch (status) {
        case "uptodate":
            return isNaN(Updates.lastCheck) ? "" : i18nc("@info %1 is a time", "Checked at %1", Updates.lastCheck.toLocaleTimeString(Qt.locale(), Locale.ShortFormat));
        case "available":
            return i18nc("@info %1 is a version number", "You have version %1. What is new:", Updates.currentVersion);
        case "error":
            return Updates.errorString;
        case "checking":
            return "";
        default:
            return i18n("Not checked for updates yet");
        }
    }

    // --- Summary ----------------------------------------------------------------
    Item {
        Layout.fillWidth: true
        implicitHeight: Math.max(56, summary.implicitHeight + 24)

        RowLayout {
            id: summary

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 14

            Rectangle {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                radius: Theme.radiusMedium
                color: card.status === "error" ? Theme.alpha(Theme.warning, 0.14) : card.available ? Theme.accentSoft : Theme.accentSofter

                Icon {
                    anchors.centerIn: parent
                    name: card.status === "error" ? "alert" : card.status === "uptodate" ? "check" : card.available ? "arrow-right" : "reset"
                    size: 18
                    color: card.status === "error" ? Theme.warning : Theme.accentText
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                UiText {
                    Layout.fillWidth: true
                    text: card.title
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                }
                UiText {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: card.subtitle
                    color: Theme.textSecondary
                    font.pointSize: Theme.fontSmall
                    wrapMode: Text.Wrap
                }
            }
            UiButton {
                visible: !card.available
                variant: "secondary"
                compact: true
                iconName: "reset"
                text: i18nc("@action:button look for a new version", "Check Now")
                enabled: card.status !== "checking"
                onClicked: Updates.check()
            }
        }
    }

    // --- Result of the last update (shown after the installer opened this app again) ----
    Rectangle {
        visible: card.install === "done" || card.install === "failed"
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.bottomMargin: 12
        implicitHeight: result.implicitHeight + 20
        radius: Theme.radiusMedium
        color: card.install === "done" ? Theme.alpha(Theme.success, Theme.dark ? 0.14 : 0.09) : Theme.alpha(Theme.warning, Theme.dark ? 0.12 : 0.08)

        ColumnLayout {
            id: result

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Icon {
                    Layout.alignment: Qt.AlignTop
                    name: card.install === "done" ? "check" : "alert"
                    size: 16
                    color: card.install === "done" ? Theme.success : Theme.warning
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    UiText {
                        Layout.fillWidth: true
                        text: card.install === "done" ? (Updates.installMessage.length > 0 ? i18nc("@info %1 is a version number", "Version %1 is installed.", Updates.installMessage)
                                                                                         : i18n("The update is installed."))
                                                      : i18n("The update failed.")
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                    }
                    UiText {
                        Layout.fillWidth: true
                        visible: card.install === "failed" && text.length > 0
                        text: Updates.installMessage
                        color: Theme.textSecondary
                        font.pointSize: Theme.fontSmall
                        wrapMode: Text.Wrap
                    }
                }
            }
            RowLayout {
                visible: card.install === "failed"
                Layout.fillWidth: true
                spacing: 8

                UiButton {
                    visible: card.available && card.hasPackage
                    variant: "secondary"
                    compact: true
                    iconName: "reset"
                    text: i18nc("@action:button", "Try Again")
                    onClicked: Updates.installUpdate()
                }
                UiButton {
                    visible: Updates.hasInstallLog
                    variant: "ghost"
                    compact: true
                    iconName: "external"
                    text: i18nc("@action:button", "Open Log")
                    onClicked: Updates.openInstallLog()
                }
                Item {
                    Layout.fillWidth: true
                }
            }
        }
    }

    // --- Release notes and update -------------------------------------------------
    Rectangle {
        visible: card.available
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        implicitHeight: 1
        color: Theme.divider
    }

    Repeater {
        model: card.available ? Updates.releases : []

        ColumnLayout {
            id: release

            required property var modelData

            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 12
            spacing: 4

            UiText {
                Layout.fillWidth: true
                text: isNaN(release.modelData.date) ? i18nc("@title %1 is a version number", "Version %1", release.modelData.version)
                                                    : i18nc("@title %1 is a version number, %2 a date", "Version %1 – %2", release.modelData.version,
                                                            release.modelData.date.toLocaleDateString(Qt.locale(), Locale.ShortFormat))
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }
            UiText {
                Layout.fillWidth: true
                visible: text.length > 0
                text: release.modelData.notes
                textFormat: Text.MarkdownText
                color: Theme.textSecondary
                wrapMode: Text.Wrap
                onLinkActivated: link => Qt.openUrlExternally(link)

                HoverHandler {
                    cursorShape: parent.hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
        }
    }

    Rectangle {
        visible: card.available
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.topMargin: 12
        implicitHeight: hint.implicitHeight + 20
        radius: Theme.radiusMedium
        color: Theme.alpha(Theme.warning, Theme.dark ? 0.12 : 0.08)

        RowLayout {
            id: hint

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            Icon {
                Layout.alignment: Qt.AlignTop
                name: "info"
                size: 16
                color: Theme.warning
            }
            UiText {
                Layout.fillWidth: true
                text: card.hasPackage ? i18n("The update downloads the new version from GitHub, closes WIN Infinit Desktop and this window for a moment, installs the new version and starts both again.")
                                     : i18n("This release has no package for this computer yet. Download it from GitHub.")
                font.pointSize: Theme.fontSmall
                wrapMode: Text.Wrap
            }
        }
    }

    RowLayout {
        visible: card.available
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.topMargin: 12
        Layout.bottomMargin: 4
        spacing: 8

        UiButton {
            visible: card.hasPackage
            variant: "primary"
            iconName: "arrow-right"
            text: i18nc("@action:button", "Install Update")
            enabled: !card.installing
            onClicked: Updates.installUpdate()
        }
        UiButton {
            variant: "ghost"
            iconName: "external"
            text: i18nc("@action:button", "Open on GitHub")
            onClicked: Qt.openUrlExternally(Updates.releasesUrl)
        }
        Item {
            Layout.fillWidth: true
        }
    }

    // --- Progress -------------------------------------------------------------------
    ColumnLayout {
        visible: card.installing
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.topMargin: 8
        spacing: 8

        UiText {
            Layout.fillWidth: true
            text: {
                switch (card.install) {
                case "downloading":
                    return Updates.installProgress >= 0
                        ? i18nc("@info %1 is a version number, %2 a percentage", "Downloading version %1 … %2 %", Updates.latestVersion, Math.round(Updates.installProgress * 100))
                        : i18nc("@info %1 is a version number", "Downloading version %1 …", Updates.latestVersion);
                case "unpacking":
                    return i18n("Unpacking …");
                default:
                    return i18n("Installing … WIN Infinit Desktop and this window close for a moment and open again when the update is done.");
                }
            }
            font.pointSize: Theme.fontSmall
            wrapMode: Text.Wrap
        }
        // The share of the download, or a light running back and forth while the length is unknown.
        Rectangle {
            id: track

            Layout.fillWidth: true
            implicitHeight: 6
            radius: 3
            color: Theme.track
            clip: true

            readonly property bool determinate: card.install === "downloading" && Updates.installProgress >= 0

            Rectangle {
                visible: track.determinate
                width: track.width * Math.max(0, Math.min(1, Updates.installProgress))
                height: parent.height
                radius: 3
                color: Theme.accentFill

                Behavior on width {
                    enabled: Theme.animations
                    NumberAnimation { duration: Theme.durationNormal }
                }
            }
            Rectangle {
                id: runner

                visible: !track.determinate
                width: track.width * 0.3
                height: parent.height
                radius: 3
                color: Theme.accentFill
                x: Theme.animations ? -width : (track.width - width) / 2

                SequentialAnimation on x {
                    running: Theme.animations && card.installing && !track.determinate && card.visible
                    loops: Animation.Infinite
                    NumberAnimation { from: -runner.width; to: track.width; duration: 1200; easing.type: Easing.InOutQuad }
                    PauseAnimation { duration: 150 }
                }
            }
        }
    }

    ColumnLayout {
        visible: card.available && card.install === "failed" && Updates.updateCommand().length > 0
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.topMargin: 8
        spacing: 6

        UiText {
            Layout.fillWidth: true
            text: i18n("You can also run the update by hand, in PowerShell:")
            color: Theme.textSecondary
            font.pointSize: Theme.fontSmall
            wrapMode: Text.Wrap
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: command.implicitHeight + 16
            radius: Theme.radiusSmall
            color: Theme.surfaceSunken

            TextEdit {
                id: command

                x: 8
                y: 8
                width: parent.width - 16
                text: Updates.updateCommand()
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                color: Theme.text
                selectionColor: Theme.accentSoft
                font.family: "Consolas"
                font.pointSize: Theme.fontSmall
            }
        }
    }

    // --- Automatic check ----------------------------------------------------------
    Rectangle {
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.topMargin: card.available ? 12 : 0
        implicitHeight: 1
        color: Theme.divider
    }
    Item {
        Layout.fillWidth: true
        implicitHeight: Math.max(48, autoRow.implicitHeight + 16)

        RowLayout {
            id: autoRow

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            UiText {
                Layout.fillWidth: true
                text: i18n("Look for updates when this app starts")
                color: Theme.textSecondary
                font.pointSize: Theme.fontSmall
                wrapMode: Text.Wrap
            }
            ToggleSwitch {
                checked: Updates.checkOnStart
                Accessible.name: i18n("Look for updates when this app starts")
                onToggled: Updates.checkOnStart = checked
            }
        }
    }
}
