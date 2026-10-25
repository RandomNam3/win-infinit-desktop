/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <windows.h>

class QImage;

namespace Wid
{

/**
 * Shows `image` (any format, converted to premultiplied ARGB) as the content of the layered
 * window `hwnd` at `topLeft` (screen coordinates), per-pixel alpha. Fully transparent pixels
 * let mouse input through to the windows below.
 */
bool updateLayeredWindow(HWND hwnd, const QImage &image, POINT topLeft);

/// Window icon (WM_GETICON with timeout, class icon, then the program's icon) as an image of
/// `size` × `size`; null image if there is none.
QImage windowIcon(HWND hwnd, int size);

} // namespace Wid
