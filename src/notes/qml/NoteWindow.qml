// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One note: a small frameless window with Markdown text. Shows the rendered text, and the
// Markdown source while it is being edited. The canvas places the window; as a tool window
// it is not in the task bar or Alt+Tab.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window

Window {
    id: window

    required property QtObject note
    required property QtObject companion

    readonly property color paper: companion.paperColor(note.color)
    readonly property color ink: "#1f1f1f"
    property bool editing: note.text.length === 0

    title: (note.title.length > 0 ? note.title : i18nc("@title:window untitled note", "Note")) + " — " + i18nc("@title:window", "Note")
    flags: Qt.Tool | Qt.FramelessWindowHint
    color: paper
    width: note.width
    height: note.height
    minimumWidth: 180
    minimumHeight: 120

    onWidthChanged: sizeTimer.restart()
    onHeightChanged: sizeTimer.restart()

    // Store the size once resizing has settled.
    Timer {
        id: sizeTimer

        interval: 400
        onTriggered: {
            window.note.width = window.width;
            window.note.height = window.height;
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: Qt.darker(window.paper, 1.25)
        border.width: 1
        z: 10
    }

    // ---- Header: drag to move, buttons ------------------------------------------
    Rectangle {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 30
        color: Qt.darker(window.paper, 1.06)

        DragHandler {
            target: null
            grabPermissions: PointerHandler.TakeOverForbidden
            onActiveChanged: {
                if (active) {
                    window.startSystemMove();
                }
            }
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: noteMenu.popup()
        }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onDoubleTapped: window.editing = true
        }

        Text {
            anchors.left: parent.left
            anchors.right: buttons.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 10
            anchors.rightMargin: 6
            text: window.note.title.length > 0 ? window.note.title : i18nc("@title untitled note", "Note")
            color: window.ink
            opacity: 0.75
            elide: Text.ElideRight
            font.bold: true
        }

        Row {
            id: buttons

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 4
            spacing: 2

            HeaderButton {
                glyph: window.note.pinned ? "◉" : "○"
                checked: window.note.pinned
                tip: window.note.pinned ? i18nc("@info:tooltip", "Release from the screen") : i18nc("@info:tooltip", "Pin to the screen: stays visible while the canvas moves")
                onClicked: window.companion.togglePin(window.note.id, false)
            }
            HeaderButton {
                glyph: "⋯"
                tip: i18nc("@info:tooltip", "More")
                onClicked: noteMenu.popup()
            }
            HeaderButton {
                glyph: "✕"
                tip: i18nc("@info:tooltip", "Hide this note (it is kept)")
                onClicked: window.companion.hideNote(window.note.id)
            }
        }
    }

    // ---- Text ------------------------------------------------------------------
    ScrollView {
        id: scroll

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 1
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        TextArea {
            id: editor

            visible: window.editing
            width: scroll.availableWidth
            text: window.note.text
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            selectByMouse: true
            color: window.ink
            placeholderText: i18nc("@info:placeholder", "Write a note… Markdown works: # heading, **bold**, - list, [link](https://…)")
            placeholderTextColor: Qt.rgba(0, 0, 0, 0.4)
            background: null
            padding: 10
            onTextChanged: {
                if (text !== window.note.text) {
                    window.note.text = text;
                }
            }
            onActiveFocusChanged: {
                if (!activeFocus && text.length > 0) {
                    window.editing = false;
                }
            }
            Keys.onEscapePressed: {
                if (text.length > 0) {
                    window.editing = false;
                }
            }
        }

        Text {
            visible: !window.editing
            width: scroll.availableWidth
            padding: 10
            text: window.note.text
            textFormat: Text.MarkdownText
            wrapMode: Text.Wrap
            color: window.ink
            linkColor: "#1d64b8"
            onLinkActivated: link => Qt.openUrlExternally(link)

            HoverHandler {
                cursorShape: parent.hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.IBeamCursor
            }
            TapHandler {
                onTapped: {
                    if (parent.hoveredLink.length === 0) {
                        window.editing = true;
                        editor.forceActiveFocus();
                    }
                }
            }
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: noteMenu.popup()
            }
        }
    }

    // ---- Resize grip -----------------------------------------------------------
    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 2
        text: "◢"
        color: window.ink
        opacity: 0.25

        MouseArea {
            anchors.fill: parent
            anchors.margins: -6
            cursorShape: Qt.SizeFDiagCursor
            onPressed: window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
        }
    }

    // ---- Menu ------------------------------------------------------------------
    Menu {
        id: noteMenu

        MenuItem {
            text: i18nc("@action note menu", "New Note")
            onTriggered: window.companion.newNote()
        }
        MenuItem {
            text: window.editing ? i18nc("@action note menu", "Show Formatted") : i18nc("@action note menu", "Edit")
            onTriggered: {
                window.editing = !window.editing;
                if (window.editing) {
                    editor.forceActiveFocus();
                }
            }
        }
        MenuSeparator {}
        MenuItem {
            text: window.note.pinned ? i18nc("@action note menu", "Release from the Screen") : i18nc("@action note menu", "Pin to the Screen")
            onTriggered: window.companion.togglePin(window.note.id, false)
        }
        MenuItem {
            checkable: true
            checked: window.note.keepAbove
            text: i18nc("@action note menu", "Pinned and Always on Top")
            onTriggered: window.companion.togglePin(window.note.id, true)
        }
        Menu {
            title: i18nc("@action note menu", "Color")

            Repeater {
                model: 5

                MenuItem {
                    required property int index

                    text: window.companion.colorName(index)
                    checkable: true
                    checked: window.note.color === index
                    onTriggered: window.note.color = index
                }
            }
        }
        MenuSeparator {}
        MenuItem {
            text: i18nc("@action note menu", "Hide This Note")
            onTriggered: window.companion.hideNote(window.note.id)
        }
        MenuItem {
            text: i18nc("@action note menu", "Hide All Notes")
            onTriggered: window.companion.setNotesVisible(false)
        }
        MenuItem {
            text: i18nc("@action note menu", "Show Map")
            onTriggered: window.companion.showMap()
        }
        MenuSeparator {}
        MenuItem {
            text: i18nc("@action note menu", "Delete Note…")
            onTriggered: confirmDelete.open()
        }
    }

    Popup {
        id: confirmDelete

        anchors.centerIn: parent
        modal: true
        padding: 12
        background: Rectangle {
            color: Qt.lighter(window.paper, 1.04)
            border.color: Qt.darker(window.paper, 1.3)
            radius: 6
        }

        Column {
            spacing: 10

            Text {
                width: Math.min(window.width - 60, 240)
                text: i18nc("@info", "Delete this note? This cannot be undone.")
                wrapMode: Text.Wrap
                color: window.ink
            }
            Row {
                spacing: 8

                Button {
                    text: i18nc("@action:button", "Delete")
                    onClicked: {
                        confirmDelete.close();
                        window.companion.deleteNote(window.note.id);
                    }
                }
                Button {
                    text: i18nc("@action:button", "Cancel")
                    onClicked: confirmDelete.close()
                }
            }
        }
    }

    component HeaderButton: AbstractButton {
        id: button

        property string glyph
        property string tip

        width: 26
        height: 24
        hoverEnabled: true
        ToolTip.visible: hovered && tip.length > 0
        ToolTip.text: tip
        ToolTip.delay: 600

        background: Rectangle {
            radius: 4
            color: button.down ? Qt.rgba(0, 0, 0, 0.16) : button.hovered || button.checked ? Qt.rgba(0, 0, 0, 0.08) : "transparent"
        }
        contentItem: Text {
            text: button.glyph
            color: "#1f1f1f"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 14
        }
    }
}
