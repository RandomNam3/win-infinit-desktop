/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <string>

namespace Wid
{

/// The few texts of the service itself (tray menu, messages), English or German after the
/// Windows display language. The settings app has the full translations.
const wchar_t *text(const wchar_t *english, const wchar_t *german);

} // namespace Wid
