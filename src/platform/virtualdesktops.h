/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <vector>

struct IVirtualDesktopManager;

namespace Wid
{

/**
 * The documented part of Windows' virtual desktops (IVirtualDesktopManager): which desktop
 * a window is on and whether that is the current one. Enumerating desktops, switching and
 * change notifications are only available through undocumented interfaces whose IDs change
 * between Windows builds; the canvas does not use them. A desktop switch shows up as a
 * burst of EVENT_OBJECT_CLOAKED / UNCLOAKED with DWM_CLOAKED_SHELL and as a change of
 * Explorer's registry values below.
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

/**
 * The current virtual desktop as Explorer keeps it in the registry:
 * HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VirtualDesktops, values
 * CurrentVirtualDesktop (one GUID) and VirtualDesktopIDs (all, in their order). Not
 * documented, but the same in every Windows 11 build so far; without them the canvas does
 * not tell desktops apart.
 */
std::optional<GUID> currentDesktopFromRegistry();
std::vector<GUID> desktopsFromRegistry();

/**
 * An event that is set whenever a value of that registry key changes (desktop switched,
 * added or removed). arm() after every signal: a notification fires once.
 */
class DesktopRegistryWatch
{
public:
    DesktopRegistryWatch();
    ~DesktopRegistryWatch();
    DesktopRegistryWatch(const DesktopRegistryWatch &) = delete;
    DesktopRegistryWatch &operator=(const DesktopRegistryWatch &) = delete;

    /// Opens the key and asks for the next change; false if the key does not exist (yet).
    bool arm();
    HANDLE event() const
    {
        return m_event;
    }

private:
    HKEY m_key = nullptr;
    HANDLE m_event = nullptr;
};

} // namespace Wid
