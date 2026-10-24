/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "texts.h"

#include <windows.h>

namespace Wid
{

const wchar_t *text(const wchar_t *english, const wchar_t *german)
{
    static const bool isGerman = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_GERMAN;
    return isGerman ? german : english;
}

} // namespace Wid
