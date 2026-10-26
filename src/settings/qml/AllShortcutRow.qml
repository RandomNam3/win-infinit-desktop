// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One shortcut of WIN Infinite Desktop or of Windows (GlobalShortcutsFilter delegate).
// Rows of one component are drawn as one continuous card. Only our own can be changed.
import QtQuick
import QtQuick.Layouts
import InfiniteDesktop.Settings

Item {
    id: row

    required property int index
    required property string shortcutKey
    required property string componentName
    required property string actionName
    required property string highlightedActionName
    required property var keys
    required property var sequenceTexts
    required property bool isDefault
    required property string defaultText
    required property bool hasConflict
    required property string conflictText
    required property bool isOwn
    required property bool editable
    required property bool firstInComponent
    required property bool lastInComponent

    property bool searchMode: false
    // Inside a Card (search results): no own card segment, divider between all rows.
    property bool inCard: false
    property bool lastRow: false
    // Horizontal space left and right of the row (ListView places delegates at x = 0).
    property real inset: 0
    property var pending: null
    property string error

    readonly property real horizontalPadding: 18
    readonly property bool stacked: width - 2 * inset < 560

    function tryAssign(slot, sequence) {
        error = "";
        const conflicts = GlobalShortcuts.conflicts(shortcutKey, sequence);
        if (conflicts.length > 0) {
            pending = { "slot": slot, "sequence": sequence, "keys": Shortcuts.keysFor(sequence), "conflicts": conflicts };
            return;
        }
        pending = null;
        if (!GlobalShortcuts.assign(shortcutKey, slot, sequence, false)) {
            error = i18n("The shortcut could not be assigned. It may be reserved by the system.");
        }
    }

    implicitHeight: layout.implicitHeight + 24
    Accessible.role: Accessible.Grouping
    Accessible.name: actionName + ", " + componentName

    Item {
        id: body

        x: row.inset
        width: row.width - 2 * row.inset
        height: row.height

        // Card segment: rounded only at the top of the first and the bottom of the last row.
        Rectangle {
            anchors.fill: parent
            visible: !row.inCard
            radius: row.firstInComponent || row.lastInComponent ? Theme.radiusLarge : 0
            color: Theme.surface

            Rectangle {
                visible: row.firstInComponent && !row.lastInComponent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Theme.radiusLarge
                color: parent.color
            }
            Rectangle {
                visible: row.lastInComponent && !row.firstInComponent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Theme.radiusLarge
                color: parent.color
            }
        }

        HoverHandler {
            id: hoverHandler
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 4
            radius: Theme.radiusMedium
            color: row.hasConflict ? Theme.alpha(Theme.warning, hoverHandler.hovered ? 0.1 : 0.06)
                                   : hoverHandler.hovered ? Theme.hover : Theme.alpha(Theme.hover, 0)

            Behavior on color {
                ColorAnimation { duration: Theme.durationFast }
            }
        }

        ColumnLayout {
            id: layout

            x: row.horizontalPadding
            width: body.width - 2 * row.horizontalPadding
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            GridLayout {
                Layout.fillWidth: true
                columns: row.stacked ? 1 : 2
                columnSpacing: 20
                rowSpacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            id: ownBadge

                            visible: row.isOwn
                            Layout.preferredWidth: badgeText.implicitWidth + 14
                            Layout.preferredHeight: 20
                            radius: 10
                            color: Theme.accentSoft

                            UiText {
                                id: badgeText
                                anchors.centerIn: parent
                                text: i18nc("@label badge for actions of this plugin", "Infinite Desktop")
                                color: Theme.accentText
                                font.pointSize: Theme.fontCaption
                                font.weight: Font.DemiBold
                            }
                        }
                        UiText {
                            Layout.fillWidth: true
                            text: row.searchMode ? row.highlightedActionName : row.actionName
                            textFormat: row.searchMode ? Text.StyledText : Text.PlainText
                            font.weight: Font.Medium
                            wrapMode: Text.Wrap
                        }
                    }
                    UiText {
                        Layout.fillWidth: true
                        visible: row.searchMode
                        text: row.componentName
                        color: Theme.textSecondary
                        font.pointSize: Theme.fontSmall
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: row.hasConflict
                        spacing: 6

                        Icon {
                            Layout.alignment: Qt.AlignTop
                            name: "warning"
                            size: 14
                            color: Theme.warning
                        }
                        UiText {
                            Layout.fillWidth: true
                            text: row.conflictText
                            color: Theme.warning
                            font.pointSize: Theme.fontSmall
                            wrapMode: Text.Wrap
                        }
                    }
                }

                RowLayout {
                    Layout.alignment: row.stacked ? Qt.AlignLeft : Qt.AlignRight | Qt.AlignVCenter
                    layoutDirection: row.stacked ? Qt.RightToLeft : Qt.LeftToRight
                    spacing: 6

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
                            GlobalShortcuts.resetToDefault(row.shortcutKey);
                        }

                        Behavior on opacity {
                            NumberAnimation { duration: Theme.durationNormal }
                        }
                    }

                    Repeater {
                        model: Math.max(1, row.keys.length)

                        KeyCapture {
                            required property int index

                            enabled: row.editable
                            keys: index < row.keys.length ? row.keys[index] : []
                            shortcutText: index < row.sequenceTexts.length ? row.sequenceTexts[index] : ""
                            actionTitle: row.actionName
                            onCaptured: sequence => row.tryAssign(index, sequence)
                            onCleared: {
                                row.pending = null;
                                GlobalShortcuts.clear(row.shortcutKey, index);
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
                    if (!GlobalShortcuts.assign(row.shortcutKey, p.slot, p.sequence, true)) {
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
            visible: row.inCard ? !row.lastRow : !row.lastInComponent
            x: row.horizontalPadding
            width: body.width - 2 * row.horizontalPadding
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.divider
        }
    }
}
