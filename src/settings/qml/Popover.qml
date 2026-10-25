// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Floating card anchored to its parent item. Opens below the parent, or above
// when there is not enough room, and is kept inside the window.
import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Popup {
    id: popover

    // Horizontal alignment relative to the parent: "start", "center" or "end".
    property string alignment: "start"
    property real gap: 6

    function place() {
        if (!parent) {
            return;
        }
        const overlayHeight = Basic.Overlay.overlay ? Basic.Overlay.overlay.height : 10000;
        const below = parent.mapToItem(null, 0, parent.height).y;
        const w = width > 0 ? width : implicitWidth;
        const h = implicitHeight;
        const fitsBelow = below + gap + h + 12 <= overlayHeight;
        y = fitsBelow ? parent.height + gap : -h - gap;
        transformOrigin = fitsBelow ? Basic.Popup.Top : Basic.Popup.Bottom;
        if (alignment === "center") {
            x = (parent.width - w) / 2;
        } else if (alignment === "end") {
            x = parent.width - w;
        } else {
            x = -8;
        }
    }

    padding: 16
    margins: 12
    focus: true
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutsideParent

    onAboutToShow: place()

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.surfaceRaised
        border.width: 1
        border.color: Theme.borderStrong

        Shadow {
            radius: parent.radius
            elevation: 2
        }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationFast }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.durationNormal; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durationFast }
    }
}
