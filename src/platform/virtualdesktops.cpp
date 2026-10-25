/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "virtualdesktops.h"

#include <shobjidl.h>

#include <cstring>

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

namespace
{

const wchar_t *const s_desktopsKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VirtualDesktops";

std::vector<GUID> readGuids(const wchar_t *value)
{
    BYTE data[16 * 64] = {};
    DWORD size = sizeof(data);
    DWORD type = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, s_desktopsKey, value, RRF_RT_REG_BINARY, &type, data, &size) != ERROR_SUCCESS) {
        return {};
    }
    std::vector<GUID> guids(size / sizeof(GUID));
    std::memcpy(guids.data(), data, guids.size() * sizeof(GUID));
    return guids;
}

} // namespace

std::optional<GUID> currentDesktopFromRegistry()
{
    const std::vector<GUID> current = readGuids(L"CurrentVirtualDesktop");
    if (current.size() != 1 || current.front() == GUID{}) {
        return std::nullopt;
    }
    return current.front();
}

std::vector<GUID> desktopsFromRegistry()
{
    return readGuids(L"VirtualDesktopIDs");
}

DesktopRegistryWatch::DesktopRegistryWatch()
    : m_event(CreateEventW(nullptr, FALSE, FALSE, nullptr))
{
}

DesktopRegistryWatch::~DesktopRegistryWatch()
{
    if (m_key) {
        RegCloseKey(m_key); // also ends a pending notification
    }
    if (m_event) {
        CloseHandle(m_event);
    }
}

bool DesktopRegistryWatch::arm()
{
    if (!m_event) {
        return false;
    }
    if (!m_key && RegOpenKeyExW(HKEY_CURRENT_USER, s_desktopsKey, 0, KEY_NOTIFY | KEY_QUERY_VALUE, &m_key) != ERROR_SUCCESS) {
        m_key = nullptr;
        return false;
    }
    // Asynchronous: the call returns at once and the event is set on the next change.
    return RegNotifyChangeKeyValue(m_key, FALSE, REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_THREAD_AGNOSTIC, m_event, TRUE) == ERROR_SUCCESS;
}

} // namespace Wid
