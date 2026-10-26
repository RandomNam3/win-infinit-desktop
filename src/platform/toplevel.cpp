/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "toplevel.h"

#include <dwmapi.h>
#include <propsys.h>
#include <shellapi.h>

#include <array>
#include <string_view>

namespace Wid
{

namespace
{

// PKEY_AppUserModel_ID, defined here instead of linking a GUID library.
const PROPERTYKEY s_appUserModelIdKey = {{0x9F4C2855, 0x9F79, 0x4B39, {0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3}}, 5};

constexpr std::array<std::wstring_view, 2> s_desktopClasses = {L"Progman", L"WorkerW"};
constexpr std::array<std::wstring_view, 2> s_taskbarClasses = {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd"};
// Shell surfaces that are top-level windows but never application windows.
constexpr std::array<std::wstring_view, 11> s_shellClasses = {
    L"Windows.UI.Core.CoreWindow", // Start, search, Action Center hosts
    L"XamlExplorerHostIslandWindow", // Task View, Snap Layouts, Alt+Tab
    L"ForegroundStaging",
    L"MultitaskingViewFrame",
    L"TaskListThumbnailWnd",
    L"NotifyIconOverflowWindow",
    L"TopLevelWindowForOverflowXamlIsland",
    L"Shell_InputSwitchTopLevelWindow",
    L"Windows.Internal.Shell.TabProxyWindow",
    L"WorkerW_TrayNotify",
    L"Xaml_WindowedPopupClass",
};

template<std::size_t N>
bool contains(const std::array<std::wstring_view, N> &list, const std::wstring &value)
{
    for (std::wstring_view entry : list) {
        if (entry == value) {
            return true;
        }
    }
    return false;
}

std::wstring classNameOf(HWND hwnd)
{
    wchar_t buffer[256] = {};
    const int length = GetClassNameW(hwnd, buffer, static_cast<int>(std::size(buffer)));
    return std::wstring(buffer, length > 0 ? static_cast<std::size_t>(length) : 0);
}

std::wstring titleOf(HWND hwnd)
{
    // InternalGetWindowText does not send WM_GETTEXT, so a hung window cannot block us.
    wchar_t buffer[512] = {};
    const int length = InternalGetWindowText(hwnd, buffer, static_cast<int>(std::size(buffer)));
    return std::wstring(buffer, length > 0 ? static_cast<std::size_t>(length) : 0);
}

void describeProcess(TopLevel &window)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, window.processId);
    if (!process) {
        return;
    }
    wchar_t path[MAX_PATH * 2] = {};
    DWORD size = static_cast<DWORD>(std::size(path));
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        window.exePath.assign(path, size);
    }
    HANDLE token = nullptr;
    if (OpenProcessToken(process, TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation = {};
        DWORD returned = 0;
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &returned)) {
            window.elevatedKnown = true;
            window.elevated = elevation.TokenIsElevated != 0;
        }
        CloseHandle(token);
    }
    CloseHandle(process);
}

std::wstring appUserModelIdOf(HWND hwnd)
{
    std::wstring result;
    IPropertyStore *store = nullptr;
    if (FAILED(SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&store)))) {
        return result;
    }
    PROPVARIANT value;
    PropVariantInit(&value);
    if (SUCCEEDED(store->GetValue(s_appUserModelIdKey, &value)) && value.vt == VT_LPWSTR && value.pwszVal) {
        result = value.pwszVal;
    }
    PropVariantClear(&value);
    store->Release();
    return result;
}

BOOL CALLBACK collectWindow(HWND hwnd, LPARAM data)
{
    reinterpret_cast<std::vector<HWND> *>(data)->push_back(hwnd);
    return TRUE;
}

} // namespace

const wchar_t *windowClassName(WindowClass windowClass)
{
    switch (windowClass) {
    case WindowClass::Canvas:
        return L"canvas";
    case WindowClass::Dialog:
        return L"dialog";
    case WindowClass::Tool:
        return L"tool";
    case WindowClass::Desktop:
        return L"desktop";
    case WindowClass::Taskbar:
        return L"taskbar";
    case WindowClass::Shell:
        return L"shell";
    case WindowClass::Overlay:
        return L"overlay";
    case WindowClass::Hidden:
        return L"hidden";
    case WindowClass::Cloaked:
        return L"cloaked";
    case WindowClass::OtherDesktop:
        return L"other-desktop";
    case WindowClass::Empty:
        return L"empty";
    }
    return L"?";
}

std::vector<HWND> enumerateTopLevel()
{
    std::vector<HWND> windows;
    windows.reserve(256);
    EnumWindows(collectWindow, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

TopLevel describe(HWND hwnd)
{
    TopLevel window;
    window.hwnd = hwnd;
    window.owner = GetWindow(hwnd, GW_OWNER);
    window.className = classNameOf(hwnd);
    window.title = titleOf(hwnd);
    GetWindowThreadProcessId(hwnd, &window.processId);
    window.style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    window.exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    GetWindowRect(hwnd, &window.windowRect);
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &window.frame, sizeof(window.frame)))) {
        window.frame = window.windowRect;
    }
    window.visible = IsWindowVisible(hwnd) != FALSE;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &window.cloaked, sizeof(window.cloaked));
    window.minimized = IsIconic(hwnd) != FALSE;
    window.maximized = IsZoomed(hwnd) != FALSE;
    describeProcess(window);
    window.appUserModelId = appUserModelIdOf(hwnd);
    return window;
}

WindowClass classify(const TopLevel &window)
{
    if (contains(s_desktopClasses, window.className)) {
        return WindowClass::Desktop;
    }
    if (contains(s_taskbarClasses, window.className)) {
        return WindowClass::Taskbar;
    }
    if (contains(s_shellClasses, window.className)) {
        return WindowClass::Shell;
    }
    if (!window.visible) {
        return WindowClass::Hidden;
    }
    if (window.cloaked == DWM_CLOAKED_SHELL) {
        return WindowClass::OtherDesktop;
    }
    if (window.cloaked != 0) {
        return WindowClass::Cloaked;
    }
    if (!window.minimized && (window.frame.right <= window.frame.left || window.frame.bottom <= window.frame.top)) {
        return WindowClass::Empty;
    }
    const bool toolWindow = (window.exStyle & WS_EX_TOOLWINDOW) != 0;
    const bool appWindow = (window.exStyle & WS_EX_APPWINDOW) != 0;
    if ((window.exStyle & WS_EX_TOPMOST) && (window.exStyle & WS_EX_NOACTIVATE)) {
        return WindowClass::Overlay;
    }
    if (window.owner && !appWindow) {
        return WindowClass::Dialog;
    }
    if (toolWindow && !appWindow) {
        return WindowClass::Tool;
    }
    return WindowClass::Canvas;
}

bool isDesktopAt(POINT screenPoint)
{
    HWND hwnd = WindowFromPoint(screenPoint);
    if (!hwnd) {
        return false;
    }
    HWND root = GetAncestor(hwnd, GA_ROOT);
    return root && contains(s_desktopClasses, classNameOf(root));
}

} // namespace Wid
