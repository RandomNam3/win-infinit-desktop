/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Wid
{

/**
 * One monitor, as the canvas needs it: geometry in physical pixels (the process is
 * per-monitor DPI aware v2), work area without taskbars and app bars, and an identity
 * that survives reconnecting the monitor to another port (EDID manufacturer and product,
 * like KDE Infinite Desktop's placement memory).
 *
 * DisplayConfig does not report the EDID serial number. Two identical monitors are told
 * apart by their device path (it contains the connector), see identity().
 */
struct MonitorInfo {
    HMONITOR handle = nullptr;
    RECT bounds = {};
    RECT work = {};
    UINT dpi = 96;
    bool primary = false;
    std::wstring gdiName; ///< "\\.\DISPLAY1"
    std::wstring friendlyName; ///< from the EDID, e.g. "DELL U2723QE"
    std::wstring devicePath; ///< monitor device interface path
    uint16_t edidManufacturer = 0; ///< as stored in the EDID (big endian PNP id)
    uint16_t edidProduct = 0;
    bool edidValid = false;
    uint32_t connectorInstance = 0;
    int outputTechnology = 0; ///< DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY

    /// Three letter PNP manufacturer id ("DEL"), empty without EDID.
    std::wstring manufacturerCode() const;
    /// "EDID:DEL:41A3" or, without EDID, "PATH:<device path>" / "GDI:<gdi name>".
    std::wstring identity() const;
};

/// All active monitors in EnumDisplayMonitors order.
std::vector<MonitorInfo> enumerateMonitors();

} // namespace Wid
