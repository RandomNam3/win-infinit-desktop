// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Titles, explanations and colors for the states of EffectStatus.
pragma Singleton

import QtQuick
import InfiniteDesktop.Settings

QtObject {
    function title(state) {
        switch (state) {
        case "active":
            return i18nc("@label plugin state", "Active");
        case "paused":
            return i18nc("@label plugin state", "Paused");
        case "disabled":
            return i18nc("@label plugin state", "Turned off");
        case "safemode":
            return i18nc("@label plugin state", "Safe mode");
        case "notinstalled":
            return i18nc("@label plugin state", "Not installed");
        default:
            return i18nc("@label plugin state", "Checking…");
        }
    }

    function description(state) {
        switch (state) {
        case "active":
            return i18n("Every screen is an infinite canvas: pan and zoom freely and place windows anywhere. Changes are applied immediately.");
        case "paused":
            return i18n("Mouse and touchpad gestures of the canvas are paused. Windows stay where they are and the desktop behaves normally.");
        case "disabled":
            return i18n("The canvas service is installed but not running. Turn it on to make every screen an infinite canvas; it then also starts when you sign in.");
        case "safemode":
            return i18n("WIN Infinite Desktop crashed several times in a row, so it was started in safe mode and leaves the windows alone. Leave safe mode to use the canvas again.");
        case "notinstalled":
            return i18n("The canvas service win-infinite-desktop.exe is not next to this app. Install WIN Infinite Desktop again.");
        default:
            return i18n("Asking the canvas service…");
        }
    }

    function color(state) {
        switch (state) {
        case "active":
            return Theme.success;
        case "paused":
        case "notinstalled":
            return Theme.warning;
        case "safemode":
            return Theme.danger;
        default:
            return Theme.textTertiary;
        }
    }

    function isProblem(state) {
        return state === "safemode" || state === "notinstalled";
    }
}
