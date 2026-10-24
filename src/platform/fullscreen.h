/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "toplevel.h"

namespace Wid
{

/// SHQueryUserNotificationState as text: "busy" (a fullscreen app), "d3d-fullscreen"
/// (exclusive mode), "presentation", "quiet", "app", "accepts", "not-present", "unknown".
const wchar_t *userNotificationState();

/**
 * Fullscreen as the canvas understands it: a window whose visible frame covers its whole
 * monitor and that is not maximized. That includes the borderless window mode of games,
 * which sets no fullscreen state at all (same rule as KDE Infinit Desktop's
 * coversScreenBorderless, within 1 px).
 */
bool coversMonitor(const TopLevel &window, const RECT &monitor);

} // namespace Wid
