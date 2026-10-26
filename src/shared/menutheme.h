/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

namespace Wid
{

#ifdef _WIN32
namespace detail
{

// uxtheme.dll exports these by ordinal only (Windows 10 1903 and newer, unchanged in 11).
enum class PreferredAppMode {
    Default,
    AllowDark,
    ForceDark,
    ForceLight,
};
using SetPreferredAppModeFunction = PreferredAppMode(WINAPI *)(PreferredAppMode);
using VoidFunction = void(WINAPI *)();
constexpr WORD SetPreferredAppModeOrdinal = 135;
constexpr WORD FlushMenuThemesOrdinal = 136;
constexpr WORD RefreshImmersiveColorPolicyStateOrdinal = 104;

inline FARPROC uxthemeOrdinal(WORD ordinal)
{
    static const HMODULE module = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return module ? GetProcAddress(module, MAKEINTRESOURCEA(ordinal)) : nullptr;
}

inline bool hasAppModes()
{
    using RtlGetVersionFunction = LONG(WINAPI *)(OSVERSIONINFOW *);
    OSVERSIONINFOW version = {};
    version.dwOSVersionInfoSize = sizeof(version);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto getVersion = ntdll ? reinterpret_cast<RtlGetVersionFunction>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;
    return getVersion && getVersion(&version) == 0 && version.dwMajorVersion >= 10 && version.dwBuildNumber >= 18362;
}

} // namespace detail

/// The app mode of Windows changed (WM_SETTINGCHANGE "ImmersiveColorSet"): menus opened from
/// now on use it.
inline void refreshMenuTheme()
{
    if (!detail::hasAppModes()) {
        return;
    }
    if (const auto refresh = reinterpret_cast<detail::VoidFunction>(detail::uxthemeOrdinal(detail::RefreshImmersiveColorPolicyStateOrdinal))) {
        refresh();
    }
    if (const auto flush = reinterpret_cast<detail::VoidFunction>(detail::uxthemeOrdinal(detail::FlushMenuThemesOrdinal))) {
        flush();
    }
}

/**
 * The native popup menus of this process (TrackPopupMenu: canvas menu, tray menu, Qt's native
 * menus) follow the app mode of Windows: dark when Settings → Personalization → Colors says
 * dark. Windows has no documented way for this; the uxtheme functions are undocumented and
 * used by many programs. Without them the menus simply stay light. Call it before the first
 * menu.
 */
inline void followSystemMenuTheme()
{
    if (!detail::hasAppModes()) {
        return;
    }
    if (const auto setMode = reinterpret_cast<detail::SetPreferredAppModeFunction>(detail::uxthemeOrdinal(detail::SetPreferredAppModeOrdinal))) {
        setMode(detail::PreferredAppMode::AllowDark);
    }
    refreshMenuTheme();
}
#else
inline void refreshMenuTheme()
{
}
inline void followSystemMenuTheme()
{
}
#endif

} // namespace Wid
