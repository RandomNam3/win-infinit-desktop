/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "virtualdesktops.h"

#include <shobjidl.h>

namespace Wid
{

namespace
{

// Defined here so that neither uuid.lib nor compiler specific __uuidof is needed.
const CLSID s_clsidVirtualDesktopManager = {0xAA509086, 0x5CA9, 0x4C25, {0x8F, 0x95, 0x58, 0x9D, 0x3C, 0x07, 0xB4, 0x8A}};
const IID s_iidVirtualDesktopManager = {0xA5CD92FF, 0x29BE, 0x454C, {0x8D, 0x04, 0xD8, 0x28, 0x79, 0xFB, 0x3F, 0x1B}};

} // namespace

VirtualDesktops::VirtualDesktops()
{
    void *manager = nullptr;
    if (SUCCEEDED(CoCreateInstance(s_clsidVirtualDesktopManager, nullptr, CLSCTX_ALL, s_iidVirtualDesktopManager, &manager))) {
        m_manager = static_cast<IVirtualDesktopManager *>(manager);
    }
}

VirtualDesktops::~VirtualDesktops()
{
    if (m_manager) {
        m_manager->Release();
    }
}

std::optional<GUID> VirtualDesktops::desktopOf(HWND hwnd) const
{
    GUID desktop = {};
    if (!m_manager || FAILED(m_manager->GetWindowDesktopId(hwnd, &desktop))) {
        return std::nullopt;
    }
    return desktop;
}

std::optional<bool> VirtualDesktops::isOnCurrentDesktop(HWND hwnd) const
{
    BOOL current = FALSE;
    if (!m_manager || FAILED(m_manager->IsWindowOnCurrentVirtualDesktop(hwnd, &current))) {
        return std::nullopt;
    }
    return current != FALSE;
}

std::wstring guidString(const GUID &guid)
{
    wchar_t buffer[40] = {};
    StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer)));
    return buffer;
}

} // namespace Wid
