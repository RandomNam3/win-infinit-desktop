/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "win32util.h"

namespace Wid
{

ATOM registerWindowClass(const wchar_t *name, WNDPROC proc, UINT style, HCURSOR cursor, HICON icon)
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (GetClassInfoExW(instance, name, &windowClass)) {
        // Already registered; the atom itself is not needed by the callers.
        return 1;
    }
    windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = style;
    windowClass.lpfnWndProc = proc;
    windowClass.hInstance = instance;
    windowClass.hCursor = cursor ? cursor : LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = icon;
    windowClass.lpszClassName = name;
    return RegisterClassExW(&windowClass);
}

} // namespace Wid
