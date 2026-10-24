/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "windowmover.h"

#include <dwmapi.h>

#include <algorithm>

namespace Wid
{

POINT frameOffset(HWND hwnd)
{
    RECT window = {};
    RECT frame = {};
    if (!GetWindowRect(hwnd, &window) || FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame)))) {
        return {0, 0};
    }
    return {window.left - frame.left, window.top - frame.top};
}

bool moveFrameTo(HWND hwnd, POINT frameTopLeft, bool synchronous, DWORD *error, POINT *windowTopLeft)
{
    const POINT offset = frameOffset(hwnd);
    const int x = std::clamp<int>(frameTopLeft.x + offset.x, -ParkingLimit, ParkingLimit);
    const int y = std::clamp<int>(frameTopLeft.y + offset.y, -ParkingLimit, ParkingLimit);
    if (windowTopLeft) {
        *windowTopLeft = POINT{x, y};
    }
    UINT flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE;
    if (!synchronous) {
        flags |= SWP_ASYNCWINDOWPOS;
    }
    SetLastError(ERROR_SUCCESS);
    const bool moved = SetWindowPos(hwnd, nullptr, x, y, 0, 0, flags) != FALSE;
    if (error) {
        *error = moved ? ERROR_SUCCESS : GetLastError();
    }
    return moved;
}

} // namespace Wid
