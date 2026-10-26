// SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Design tokens of the settings app: colors, spacing, radii, typography, motion.
pragma Singleton

import QtQuick
import InfiniteDesktop.Settings

QtObject {
    id: theme

    readonly property bool dark: SystemTheme.dark
    readonly property color accent: SystemTheme.accent
    readonly property bool animations: SystemTheme.animations

    // --- Neutrals -----------------------------------------------------------
    readonly property color background: dark ? "#131519" : "#f3f4f7"
    readonly property color sidebar: dark ? "#17191d" : "#eaecf0"
    readonly property color surface: dark ? "#1e2126" : "#ffffff"
    readonly property color surfaceRaised: dark ? "#262a30" : "#ffffff"
    readonly property color surfaceSunken: dark ? "#16181b" : "#f4f5f8"
    readonly property color border: dark ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0.07, 0.09, 0.15, 0.08)
    readonly property color borderStrong: dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0.07, 0.09, 0.15, 0.14)
    readonly property color divider: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0.07, 0.09, 0.15, 0.065)

    readonly property color text: dark ? "#eceef2" : "#15171c"
    readonly property color textSecondary: dark ? "#a3a8b1" : "#5b616c"
    readonly property color textTertiary: dark ? "#787e88" : "#878d98"

    readonly property color control: dark ? "#2a2e35" : "#edeff3"
    readonly property color controlHover: dark ? "#323740" : "#e4e7ec"
    readonly property color controlPressed: dark ? "#3a404a" : "#d9dde4"
    readonly property color track: dark ? "#3c414b" : "#d2d6dd"
    readonly property color input: dark ? "#15171a" : "#ffffff"
    readonly property color hover: dark ? Qt.rgba(1, 1, 1, 0.045) : Qt.rgba(0.07, 0.09, 0.15, 0.04)
    readonly property color pressed: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0.07, 0.09, 0.15, 0.075)
    readonly property color shadow: dark ? Qt.rgba(0, 0, 0, 0.55) : Qt.rgba(0.07, 0.09, 0.2, 0.11)
    readonly property color scrim: dark ? Qt.rgba(0, 0, 0, 0.55) : Qt.rgba(0.07, 0.08, 0.12, 0.34)

    // Inverted surfaces (toasts, value bubbles)
    readonly property color inverse: dark ? "#eef0f3" : "#1b1e23"
    readonly property color onInverse: dark ? "#15171c" : "#ffffff"

    // --- Status -------------------------------------------------------------
    readonly property color success: dark ? "#4fcf8f" : "#14834f"
    readonly property color warning: dark ? "#f1b74c" : "#9a6200"
    readonly property color danger: dark ? "#ff7d74" : "#cc3a31"
    readonly property color dangerFill: dark ? "#e5534b" : "#d43d33"

    // --- Accent -------------------------------------------------------------
    function luminance(c) {
        function channel(v) {
            return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
        }
        return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
    }
    function contrast(a, b) {
        const la = luminance(a);
        const lb = luminance(b);
        return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05);
    }
    function withLightness(c, l) {
        return Qt.hsla(Math.max(0, c.hslHue), c.hslSaturation, Math.max(0, Math.min(1, l)), 1);
    }
    // Moves the lightness of c away from bg until the contrast target is met.
    function readable(c, bg, target) {
        let result = c;
        let l = c.hslLightness;
        const lighten = luminance(bg) < 0.5;
        for (let i = 0; i < 24 && contrast(result, bg) < target; ++i) {
            l += lighten ? 0.025 : -0.025;
            result = withLightness(c, l);
        }
        return result;
    }
    function alpha(c, a) {
        return Qt.rgba(c.r, c.g, c.b, a);
    }

    readonly property color accentText: readable(accent, surface, 4.5)
    readonly property color accentFill: dark ? readable(accent, background, 3.0) : readable(accent, Qt.rgba(1, 1, 1, 1), 3.0)
    readonly property color accentFillHover: dark ? Qt.lighter(accentFill, 1.08) : Qt.darker(accentFill, 1.08)
    readonly property color accentFillPressed: dark ? Qt.lighter(accentFill, 1.16) : Qt.darker(accentFill, 1.16)
    readonly property color onAccent: contrast(accentFill, Qt.rgba(1, 1, 1, 1)) >= 3.0 ? "#ffffff" : "#0d1015"
    readonly property color accentSoft: alpha(accent, dark ? 0.2 : 0.13)
    readonly property color accentSofter: alpha(accent, dark ? 0.11 : 0.07)
    readonly property color focusRing: alpha(accentText, 0.85)

    // --- Spacing scale (px) -------------------------------------------------
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 20
    readonly property int space6: 24
    readonly property int space7: 32
    readonly property int space8: 40
    readonly property int space9: 56

    // --- Radii --------------------------------------------------------------
    readonly property int radiusSmall: 6
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 12
    readonly property int radiusXLarge: 16

    // --- Typography (relative to the system font) ----------------------------
    readonly property real basePointSize: Qt.application.font.pointSize > 0 ? Qt.application.font.pointSize : 10
    readonly property real fontCaption: basePointSize * 0.85
    readonly property real fontSmall: basePointSize * 0.92
    readonly property real fontBody: basePointSize
    readonly property real fontTitle: basePointSize * 1.2
    readonly property real fontHeadline: basePointSize * 1.65
    readonly property real fontDisplay: basePointSize * 2.0

    // --- Sizes --------------------------------------------------------------
    readonly property int controlHeight: 32
    readonly property int sidebarWidth: 256
    readonly property int sidebarCompactWidth: 64
    readonly property int contentMaxWidth: 800
    readonly property int compactBreakpoint: 860

    // --- Motion -------------------------------------------------------------
    readonly property int durationFast: animations ? 110 : 0
    readonly property int durationNormal: animations ? 180 : 0
    readonly property int durationSlow: animations ? 260 : 0
}
