// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Scrollable page with a centered content column of limited width. Keeps the
// item with keyboard focus visible.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Flickable {
    id: page

    default property alias content: column.data
    property real maxContentWidth: Theme.contentMaxWidth
    property real sideMargin: width < 600 ? 16 : 32
    property bool isSearchPage: false
    property real topPadding: 28

    function isInside(item) {
        for (let p = item; p; p = p.parent) {
            if (p === column) {
                return true;
            }
        }
        return false;
    }

    function ensureVisible(item) {
        if (!item || !isInside(item)) {
            return;
        }
        const top = item.mapToItem(page.contentItem, 0, 0).y;
        const bottom = top + item.height;
        const margin = 24;
        if (top - margin < contentY) {
            contentY = Math.max(0, top - margin);
        } else if (bottom + margin > contentY + height) {
            contentY = Math.max(0, Math.min(contentHeight - height, bottom + margin - height));
        }
    }

    contentWidth: width
    contentHeight: column.implicitHeight + topPadding + 48
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick
    pixelAligned: true

    Keys.onPressed: event => {
        if (event.key === Qt.Key_PageDown) {
            contentY = Math.min(Math.max(0, contentHeight - height), contentY + height * 0.85);
            event.accepted = true;
        } else if (event.key === Qt.Key_PageUp) {
            contentY = Math.max(0, contentY - height * 0.85);
            event.accepted = true;
        }
    }

    Connections {
        target: page.Window.window
        function onActiveFocusItemChanged() {
            page.ensureVisible(page.Window.window ? page.Window.window.activeFocusItem : null);
        }
    }

    ColumnLayout {
        id: column

        x: Math.round((page.width - width) / 2)
        y: page.topPadding
        width: Math.min(page.width - 2 * page.sideMargin, page.maxContentWidth)
        spacing: 16
    }

    Basic.ScrollBar.vertical: UiScrollBar {}
}
