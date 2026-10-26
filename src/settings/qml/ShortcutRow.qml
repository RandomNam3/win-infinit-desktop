// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One global shortcut with its key combinations, reset and conflict handling.
// Used as delegate for ShortcutFilter models.
import QtQuick
import QtQuick.Layouts
import InfiniteDesktop.Settings

Item {
    id: row

    required property int index
    required property string shortcutId
    required property string title
    required property string description
    required property string highlightedTitle
    required property string highlightedDescription
    required property var keys
    required property var sequenceTexts
    required property bool isDefault
    required property string defaultText

    property bool last: false
    property bool searchMode: false

    // Pending assignment that conflicts with other actions.
    property var pending: null
    property string error

    readonly property real horizontalPadding: 18
    readonly property bool stacked: width < 560

    function startCapture() {
        captureRepeater.itemAt(0).startRecording();
    }

    function tryAssign(slot, sequence) {
        error = "";
        const conflicts = Shortcuts.conflicts(shortcutId, sequence);
        if (conflicts.length > 0) {
            pending = {
                slot: slot,
                sequence: sequence,
                keys: Shortcuts.keysFor(sequence),
                conflicts: conflicts
            };
            return;
        }
        pending = null;
        if (!Shortcuts.assign(shortcutId, slot, sequence, false)) {
            error = i18n("The shortcut could not be assigned. It may be reserved by the system.");
        }
    }

    implicitHeight: layout.implicitHeight + 28
    Accessible.role: Accessible.Grouping
    Accessible.name: title

    HoverHandler {
        id: hoverHandler
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 4
        radius: Theme.radiusMedium
        color: hoverHandler.hovered ? Theme.hover : Theme.alpha(Theme.hover, 0)

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }
    }

    ColumnLayout {
        id: layout

        x: row.horizontalPadding
        width: row.width - 2 * row.horizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        GridLayout {
            Layout.fillWidth: true
            columns: row.stacked ? 1 : 2
            columnSpacing: 20
            rowSpacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                UiText {
                    Layout.fillWidth: true
                    text: row.searchMode ? row.highlightedTitle : row.title
                    textFormat: row.searchMode ? Text.StyledText : Text.PlainText
                    font.weight: Font.Medium
                    wrapMode: Text.Wrap
                }
                UiText {
                    Layout.fillWidth: true
                    text: row.searchMode ? row.highlightedDescription : row.description
                    textFormat: row.searchMode ? Text.StyledText : Text.PlainText
                    color: Theme.textSecondary
                    font.pointSize: Theme.fontSmall
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }

            RowLayout {
                Layout.alignment: row.stacked ? Qt.AlignLeft : Qt.AlignRight | Qt.AlignVCenter
                spacing: 6
                layoutDirection: row.stacked ? Qt.RightToLeft : Qt.LeftToRight

                IconButton {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    iconName: "reset"
                    iconSize: 15
                    tooltip: i18nc("@action:button %1 is a key combination", "Reset to default (%1)", row.defaultText)
                    opacity: row.isDefault ? 0 : 1
                    visible: opacity > 0
                    onClicked: {
                        row.pending = null;
                        row.error = "";
                        Shortcuts.resetToDefault(row.shortcutId);
                    }

                    Behavior on opacity {
                        NumberAnimation { duration: Theme.durationNormal }
                    }
                }

                Repeater {
                    id: captureRepeater

                    model: Math.max(1, row.keys.length)

                    KeyCapture {
                        required property int index

                        keys: index < row.keys.length ? row.keys[index] : []
                        shortcutText: index < row.sequenceTexts.length ? row.sequenceTexts[index] : ""
                        actionTitle: row.title
                        onCaptured: sequence => row.tryAssign(index, sequence)
                        onCleared: {
                            row.pending = null;
                            Shortcuts.clear(row.shortcutId, index);
                        }
                    }
                }
            }
        }

        ConflictBanner {
            Layout.fillWidth: true
            pending: row.pending
            onCancel: row.pending = null
            onReassign: {
                const p = row.pending;
                row.pending = null;
                if (!Shortcuts.assign(row.shortcutId, p.slot, p.sequence, true)) {
                    row.error = i18n("The shortcut could not be assigned. It may be reserved by the system.");
                }
            }
        }

        UiText {
            Layout.fillWidth: true
            visible: row.error.length > 0
            text: row.error
            color: Theme.danger
            font.pointSize: Theme.fontSmall
            wrapMode: Text.Wrap
        }
    }

    Rectangle {
        visible: !row.last
        x: row.horizontalPadding
        width: row.width - 2 * row.horizontalPadding
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.divider
    }
}
