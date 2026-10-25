// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One note: a small frameless window with Markdown text. Opens with the Markdown source to
// edit; the pencil in the header switches to the formatted text and back (kept per note).
// The canvas places the window; as a tool window it is not in the task bar or Alt+Tab.
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
    readonly property bool editing: !note.formatted
    // Menus are native (like the canvas menu) and follow the system's light or dark mode;
    // tool tips and the fallback menus use these colors.
    readonly property bool darkMode: Qt.styleHints.colorScheme === Qt.ColorScheme.Dark

    function setEditing(on) {
        window.note.formatted = !on;
        if (on) {
            editor.forceActiveFocus();
        }
    }

    title: (note.title.length > 0 ? note.title : i18nc("@title:window untitled note", "Note")) + " — " + i18nc("@title:window", "Note")
    flags: Qt.Tool | Qt.FramelessWindowHint
    color: paper
    width: note.width
    height: note.height
    minimumWidth: 180
    minimumHeight: 120

    palette.window: darkMode ? "#2c2c2c" : "#f9f9f9"
    palette.windowText: darkMode ? "#ffffff" : "#1a1a1a"
    palette.light: darkMode ? "#3d3d3d" : "#ececec"
    palette.midlight: darkMode ? "#474747" : "#e0e0e0"
    palette.mid: darkMode ? "#4a4a4a" : "#dcdcdc"
    palette.dark: darkMode ? "#4a4a4a" : "#d6d6d6"
    palette.toolTipBase: darkMode ? "#2c2c2c" : "#f9f9f9"
    palette.toolTipText: darkMode ? "#ffffff" : "#1a1a1a"

    onWidthChanged: sizeTimer.restart()
    onHeightChanged: sizeTimer.restart()
    Component.onCompleted: {
        if (window.editing) {
            editor.forceActiveFocus();
        }
    }

    // Store the size once resizing has settled.
    Timer {
        id: sizeTimer

        interval: 400
        onTriggered: {
            window.note.width = window.width;
            window.note.height = window.height;
        }
    }

    Shortcut {
        sequence: "Ctrl+E"
        onActivated: window.setEditing(!window.editing)
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
                glyph: "✎"
                checked: window.editing
                tip: window.editing ? i18nc("@info:tooltip", "Show formatted (Ctrl+E)") : i18nc("@info:tooltip", "Edit the Markdown text (Ctrl+E)")
                onClicked: window.setEditing(!window.editing)
            }
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
            persistentSelection: true
            color: window.ink
            background: null
            padding: 10
            onTextChanged: {
                if (text !== window.note.text) {
                    window.note.text = text;
                }
            }

            // The hint wraps (TextArea's own placeholder stays on one line).
            Text {
                x: editor.leftPadding
                y: editor.topPadding
                width: editor.width - editor.leftPadding - editor.rightPadding
                visible: editor.length === 0 && editor.preeditText.length === 0
                text: i18nc("@info:placeholder", "Write a note… Markdown works: # heading, **bold**, - list, [link](https://…)")
                wrapMode: Text.Wrap
                color: Qt.rgba(0, 0, 0, 0.4)
                font: editor.font
            }
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: textMenu.popup()
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
                cursorShape: parent.hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
            TapHandler {
                onDoubleTapped: {
                    if (parent.hoveredLink.length === 0) {
                        window.setEditing(true);
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

    // ---- Actions and menus -----------------------------------------------------
    // The header menu and the right-click menu of the text share these actions; the text
    // menu adds the editing commands. Checkable actions bind their state again after a
    // click, which would otherwise replace the binding with a fixed value.
    Action {
        id: editAction

        text: i18nc("@action note menu", "Edit")
        checkable: true
        checked: window.editing
        onTriggered: {
            window.setEditing(!window.editing);
            checked = Qt.binding(() => window.editing);
        }
    }
    Action {
        id: newNoteAction

        text: i18nc("@action note menu", "New Note")
        onTriggered: window.companion.newNote()
    }
    Action {
        id: pinAction

        text: window.note.pinned ? i18nc("@action note menu", "Release from the Screen") : i18nc("@action note menu", "Pin to the Screen")
        onTriggered: window.companion.togglePin(window.note.id, false)
    }
    Action {
        id: keepAboveAction

        text: i18nc("@action note menu", "Pinned and Always on Top")
        checkable: true
        checked: window.note.keepAbove
        onTriggered: {
            window.companion.togglePin(window.note.id, true);
            checked = Qt.binding(() => window.note.keepAbove);
        }
    }
    Action {
        id: colorAction

        text: i18nc("@action note menu", "Color…")
        // A menu of its own instead of a submenu: Qt 6.8 sometimes leaves native submenus
        // out. Opened after the first menu has closed.
        onTriggered: Qt.callLater(() => colorMenu.popup())
    }
    Action {
        id: hideAction

        text: i18nc("@action note menu", "Hide This Note")
        onTriggered: window.companion.hideNote(window.note.id)
    }
    Action {
        id: hideAllAction

        text: i18nc("@action note menu", "Hide All Notes")
        onTriggered: window.companion.setNotesVisible(false)
    }
    Action {
        id: mapAction

        text: i18nc("@action note menu", "Show Map")
        onTriggered: window.companion.showMap()
    }
    Action {
        id: deleteAction

        text: i18nc("@action note menu", "Delete Note…")
        onTriggered: confirmDelete.open()
    }

    NoteMenu {
        id: noteMenu

        MenuItem {
            action: editAction
        }
        MenuItem {
            action: newNoteAction
        }
        MenuSeparator {}
        MenuItem {
            action: pinAction
        }
        MenuItem {
            action: keepAboveAction
        }
        MenuItem {
            action: colorAction
        }
        MenuSeparator {}
        MenuItem {
            action: hideAction
        }
        MenuItem {
            action: hideAllAction
        }
        MenuItem {
            action: mapAction
        }
        MenuSeparator {}
        MenuItem {
            action: deleteAction
        }
    }

    NoteMenu {
        id: textMenu

        MenuItem {
            text: i18nc("@action text", "Undo")
            enabled: editor.canUndo
            onTriggered: editor.undo()
        }
        MenuItem {
            text: i18nc("@action text", "Redo")
            enabled: editor.canRedo
            onTriggered: editor.redo()
        }
        MenuSeparator {}
        MenuItem {
            text: i18nc("@action text", "Cut")
            enabled: editor.selectedText.length > 0
            onTriggered: editor.cut()
        }
        MenuItem {
            text: i18nc("@action text", "Copy")
            enabled: editor.selectedText.length > 0
            onTriggered: editor.copy()
        }
        MenuItem {
            text: i18nc("@action text", "Paste")
            enabled: editor.canPaste
            onTriggered: editor.paste()
        }
        MenuItem {
            text: i18nc("@action text", "Select All")
            enabled: editor.length > 0
            onTriggered: editor.selectAll()
        }
        MenuSeparator {}
        MenuItem {
            action: editAction
        }
        MenuItem {
            action: newNoteAction
        }
        MenuSeparator {}
        MenuItem {
            action: pinAction
        }
        MenuItem {
            action: keepAboveAction
        }
        MenuItem {
            action: colorAction
        }
        MenuSeparator {}
        MenuItem {
            action: hideAction
        }
        MenuItem {
            action: hideAllAction
        }
        MenuItem {
            action: mapAction
        }
        MenuSeparator {}
        MenuItem {
            action: deleteAction
        }
    }

    NoteMenu {
        id: colorMenu

        ColorItem {
            note: window.note
            companion: window.companion
            colorIndex: 0
        }
        ColorItem {
            note: window.note
            companion: window.companion
            colorIndex: 1
        }
        ColorItem {
            note: window.note
            companion: window.companion
            colorIndex: 2
        }
        ColorItem {
            note: window.note
            companion: window.companion
            colorIndex: 3
        }
        ColorItem {
            note: window.note
            companion: window.companion
            colorIndex: 4
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

                PaperButton {
                    text: i18nc("@action:button", "Delete")
                    paper: window.paper
                    danger: true
                    onClicked: {
                        confirmDelete.close();
                        window.companion.deleteNote(window.note.id);
                    }
                }
                PaperButton {
                    text: i18nc("@action:button", "Cancel")
                    paper: window.paper
                    onClicked: confirmDelete.close()
                }
            }
        }
    }

    // A native menu where the platform has one (Windows, and Plasma with the widgets
    // menu), otherwise its own window in the colors above.
    component NoteMenu: Menu {
        popupType: Popup.Native
    }

    component ColorItem: MenuItem {
        id: colorItem

        required property QtObject note
        required property QtObject companion
        required property int colorIndex

        text: companion.colorName(colorIndex)
        checkable: true
        checked: note.color === colorIndex
        onTriggered: {
            colorItem.note.color = colorItem.colorIndex;
            checked = Qt.binding(() => colorItem.note.color === colorItem.colorIndex);
        }
    }

    component HeaderButton: AbstractButton {
        id: button

        property string glyph
        property string tip

        width: 26
        height: 24
        hoverEnabled: true

        // A window of its own, so it may be wider than the note; below the button and ending at
        // its right edge, so it never comes under the pointer (that would end the hover).
        ToolTip {
            parent: button
            x: button.width - width
            y: button.height + 4
            visible: button.hovered && button.tip.length > 0
            text: button.tip
            delay: 600
            popupType: Popup.Window
        }

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

    component PaperButton: AbstractButton {
        id: paperButton

        property color paper
        property bool danger

        implicitWidth: Math.max(72, label.implicitWidth + 24)
        implicitHeight: 28
        hoverEnabled: true

        background: Rectangle {
            radius: 4
            color: paperButton.danger ? (paperButton.down ? "#a8322b" : paperButton.hovered ? "#bf3a32" : "#cc3a31")
                                      : Qt.darker(paperButton.paper, paperButton.down ? 1.22 : paperButton.hovered ? 1.14 : 1.08)
            border.color: paperButton.danger ? "transparent" : Qt.darker(paperButton.paper, 1.3)
        }
        contentItem: Text {
            id: label

            text: paperButton.text
            color: paperButton.danger ? "#ffffff" : "#1f1f1f"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
