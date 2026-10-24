/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <windows.h>

namespace Wid
{

/// Parked windows stay within ±ParkingLimit: WM_MOVE packs coordinates into 16 bits, and
/// (-32000, -32000) is where Windows puts minimized windows.
inline constexpr int ParkingLimit = 30000;

/**
 * Moves a window so that its *visible* frame (DWMWA_EXTENDED_FRAME_BOUNDS) starts at
 * `frameTopLeft`. Windows 10/11 windows have invisible resize borders around the frame,
 * so the window rectangle is a few pixels larger.
 *
 * Without `synchronous`, the move is posted to the window's thread (SWP_ASYNCWINDOWPOS):
 * a hung application cannot block the canvas. Never activates and never changes the
 * z-order. Fails (ERROR_ACCESS_DENIED) for windows of elevated processes unless this
 * process is elevated too or has uiAccess. `windowTopLeft` receives the window rect position
 * that was requested.
 */
bool moveFrameTo(HWND hwnd, POINT frameTopLeft, bool synchronous = false, DWORD *error = nullptr, POINT *windowTopLeft = nullptr);

/// Offset from the visible frame to the window rectangle (left, top), usually (-7, 0).
POINT frameOffset(HWND hwnd);

} // namespace Wid
