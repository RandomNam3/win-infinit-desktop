// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shows a shortcut as key caps. Click (or Enter/Space) to record a new one:
// press the key combination, Esc cancels, Backspace removes the shortcut.
import QtQuick
import QtQuick.Controls.Basic as Basic
import InfiniteDesktop.Settings

Basic.AbstractButton {
    id: control

    property var keys: []
    property string shortcutText
    property string actionTitle
    property bool recording: false
    property var pendingKeys: []
    property string message

    signal captured(string sequence)
    signal cleared()

    function startRecording() {
        pendingKeys = [];
        message = "";
        recording = true;
        forceActiveFocus(Qt.MouseFocusReason);
        Shortcuts.capturing = true;
    }

    function stopRecording() {
        if (!recording) {
            return;
        }
        recording = false;
        pendingKeys = [];
        message = "";
        Shortcuts.capturing = false;
    }

    function modifierFlag(key) {
        switch (key) {
        case Qt.Key_Shift:
            return Qt.ShiftModifier;
        case Qt.Key_Control:
            return Qt.ControlModifier;
        case Qt.Key_Alt:
            return Qt.AltModifier;
        case Qt.Key_Meta:
        case Qt.Key_Super_L:
        case Qt.Key_Super_R:
            return Qt.MetaModifier;
        }
        return 0;
    }

    implicitWidth: Math.max(128, content.implicitWidth + 24)
    implicitHeight: Theme.controlHeight
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.Button
    Accessible.name: recording ? i18nc("@info accessible name while recording a shortcut", "Recording a shortcut for “%1”. Press the new key combination, Escape to cancel, Backspace to remove it.", actionTitle)
                               : keys.length > 0 ? i18nc("@action:button %1 is a key combination, %2 the action", "%1, shortcut for “%2”. Press to change it.", shortcutText, actionTitle)
                                                 : i18nc("@action:button %1 is the action", "No shortcut for “%1”. Press to assign one.", actionTitle)

    onClicked: recording ? stopRecording() : startRecording()
    onActiveFocusChanged: {
        if (!activeFocus) {
            stopRecording();
        }
    }
    Component.onDestruction: stopRecording()

    Keys.onShortcutOverride: event => {
        // Keep application shortcuts (Ctrl+F, …) from firing while recording.
        event.accepted = control.recording;
    }
    Keys.onPressed: event => {
        if (!control.recording) {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                control.startRecording();
                event.accepted = true;
            }
            return;
        }
        event.accepted = true;
        if (event.isAutoRepeat) {
            return;
        }
        const result = Shortcuts.interpretKey(event.key, event.modifiers);
        switch (result.action) {
        case "modifiers":
            control.pendingKeys = result.keys;
            control.message = "";
            break;
        case "cancel":
            control.stopRecording();
            break;
        case "clear":
            control.stopRecording();
            control.cleared();
            break;
        case "needsModifier":
            control.pendingKeys = result.keys;
            control.message = i18nc("@info while recording a shortcut", "Add Win, Ctrl or Alt");
            break;
        case "accept":
            control.stopRecording();
            control.captured(result.sequence);
            break;
        }
    }
    Keys.onReleased: event => {
        if (!control.recording) {
            return;
        }
        event.accepted = true;
        const remaining = event.modifiers & ~control.modifierFlag(event.key);
        const result = Shortcuts.interpretKey(0, remaining);
        control.pendingKeys = result.keys;
    }

    contentItem: Item {
        implicitWidth: content.implicitWidth
        implicitHeight: content.implicitHeight

        Row {
            id: content

            anchors.centerIn: parent
            spacing: 8

            Keycaps {
                anchors.verticalCenter: parent.verticalCenter
                visible: !control.recording && control.keys.length > 0
                keys: control.keys
                small: true
            }
            UiText {
                anchors.verticalCenter: parent.verticalCenter
                visible: !control.recording && control.keys.length === 0
                text: i18nc("@label no shortcut assigned", "Not assigned")
                color: Theme.textTertiary
                font.pointSize: Theme.fontSmall
            }

            // Recording state
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.recording
                width: 8
                height: 8
                radius: 4
                color: Theme.danger

                SequentialAnimation on opacity {
                    running: control.recording && Theme.animations
                    loops: Animation.Infinite
                    NumberAnimation { from: 1; to: 0.3; duration: 600 }
                    NumberAnimation { from: 0.3; to: 1; duration: 600 }
                }
            }
            Keycaps {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.recording && control.pendingKeys.length > 0
                keys: control.pendingKeys
                small: true
                highlighted: true
            }
            UiText {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.recording && (control.pendingKeys.length === 0 || control.message.length > 0)
                text: control.message.length > 0 ? control.message : i18nc("@info while recording a shortcut", "Press a key combination…")
                color: Theme.accentText
                font.pointSize: Theme.fontSmall
                font.weight: Font.Medium
            }
        }
    }

    background: Rectangle {
        radius: Theme.radiusMedium
        color: control.recording ? Theme.accentSofter : control.down ? Theme.controlPressed : control.hovered ? Theme.controlHover : Theme.control
        border.width: control.recording ? 1.5 : 0
        border.color: Theme.accentText

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }

        FocusRing {
            radius: parent.radius + offset
            visible: control.visualFocus && !control.recording
        }
    }
}
