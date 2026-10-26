/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcService)

namespace Wid
{

/// Debug output on or off (setting DebugLogging, or WID_DEBUG=1 in the environment).
/// Warnings always go to the debugger output and to service.log in the state directory.
void installLogging();
void setDebugLogging(bool enabled);

} // namespace Wid
