// SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Text with the defaults of the design system.
import QtQuick

Text {
    color: Theme.text
    font.pointSize: Theme.fontBody
    renderType: Text.NativeRendering
    linkColor: Theme.accentText
    textFormat: Text.PlainText
}
