/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <windows.h>

#include <map>
#include <string>
#include <vector>

namespace Wid
{

/// Device names of all precision touchpads (HID usage page 0x0D, usage 0x05).
std::vector<std::wstring> precisionTouchpads();

/**
 * Contacts on precision touchpads, from Raw Input (observe only, works in the background).
 *
 * Windows turns two finger scrolling into WM_MOUSEWHEEL, the same message as a wheel.
 * The canvas tells them apart by asking whether a touchpad reported two or more contacts
 * just before the wheel event (see recentContacts()).
 *
 * start() registers the target window for WM_INPUT; feed every WM_INPUT to handleInput().
 * Use from one thread.
 */
class TouchpadContacts
{
public:
    bool start(HWND target);
    void stop();

    /// Returns true if lParam was a touchpad report.
    bool handleInput(LPARAM lParam);
    /// Highest contact count reported within the last `withinMs` milliseconds.
    int recentContacts(DWORD now, DWORD withinMs) const;
    int lastContacts() const { return m_lastContacts; }

private:
    const std::vector<BYTE> *preparsedData(HANDLE device);

    std::map<HANDLE, std::vector<BYTE>> m_preparsed;
    std::vector<BYTE> m_buffer;
    int m_lastContacts = 0;
    int m_peakContacts = 0;
    DWORD m_lastTime = 0;
    bool m_registered = false;
};

} // namespace Wid
