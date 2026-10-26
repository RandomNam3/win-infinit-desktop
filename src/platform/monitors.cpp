/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "monitors.h"

#include <shellscalingapi.h>

#include <cstdio>

namespace Wid
{

namespace
{

struct DisplayConfigTarget {
    std::wstring gdiName;
    DISPLAYCONFIG_TARGET_DEVICE_NAME target;
};

/// Source (GDI name) to target (monitor, EDID) for every active path.
std::vector<DisplayConfigTarget> displayConfigTargets()
{
    std::vector<DisplayConfigTarget> result;
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    LONG status = ERROR_INSUFFICIENT_BUFFER;
    // The configuration can change between the two calls, hence the loop.
    for (int attempt = 0; attempt < 3 && status == ERROR_INSUFFICIENT_BUFFER; ++attempt) {
        UINT32 pathCount = 0;
        UINT32 modeCount = 0;
        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) {
            return result;
        }
        paths.resize(pathCount);
        modes.resize(modeCount);
        status = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
        paths.resize(pathCount);
    }
    if (status != ERROR_SUCCESS) {
        return result;
    }
    for (const DISPLAYCONFIG_PATH_INFO &path : paths) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source = {};
        source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source.header.size = sizeof(source);
        source.header.adapterId = path.sourceInfo.adapterId;
        source.header.id = path.sourceInfo.id;
        if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS) {
            continue;
        }
        DisplayConfigTarget entry;
        entry.gdiName = source.viewGdiDeviceName;
        entry.target = {};
        entry.target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        entry.target.header.size = sizeof(entry.target);
        entry.target.header.adapterId = path.targetInfo.adapterId;
        entry.target.header.id = path.targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&entry.target.header) != ERROR_SUCCESS) {
            continue;
        }
        result.push_back(std::move(entry));
    }
    return result;
}

BOOL CALLBACK collectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data)
{
    auto *monitors = reinterpret_cast<std::vector<MonitorInfo> *>(data);
    MONITORINFOEXW info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return TRUE;
    }
    MonitorInfo entry;
    entry.handle = monitor;
    entry.bounds = info.rcMonitor;
    entry.work = info.rcWork;
    entry.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    entry.gdiName = info.szDevice;
    UINT dpiX = 0;
    UINT dpiY = 0;
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        entry.dpi = dpiX;
    }
    monitors->push_back(std::move(entry));
    return TRUE;
}

} // namespace

std::wstring MonitorInfo::manufacturerCode() const
{
    if (!edidValid) {
        return {};
    }
    // EDID bytes 8-9: three 5 bit letters, 'A' = 1. DisplayConfig hands the two bytes over
    // in EDID order, so the first byte is the high one.
    const uint16_t id = static_cast<uint16_t>((edidManufacturer >> 8) | (edidManufacturer << 8));
    std::wstring code;
    for (int shift : {10, 5, 0}) {
        const int letter = (id >> shift) & 0x1f;
        if (letter < 1 || letter > 26) {
            return {};
        }
        code.push_back(static_cast<wchar_t>(L'A' + letter - 1));
    }
    return code;
}

std::wstring MonitorInfo::identity() const
{
    const std::wstring manufacturer = manufacturerCode();
    if (!manufacturer.empty()) {
        wchar_t product[8] = {};
        swprintf(product, 8, L"%04X", static_cast<unsigned>(edidProduct));
        return L"EDID:" + manufacturer + L":" + product;
    }
    if (!devicePath.empty()) {
        return L"PATH:" + devicePath;
    }
    return L"GDI:" + gdiName;
}

std::vector<MonitorInfo> enumerateMonitors()
{
    std::vector<MonitorInfo> monitors;
    EnumDisplayMonitors(nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&monitors));
    const std::vector<DisplayConfigTarget> targets = displayConfigTargets();
    for (MonitorInfo &monitor : monitors) {
        for (const DisplayConfigTarget &entry : targets) {
            if (entry.gdiName != monitor.gdiName) {
                continue;
            }
            // Mirrored monitors share a source; the first target wins.
            const DISPLAYCONFIG_TARGET_DEVICE_NAME &target = entry.target;
            monitor.friendlyName = target.monitorFriendlyDeviceName;
            monitor.devicePath = target.monitorDevicePath;
            monitor.edidValid = target.flags.edidIdsValid != 0;
            monitor.edidManufacturer = target.edidManufactureId;
            monitor.edidProduct = target.edidProductCodeId;
            monitor.connectorInstance = target.connectorInstance;
            monitor.outputTechnology = static_cast<int>(target.outputTechnology);
            break;
        }
    }
    return monitors;
}

} // namespace Wid
