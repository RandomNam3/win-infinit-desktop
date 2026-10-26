/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "osversion.h"

#include <windows.h>

namespace Wid
{

namespace
{

const wchar_t *const s_currentVersionKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

std::wstring registryString(const wchar_t *name)
{
    wchar_t buffer[64] = {};
    DWORD size = sizeof(buffer);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, s_currentVersionKey, name, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS) {
        return {};
    }
    return buffer;
}

uint32_t registryDword(const wchar_t *name)
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, s_currentVersionKey, name, RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
        return 0;
    }
    return value;
}

} // namespace

OsVersion currentOsVersion()
{
    OsVersion version;
    // RtlGetVersion reports the real version regardless of the application manifest.
    using RtlGetVersionFn = LONG(WINAPI *)(OSVERSIONINFOW *);
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
        auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void *>(GetProcAddress(ntdll, "RtlGetVersion")));
        OSVERSIONINFOW info = {};
        info.dwOSVersionInfoSize = sizeof(info);
        if (rtlGetVersion && rtlGetVersion(&info) == 0) {
            version.major = info.dwMajorVersion;
            version.minor = info.dwMinorVersion;
            version.build = info.dwBuildNumber;
        }
    }
    version.revision = registryDword(L"UBR");
    version.displayVersion = registryString(L"DisplayVersion");
    return version;
}

OsSupport osSupport(const OsVersion &version)
{
    if (version.major < 10 || version.build < Build24H2) {
        return OsSupport::Unsupported;
    }
    if (version.build == Build25H2 || version.build == Build26H2) {
        return OsSupport::Supported;
    }
    return OsSupport::Untested;
}

std::wstring releaseName(uint32_t build)
{
    switch (build) {
    case Build24H2:
        return L"24H2";
    case Build25H2:
        return L"25H2";
    case Build26H2:
        return L"26H2";
    case Build26H1:
        return L"26H1";
    default:
        return {};
    }
}

} // namespace Wid
