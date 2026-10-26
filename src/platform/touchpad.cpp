/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "touchpad.h"

// Order matters: hidpi.h needs NTSTATUS (winternl.h) and USAGE (hidusage.h).
#include <winternl.h>

// MinGW's hidpi.h lacks extern "C"; nesting it is harmless with the Windows SDK.
extern "C" {
#include <hidusage.h>

#include <hidpi.h>
}

#include <algorithm>

namespace Wid
{

namespace
{

constexpr USHORT DigitizerPage = 0x0D;
constexpr USHORT TouchpadUsage = 0x05;
constexpr USAGE ContactCountUsage = 0x54;

} // namespace

std::vector<std::wstring> precisionTouchpads()
{
    std::vector<std::wstring> result;
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0) {
        return result;
    }
    std::vector<RAWINPUTDEVICELIST> devices(count);
    const UINT got = GetRawInputDeviceList(devices.data(), &count, sizeof(RAWINPUTDEVICELIST));
    if (got == static_cast<UINT>(-1)) {
        return result;
    }
    devices.resize(got);
    for (const RAWINPUTDEVICELIST &device : devices) {
        if (device.dwType != RIM_TYPEHID) {
            continue;
        }
        RID_DEVICE_INFO info = {};
        info.cbSize = sizeof(info);
        UINT size = sizeof(info);
        if (GetRawInputDeviceInfoW(device.hDevice, RIDI_DEVICEINFO, &info, &size) == static_cast<UINT>(-1)) {
            continue;
        }
        if (info.hid.usUsagePage != DigitizerPage || info.hid.usUsage != TouchpadUsage) {
            continue;
        }
        UINT nameLength = 0;
        GetRawInputDeviceInfoW(device.hDevice, RIDI_DEVICENAME, nullptr, &nameLength);
        std::wstring name(nameLength, L'\0');
        if (nameLength > 0 && GetRawInputDeviceInfoW(device.hDevice, RIDI_DEVICENAME, name.data(), &nameLength) != static_cast<UINT>(-1)) {
            name.resize(wcslen(name.c_str()));
        }
        result.push_back(name);
    }
    return result;
}

bool TouchpadContacts::start(HWND target)
{
    RAWINPUTDEVICE device = {};
    device.usUsagePage = DigitizerPage;
    device.usUsage = TouchpadUsage;
    device.dwFlags = RIDEV_INPUTSINK;
    device.hwndTarget = target;
    m_registered = RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
    return m_registered;
}

void TouchpadContacts::stop()
{
    if (m_registered) {
        RAWINPUTDEVICE device = {};
        device.usUsagePage = DigitizerPage;
        device.usUsage = TouchpadUsage;
        device.dwFlags = RIDEV_REMOVE;
        RegisterRawInputDevices(&device, 1, sizeof(device));
        m_registered = false;
    }
    m_preparsed.clear();
}

const std::vector<BYTE> *TouchpadContacts::preparsedData(HANDLE device)
{
    auto it = m_preparsed.find(device);
    if (it != m_preparsed.end()) {
        return &it->second;
    }
    UINT size = 0;
    if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, nullptr, &size) != 0 || size == 0) {
        return nullptr;
    }
    std::vector<BYTE> data(size);
    if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, data.data(), &size) == static_cast<UINT>(-1)) {
        return nullptr;
    }
    // Devices come and go rarely; bound the cache anyway.
    if (m_preparsed.size() > 16) {
        m_preparsed.clear();
    }
    return &m_preparsed.emplace(device, std::move(data)).first->second;
}

bool TouchpadContacts::handleInput(LPARAM lParam)
{
    UINT size = 0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0) {
        return false;
    }
    m_buffer.resize(size);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, m_buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size) {
        return false;
    }
    const auto *input = reinterpret_cast<const RAWINPUT *>(m_buffer.data());
    if (input->header.dwType != RIM_TYPEHID) {
        return false;
    }
    const std::vector<BYTE> *preparsed = preparsedData(input->header.hDevice);
    if (!preparsed) {
        return false;
    }
    const RAWHID &hid = input->data.hid;
    const BYTE *report = hid.bRawData;
    for (DWORD i = 0; i < hid.dwCount; ++i, report += hid.dwSizeHid) {
        ULONG contacts = 0;
        const NTSTATUS status = HidP_GetUsageValue(HidP_Input, DigitizerPage, 0, ContactCountUsage, &contacts,
                                                   reinterpret_cast<PHIDP_PREPARSED_DATA>(const_cast<BYTE *>(preparsed->data())),
                                                   reinterpret_cast<PCHAR>(const_cast<BYTE *>(report)), hid.dwSizeHid);
        if (status != HIDP_STATUS_SUCCESS) {
            continue;
        }
        // Hybrid reporting splits one frame over several reports and only the first carries
        // the count, hence the peak over a short window.
        const DWORD now = GetTickCount();
        if (now - m_lastTime > 50) {
            m_peakContacts = 0;
        }
        m_lastContacts = static_cast<int>(contacts);
        m_peakContacts = std::max(m_peakContacts, m_lastContacts);
        m_lastTime = now;
    }
    return true;
}

int TouchpadContacts::recentContacts(DWORD now, DWORD withinMs) const
{
    if (m_lastTime == 0 || now - m_lastTime > withinMs) {
        return 0;
    }
    return m_peakContacts;
}

} // namespace Wid
