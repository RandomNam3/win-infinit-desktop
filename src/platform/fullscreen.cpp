/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "fullscreen.h"

#include <shellapi.h>

#include <cstdlib>

namespace Wid
{

const wchar_t *userNotificationState()
{
    QUERY_USER_NOTIFICATION_STATE state = QUNS_ACCEPTS_NOTIFICATIONS;
    if (FAILED(SHQueryUserNotificationState(&state))) {
        return L"unknown";
    }
    switch (state) {
    case QUNS_NOT_PRESENT:
        return L"not-present";
    case QUNS_BUSY:
        return L"busy";
    case QUNS_RUNNING_D3D_FULL_SCREEN:
        return L"d3d-fullscreen";
    case QUNS_PRESENTATION_MODE:
        return L"presentation";
    case QUNS_ACCEPTS_NOTIFICATIONS:
        return L"accepts";
    case QUNS_QUIET_TIME:
        return L"quiet";
    case QUNS_APP:
        return L"app";
    }
    return L"unknown";
}

bool coversMonitor(const TopLevel &window, const RECT &monitor)
{
    if (window.maximized || window.minimized) {
        return false;
    }
    const RECT &frame = window.frame;
    return std::abs(frame.left - monitor.left) <= 1 && std::abs(frame.top - monitor.top) <= 1 && std::abs(frame.right - monitor.right) <= 1
        && std::abs(frame.bottom - monitor.bottom) <= 1;
}

} // namespace Wid
