/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <windows.h>

#include <optional>
#include <string>

struct IVirtualDesktopManager;

namespace Wid
{

/**
 * The documented part of Windows' virtual desktops (IVirtualDesktopManager): which desktop
 * a window is on and whether that is the current one. Enumerating desktops, switching and
 * change notifications are only available through undocumented interfaces whose IDs change
 * between Windows builds; the canvas does not use them. A desktop switch shows up as a
 * burst of EVENT_OBJECT_CLOAKED / UNCLOAKED with DWM_CLOAKED_SHELL instead.
 *
 * Needs COM on the calling thread (CoInitializeEx). Not thread safe.
 */
class VirtualDesktops
{
public:
    VirtualDesktops();
    ~VirtualDesktops();
    VirtualDesktops(const VirtualDesktops &) = delete;
    VirtualDesktops &operator=(const VirtualDesktops &) = delete;

    bool isAvailable() const { return m_manager != nullptr; }
    std::optional<GUID> desktopOf(HWND hwnd) const;
    std::optional<bool> isOnCurrentDesktop(HWND hwnd) const;

private:
    IVirtualDesktopManager *m_manager = nullptr;
};

std::wstring guidString(const GUID &guid);

} // namespace Wid
