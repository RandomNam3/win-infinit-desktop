// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Search field: Ctrl+F focuses it (see Main.qml), Esc clears it.
import QtQuick

FocusScope {
    id: root

    property alias text: input.text
    property string shortcutHint: ""

    signal accepted()

    function focusField() {
        input.forceActiveFocus(Qt.ShortcutFocusReason);
        input.selectAll();
    }

    implicitWidth: 240
    implicitHeight: 38

    Rectangle {
        id: frame

        anchors.fill: parent
        radius: Theme.radiusMedium
        color: input.activeFocus ? Theme.input : hoverHandler.hovered ? Theme.controlHover : Theme.control
        border.width: input.activeFocus ? 1.5 : 0
        border.color: Theme.accentText

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }

        HoverHandler {
            id: hoverHandler
            cursorShape: Qt.IBeamCursor
        }

        TapHandler {
            onTapped: root.focusField()
        }

        Icon {
            id: searchIcon
            x: 11
            anchors.verticalCenter: parent.verticalCenter
            name: "search"
            size: 16
            color: input.activeFocus ? Theme.accentText : Theme.textTertiary
        }

        TextInput {
            id: input

            anchors.left: searchIcon.right
            anchors.leftMargin: 8
            anchors.right: trailing.left
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.text
            selectionColor: Theme.accentSoft
            selectedTextColor: Theme.text
            font.pointSize: Theme.fontBody
            selectByMouse: true
            clip: true
            focus: true
            renderType: Text.NativeRendering

            Accessible.role: Accessible.EditableText
            Accessible.name: i18nc("@label accessible name of the search field", "Search settings")
            Accessible.searchEdit: true

            Keys.onEscapePressed: event => {
                if (text.length > 0) {
                    text = "";
                } else {
                    focus = false;
                    event.accepted = false;
                }
            }
            onAccepted: root.accepted()

            UiText {
                anchors.verticalCenter: parent.verticalCenter
                visible: input.text.length === 0 && input.preeditText.length === 0
                text: i18nc("@info:placeholder", "Search settings…")
                color: Theme.textTertiary
                font: input.font
                elide: Text.ElideRight
                width: parent.width
            }
        }

        Item {
            id: trailing

            anchors.right: parent.right
            anchors.rightMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            width: input.text.length > 0 ? clearButton.width : (hint.visible ? hint.width + 4 : 0)
            height: parent.height

            IconButton {
                id: clearButton

                anchors.verticalCenter: parent.verticalCenter
                visible: input.text.length > 0
                implicitWidth: 26
                implicitHeight: 26
                iconName: "close"
                iconSize: 14
                tooltip: i18nc("@action:button", "Clear search")
                focusPolicy: Qt.NoFocus
                onClicked: {
                    input.text = "";
                    root.focusField();
                }
            }

            Keycap {
                id: hint

                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 4
                visible: input.text.length === 0 && !input.activeFocus && root.shortcutHint.length > 0
                text: root.shortcutHint
                small: true
            }
        }
    }
}
