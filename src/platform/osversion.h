/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <cstdint>
#include <string>

namespace Wid
{

/// The real Windows version (RtlGetVersion and the registry, not the manifest dependent GetVersionEx).
struct OsVersion {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t build = 0;
    uint32_t revision = 0; ///< UBR, the monthly update
    std::wstring displayVersion; ///< "25H2" as Windows names it
};

OsVersion currentOsVersion();

// Windows 11 24H2, 25H2 and 26H2 share one platform (Germanium, build 26100) and get the same
// monthly updates; 25H2 and 26H2 are enablement packages on top of it. 26H1 (28000) is a
// separate platform that only comes preinstalled on new ARM64 devices.
inline constexpr uint32_t Build24H2 = 26100;
inline constexpr uint32_t Build25H2 = 26200;
inline constexpr uint32_t Build26H2 = 26300;
inline constexpr uint32_t Build26H1 = 28000;

enum class OsSupport {
    Unsupported, ///< older than the shared platform, the program refuses to start
    Untested, ///< runs, but nobody tested this build (24H2, 26H1, newer releases)
    Supported, ///< 25H2 and 26H2
};

OsSupport osSupport(const OsVersion &version);
/// "25H2" for a known build, otherwise empty.
std::wstring releaseName(uint32_t build);

} // namespace Wid
