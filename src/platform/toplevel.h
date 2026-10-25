/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace Wid
{

/// What a top-level window is to the canvas. Mirrors the window modes of KDE Infinit Desktop.
enum class WindowClass {
    Canvas, ///< normal application window, moves with the canvas
    Dialog, ///< owned window (dialog, tool palette), follows its owner
    Tool, ///< WS_EX_TOOLWINDOW without owner: floating palettes, overlays
    Desktop, ///< Progman / WorkerW (wallpaper and desktop icons)
    Taskbar, ///< Shell_TrayWnd / Shell_SecondaryTrayWnd
    Shell, ///< Start, search, notifications, flyouts, Task View …
    Overlay, ///< topmost, not activatable (game overlays, OSDs)
    Hidden, ///< not visible
    Cloaked, ///< cloaked by its app or by DWM (not by the shell)
    OtherDesktop, ///< cloaked by the shell: lies on another virtual desktop
    Empty, ///< zero size
};

const wchar_t *windowClassName(WindowClass windowClass);

/// A snapshot of one top-level window.
struct TopLevel {
    HWND hwnd = nullptr;
    HWND owner = nullptr;
    std::wstring className;
    std::wstring title;
    std::wstring exePath;
    std::wstring appUserModelId; ///< explicit AppUserModelID, empty for most Win32 apps
    DWORD processId = 0;
    LONG_PTR style = 0;
    LONG_PTR exStyle = 0;
    RECT windowRect = {}; ///< GetWindowRect, includes the invisible resize borders
    RECT frame = {}; ///< DWMWA_EXTENDED_FRAME_BOUNDS, what the user sees
    bool visible = false;
    DWORD cloaked = 0; ///< DWM_CLOAKED_* bits
    bool minimized = false;
    bool maximized = false;
    bool elevatedKnown = false;
    bool elevated = false;
};

/// Top-level windows in z-order, topmost first.
std::vector<HWND> enumerateTopLevel();
TopLevel describe(HWND hwnd);
WindowClass classify(const TopLevel &window);

/// The window the canvas sees as "empty desktop" under a screen point: true for the
/// desktop window and its icon view (SHELLDLL_DefView / SysListView32), at any depth.
bool isDesktopAt(POINT screenPoint);

} // namespace Wid
