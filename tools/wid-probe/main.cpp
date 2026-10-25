/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later

    wid-probe: reports what the Windows interfaces the canvas relies on actually do on
    this machine (see docs/ARCHITEKTUR.md, section 15). Window titles are only
    included with --titles, and paths below the user profile are shortened to
    %USERPROFILE%, so the report can be shared.
*/
#include "fullscreen.h"
#include "inputhook.h"
#include "monitors.h"
#include "osversion.h"
#include "toplevel.h"
#include "touchpad.h"
#include "virtualdesktops.h"
#include "windowevents.h"
#include "windowmover.h"

#include <dwmapi.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdarg>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

using namespace Wid;

namespace
{

// --- Output -------------------------------------------------------------------------------

std::string utf8(const std::wstring &text)
{
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

void out(const std::wstring &text)
{
    const std::string bytes = utf8(text);
    fwrite(bytes.data(), 1, bytes.size(), stdout);
}

void line(const std::wstring &text = {})
{
    out(text + L"\n");
}

std::wstring format(const wchar_t *pattern, ...)
{
    wchar_t buffer[1024];
    va_list args;
    va_start(args, pattern);
    _vsnwprintf(buffer, std::size(buffer) - 1, pattern, args);
    va_end(args);
    buffer[std::size(buffer) - 1] = L'\0';
    return buffer;
}

std::wstring rectText(const RECT &rect)
{
    return format(L"%ld,%ld %ldx%ld", rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}

std::wstring hwndText(HWND hwnd)
{
    return format(L"0x%llX", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hwnd)));
}

std::wstring privatePath(const std::wstring &path)
{
    wchar_t profile[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    if (length > 0 && length < MAX_PATH && path.size() >= length && _wcsnicmp(path.c_str(), profile, length) == 0) {
        return L"%USERPROFILE%" + path.substr(length);
    }
    return path;
}

std::wstring baseName(const std::wstring &path)
{
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

/// Minimal JSON writer: objects and arrays as nested scopes, values escaped.
class Json
{
public:
    void beginObject(const wchar_t *key = nullptr)
    {
        prefix(key);
        m_text += L"{";
        m_first.push_back(true);
    }
    void endObject()
    {
        m_first.pop_back();
        m_text += L"}";
    }
    void beginArray(const wchar_t *key = nullptr)
    {
        prefix(key);
        m_text += L"[";
        m_first.push_back(true);
    }
    void endArray()
    {
        m_first.pop_back();
        m_text += L"]";
    }
    void value(const wchar_t *key, const std::wstring &text)
    {
        prefix(key);
        m_text += L"\"" + escape(text) + L"\"";
    }
    void value(const wchar_t *key, const wchar_t *text)
    {
        value(key, std::wstring(text ? text : L""));
    }
    void value(const wchar_t *key, long long number)
    {
        prefix(key);
        m_text += std::to_wstring(number);
    }
    void value(const wchar_t *key, bool flag)
    {
        prefix(key);
        m_text += flag ? L"true" : L"false";
    }
    void rect(const wchar_t *key, const RECT &rect)
    {
        beginArray(key);
        value(nullptr, static_cast<long long>(rect.left));
        value(nullptr, static_cast<long long>(rect.top));
        value(nullptr, static_cast<long long>(rect.right - rect.left));
        value(nullptr, static_cast<long long>(rect.bottom - rect.top));
        endArray();
    }
    const std::wstring &text() const
    {
        return m_text;
    }

private:
    void prefix(const wchar_t *key)
    {
        if (!m_first.empty()) {
            if (!m_first.back()) {
                m_text += L",";
            }
            m_first.back() = false;
        }
        if (key) {
            m_text += L"\"" + escape(key) + L"\":";
        }
    }
    static std::wstring escape(const std::wstring &text)
    {
        std::wstring result;
        result.reserve(text.size());
        for (wchar_t c : text) {
            switch (c) {
            case L'"':
                result += L"\\\"";
                break;
            case L'\\':
                result += L"\\\\";
                break;
            case L'\n':
                result += L"\\n";
                break;
            case L'\r':
                result += L"\\r";
                break;
            case L'\t':
                result += L"\\t";
                break;
            default:
                if (c < 0x20) {
                    result += format(L"\\u%04x", static_cast<unsigned>(c));
                } else {
                    result += c;
                }
            }
        }
        return result;
    }

    std::wstring m_text;
    std::vector<bool> m_first;
};

// --- Helpers ------------------------------------------------------------------------------

bool selfElevated()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }
    TOKEN_ELEVATION elevation = {};
    DWORD size = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated;
    CloseHandle(token);
    return ok;
}

std::optional<DWORD> registryDword(HKEY root, const wchar_t *key, const wchar_t *name)
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(root, key, name, RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return value;
}

std::optional<DWORD> registryBinarySize(HKEY root, const wchar_t *key, const wchar_t *name)
{
    DWORD size = 0;
    if (RegGetValueW(root, key, name, RRF_RT_REG_BINARY, nullptr, nullptr, &size) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return size;
}

bool systemFlag(UINT action)
{
    BOOL value = FALSE;
    SystemParametersInfoW(action, 0, &value, 0);
    return value != FALSE;
}

int monitorIndex(const std::vector<MonitorInfo> &monitors, HWND hwnd)
{
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    for (std::size_t i = 0; i < monitors.size(); ++i) {
        if (monitors[i].handle == monitor) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const wchar_t *supportText(OsSupport support)
{
    switch (support) {
    case OsSupport::Supported:
        return L"supported";
    case OsSupport::Untested:
        return L"untested";
    case OsSupport::Unsupported:
        return L"unsupported";
    }
    return L"?";
}

/// Runs the message loop of this thread for `milliseconds`, calling `onMessage` for every message.
template<typename Handler>
void pumpFor(DWORD milliseconds, Handler onMessage)
{
    const ULONGLONG end = GetTickCount64() + milliseconds;
    for (;;) {
        const ULONGLONG now = GetTickCount64();
        if (now >= end) {
            return;
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, static_cast<DWORD>(end - now), QS_ALLINPUT);
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            onMessage(message);
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

double milliseconds(LARGE_INTEGER from, LARGE_INTEGER to)
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return static_cast<double>(to.QuadPart - from.QuadPart) * 1000.0 / static_cast<double>(frequency.QuadPart);
}

// --- Report -------------------------------------------------------------------------------

struct Options {
    bool json = false;
    bool titles = false;
    bool all = false;
};

const wchar_t *const s_touchpadKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\PrecisionTouchPad";

int report(const Options &options)
{
    const OsVersion os = currentOsVersion();
    const std::vector<MonitorInfo> monitors = enumerateMonitors();
    const std::vector<std::wstring> touchpads = precisionTouchpads();
    VirtualDesktops desktops;
    const RECT virtualScreen = {GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                                GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN)};
    const std::optional<DWORD> hookTimeout = registryDword(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"LowLevelHooksTimeout");
    const std::optional<DWORD> desktopIds =
        registryBinarySize(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VirtualDesktops", L"VirtualDesktopIDs");
    const wchar_t *touchpadValues[] = {L"ThreeFingerSlideEnabled", L"FourFingerSlideEnabled", L"PanEnabled", L"ZoomEnabled", L"ScrollDirection"};

    struct Row {
        TopLevel window;
        WindowClass windowClass;
        int monitor;
        bool fullscreen;
        std::optional<GUID> desktop;
        std::optional<bool> currentDesktop;
    };
    std::vector<Row> rows;
    std::map<WindowClass, int> classCounts;
    for (HWND hwnd : enumerateTopLevel()) {
        Row row{describe(hwnd), WindowClass::Hidden, -1, false, std::nullopt, std::nullopt};
        row.windowClass = classify(row.window);
        ++classCounts[row.windowClass];
        if (row.windowClass == WindowClass::Hidden && !options.all) {
            continue;
        }
        row.monitor = monitorIndex(monitors, hwnd);
        if (row.monitor >= 0) {
            row.fullscreen = coversMonitor(row.window, monitors[static_cast<std::size_t>(row.monitor)].bounds);
        }
        row.desktop = desktops.desktopOf(hwnd);
        row.currentDesktop = desktops.isOnCurrentDesktop(hwnd);
        rows.push_back(std::move(row));
    }

    if (options.json) {
        Json json;
        json.beginObject();
        json.value(L"probeVersion", std::wstring(WID_VERSION_STRING, WID_VERSION_STRING + strlen(WID_VERSION_STRING)));
        json.beginObject(L"os");
        json.value(L"version", format(L"%u.%u.%u.%u", os.major, os.minor, os.build, os.revision));
        json.value(L"displayVersion", os.displayVersion);
        json.value(L"release", releaseName(os.build));
        json.value(L"support", supportText(osSupport(os)));
        json.endObject();
        json.value(L"elevated", selfElevated());
        json.value(L"notificationState", userNotificationState());
        json.rect(L"virtualScreen", virtualScreen);
        json.beginArray(L"monitors");
        for (const MonitorInfo &monitor : monitors) {
            json.beginObject();
            json.value(L"gdiName", monitor.gdiName);
            json.value(L"name", monitor.friendlyName);
            json.value(L"identity", monitor.identity());
            json.value(L"edidValid", monitor.edidValid);
            json.value(L"connector", static_cast<long long>(monitor.connectorInstance));
            json.value(L"outputTechnology", static_cast<long long>(monitor.outputTechnology));
            json.rect(L"bounds", monitor.bounds);
            json.rect(L"work", monitor.work);
            json.value(L"dpi", static_cast<long long>(monitor.dpi));
            json.value(L"primary", monitor.primary);
            json.endObject();
        }
        json.endArray();
        json.beginObject(L"settings");
        json.value(L"snap", systemFlag(SPI_GETWINARRANGING));
        json.value(L"dockMoving", systemFlag(SPI_GETDOCKMOVING));
        json.value(L"snapSizing", systemFlag(SPI_GETSNAPSIZING));
        json.value(L"lowLevelHooksTimeout", hookTimeout ? static_cast<long long>(*hookTimeout) : -1LL);
        json.value(L"virtualDesktopApi", desktops.isAvailable());
        json.value(L"virtualDesktops", desktopIds ? static_cast<long long>(*desktopIds / 16) : -1LL);
        json.beginArray(L"precisionTouchpads");
        for (const std::wstring &touchpad : touchpads) {
            json.value(nullptr, touchpad);
        }
        json.endArray();
        json.beginObject(L"touchpadRegistry");
        for (const wchar_t *name : touchpadValues) {
            const std::optional<DWORD> value = registryDword(HKEY_CURRENT_USER, s_touchpadKey, name);
            json.value(name, value ? static_cast<long long>(*value) : -1LL);
        }
        json.endObject();
        json.endObject();
        json.beginObject(L"windowCounts");
        for (const auto &[windowClass, count] : classCounts) {
            json.value(windowClassName(windowClass), static_cast<long long>(count));
        }
        json.endObject();
        json.beginArray(L"windows");
        for (const Row &row : rows) {
            const TopLevel &w = row.window;
            json.beginObject();
            json.value(L"hwnd", hwndText(w.hwnd));
            json.value(L"class", windowClassName(row.windowClass));
            json.value(L"windowClass", w.className);
            if (options.titles) {
                json.value(L"title", w.title);
            }
            json.value(L"exe", baseName(w.exePath));
            json.value(L"path", privatePath(w.exePath));
            json.value(L"aumid", w.appUserModelId);
            json.value(L"pid", static_cast<long long>(w.processId));
            json.value(L"elevated", w.elevatedKnown ? (w.elevated ? L"yes" : L"no") : L"unknown");
            json.value(L"owner", w.owner ? hwndText(w.owner) : std::wstring());
            json.value(L"style", format(L"0x%08llX", static_cast<unsigned long long>(w.style)));
            json.value(L"exStyle", format(L"0x%08llX", static_cast<unsigned long long>(w.exStyle)));
            json.rect(L"frame", w.frame);
            json.rect(L"windowRect", w.windowRect);
            json.value(L"cloaked", static_cast<long long>(w.cloaked));
            json.value(L"minimized", w.minimized);
            json.value(L"maximized", w.maximized);
            json.value(L"fullscreen", row.fullscreen);
            json.value(L"monitor", static_cast<long long>(row.monitor));
            json.value(L"desktop", row.desktop ? guidString(*row.desktop) : std::wstring());
            json.value(L"currentDesktop", row.currentDesktop ? (*row.currentDesktop ? L"yes" : L"no") : L"unknown");
            json.endObject();
        }
        json.endArray();
        json.endObject();
        line(json.text());
        return 0;
    }

    line(L"== WIN Infinit Desktop probe " WID_VERSION_STRING L" ==");
    line(format(L"Windows %u.%u.%u.%u  DisplayVersion %ls  (%ls)", os.major, os.minor, os.build, os.revision, os.displayVersion.c_str(),
                supportText(osSupport(os))));
    line(format(L"Elevated: %ls   Notification state: %ls", selfElevated() ? L"yes" : L"no", userNotificationState()));
    line(L"Virtual screen: " + rectText(virtualScreen));
    line();
    line(format(L"Monitors (%zu):", monitors.size()));
    for (std::size_t i = 0; i < monitors.size(); ++i) {
        const MonitorInfo &m = monitors[i];
        line(format(L"  [%zu] %ls %ls  %ls  work %ls  %u dpi%ls", i, m.gdiName.c_str(), m.friendlyName.c_str(), rectText(m.bounds).c_str(),
                    rectText(m.work).c_str(), m.dpi, m.primary ? L"  primary" : L""));
        line(format(L"      identity %ls  connector %u  output %d", m.identity().c_str(), m.connectorInstance, m.outputTechnology));
    }
    line();
    line(format(L"Snap %ls, dock moving %ls, snap sizing %ls", systemFlag(SPI_GETWINARRANGING) ? L"on" : L"off",
                systemFlag(SPI_GETDOCKMOVING) ? L"on" : L"off", systemFlag(SPI_GETSNAPSIZING) ? L"on" : L"off"));
    line(format(L"LowLevelHooksTimeout: %ls", hookTimeout ? std::to_wstring(*hookTimeout).c_str() : L"default"));
    line(format(L"Virtual desktops: API %ls, %ls desktops", desktops.isAvailable() ? L"available" : L"missing",
                desktopIds ? std::to_wstring(*desktopIds / 16).c_str() : L"?"));
    line(format(L"Precision touchpads: %zu", touchpads.size()));
    for (const wchar_t *name : touchpadValues) {
        const std::optional<DWORD> value = registryDword(HKEY_CURRENT_USER, s_touchpadKey, name);
        if (value) {
            line(format(L"  %ls = %lu", name, *value));
        }
    }
    line();
    std::wstring counts = L"Top-level windows:";
    for (const auto &[windowClass, count] : classCounts) {
        counts += format(L" %ls %d", windowClassName(windowClass), count);
    }
    line(counts);
    for (const Row &row : rows) {
        const TopLevel &w = row.window;
        std::wstring flags;
        if (w.minimized) {
            flags += L" min";
        }
        if (w.maximized) {
            flags += L" max";
        }
        if (row.fullscreen) {
            flags += L" fullscreen";
        }
        if (w.elevatedKnown && w.elevated) {
            flags += L" elevated";
        } else if (!w.elevatedKnown) {
            flags += L" elevation?";
        }
        if (w.cloaked) {
            flags += format(L" cloaked=%lu", w.cloaked);
        }
        if (row.currentDesktop && !*row.currentDesktop) {
            flags += L" other-desktop";
        }
        line(format(L"  %-12ls %-13ls %-28ls %-24ls mon %d  %ls%ls", hwndText(w.hwnd).c_str(), windowClassName(row.windowClass),
                    w.className.substr(0, 28).c_str(), baseName(w.exePath).substr(0, 24).c_str(), row.monitor, rectText(w.frame).c_str(),
                    flags.c_str()));
        if (options.titles && !w.title.empty()) {
            line(L"               \"" + w.title + L"\"");
        }
        if (!w.appUserModelId.empty()) {
            line(L"               AUMID " + w.appUserModelId);
        }
    }
    return 0;
}

// --- Input --------------------------------------------------------------------------------

LRESULT CALLBACK probeWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

HWND createHiddenWindow()
{
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = probeWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"WidProbeHidden";
    RegisterClassW(&windowClass);
    return CreateWindowExW(WS_EX_TOOLWINDOW, windowClass.lpszClassName, L"wid-probe", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
                           windowClass.hInstance, nullptr);
}

int watchInput(DWORD seconds)
{
    struct Wheel {
        DWORD time;
        int delta;
        bool horizontal;
        bool injected;
        bool control;
        ULONG_PTR extraInfo;
    };
    std::mutex mutex;
    std::vector<Wheel> wheels;
    std::atomic<long> moves{0};
    std::atomic<long> buttons{0};

    HWND hidden = createHiddenWindow();
    TouchpadContacts touchpad;
    const bool touchpadOk = touchpad.start(hidden);
    std::vector<std::pair<DWORD, int>> contacts;

    InputHook hook;
    const bool hookOk = hook.start(
        [&](const MouseEvent &event) {
            if (event.message == WM_MOUSEWHEEL || event.message == WM_MOUSEHWHEEL) {
                Wheel wheel{event.time, event.wheelDelta, event.message == WM_MOUSEHWHEEL, (event.flags & LLMHF_INJECTED) != 0,
                            (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0, event.extraInfo};
                std::lock_guard lock(mutex);
                if (wheels.size() < 100000) {
                    wheels.push_back(wheel);
                }
            } else if (event.message == WM_MOUSEMOVE) {
                ++moves;
            } else {
                ++buttons;
            }
            return HookAction::Pass;
        },
        {});
    line(format(L"Watching input for %lu s (touchpad raw input %ls, mouse hook %ls).", seconds, touchpadOk ? L"on" : L"off",
                hookOk ? L"on" : L"off"));
    line(L"Turn the mouse wheel, scroll with two fingers on the touchpad, pinch, and Ctrl + wheel.");
    pumpFor(seconds * 1000, [&](const MSG &message) {
        if (message.message == WM_INPUT && touchpad.handleInput(message.lParam)) {
            if (contacts.size() < 200000) {
                contacts.emplace_back(GetTickCount(), touchpad.lastContacts());
            }
        }
    });
    hook.stop();
    touchpad.stop();
    DestroyWindow(hidden);

    std::lock_guard lock(mutex);
    int notch = 0;
    int fine = 0;
    int withFingers = 0;
    int notchWithFingers = 0;
    int controlInjected = 0;
    line();
    line(L"  time   delta  axis  injected ctrl  extra       fingers");
    const DWORD start = wheels.empty() ? 0 : wheels.front().time;
    for (const Wheel &wheel : wheels) {
        int fingers = 0;
        for (const auto &[time, count] : contacts) {
            // Message times and GetTickCount share the same clock.
            const long difference = static_cast<long>(wheel.time - time);
            if (difference >= -10 && difference <= 60) {
                fingers = std::max(fingers, count);
            }
        }
        const bool isNotch = wheel.delta % 120 == 0;
        notch += isNotch ? 1 : 0;
        fine += isNotch ? 0 : 1;
        withFingers += fingers >= 2 ? 1 : 0;
        notchWithFingers += (fingers >= 2 && isNotch) ? 1 : 0;
        controlInjected += (wheel.control && wheel.injected) ? 1 : 0;
        if (&wheel - wheels.data() < 400) {
            line(format(L"  %6lu %6d  %ls     %ls      %ls  0x%08llX  %d", wheel.time - start, wheel.delta, wheel.horizontal ? L"h" : L"v",
                        wheel.injected ? L"yes" : L"no ", wheel.control ? L"yes" : L"no ",
                        static_cast<unsigned long long>(wheel.extraInfo), fingers));
        }
    }
    line();
    line(format(L"Wheel events %zu: multiples of 120 %d, finer %d; with >= 2 touchpad fingers %d (of them multiples of 120: %d)",
                wheels.size(), notch, fine, withFingers, notchWithFingers));
    line(format(L"Ctrl + injected wheel (pinch?) %d; touchpad reports %zu; mouse moves %ld, buttons %ld", controlInjected, contacts.size(),
                moves.load(), buttons.load()));
    return 0;
}

int testWinKey(DWORD seconds)
{
    std::atomic<long> swallowed{0};
    InputHook hook;
    const bool ok = hook.start(
        [&](const MouseEvent &event) {
            if ((hook.modifiers() & MOD_WIN) && (event.message == WM_MOUSEWHEEL || event.message == WM_MOUSEHWHEEL)) {
                ++swallowed;
                return HookAction::Swallow;
            }
            return HookAction::Pass;
        },
        {});
    if (!ok) {
        line(L"Could not install the low-level hooks.");
        return 1;
    }
    line(format(L"For %lu s: Win + mouse wheel is swallowed, and VK 0xE8 is sent once so that Start stays closed.", seconds));
    line(L"1. Hold Win, turn the wheel, release Win: Start must stay closed.");
    line(L"2. Press and release Win alone: Start must open as usual.");
    pumpFor(seconds * 1000, [](const MSG &) {});
    hook.stop();
    line(format(L"Swallowed wheel events: %ld. Please report whether Start stayed closed in step 1 and opened in step 2.", swallowed.load()));
    return 0;
}

// --- Window events ------------------------------------------------------------------------

int watchEvents(DWORD seconds)
{
    std::map<HWND, int> locations;
    const ULONGLONG start = GetTickCount64();
    WindowEvents events;
    const bool ok = events.start([&](DWORD event, HWND hwnd) {
        if (event == EVENT_OBJECT_LOCATIONCHANGE) {
            ++locations[hwnd];
            return;
        }
        TopLevel window;
        if (event != EVENT_OBJECT_DESTROY && IsWindow(hwnd)) {
            window = describe(hwnd);
        }
        line(format(L"  %6llu  %-15ls %-12ls %-13ls %-28ls %ls", GetTickCount64() - start, windowEventName(event), hwndText(hwnd).c_str(),
                    window.hwnd ? windowClassName(classify(window)) : L"", window.className.substr(0, 28).c_str(),
                    baseName(window.exePath).c_str()));
    });
    if (!ok) {
        line(L"Could not install the window event hooks.");
        return 1;
    }
    line(format(L"Watching window events for %lu s. Open, move, minimize and close some windows, switch virtual desktops.", seconds));
    pumpFor(seconds * 1000, [](const MSG &) {});
    events.stop();
    line(L"Location changes per window:");
    for (const auto &[hwnd, count] : locations) {
        line(format(L"  %ls  %d", hwndText(hwnd).c_str(), count));
    }
    return 0;
}

// --- Moving windows -----------------------------------------------------------------------

LRESULT CALLBACK colorWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_TIMER) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        // A new solid color every frame, so a frozen picture is easy to detect.
        static unsigned counter = 0;
        ++counter;
        const COLORREF color = RGB((counter * 53) % 256, (counter * 97) % 256, (counter * 151) % 256);
        HBRUSH brush = CreateSolidBrush(color);
        RECT client;
        GetClientRect(hwnd, &client);
        FillRect(dc, &client, brush);
        DeleteObject(brush);
        EndPaint(hwnd, &paint);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

HWND createColorWindow(const wchar_t *title, int x, int y, int width, int height)
{
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = colorWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = L"WidProbeColor";
    RegisterClassW(&windowClass);
    HWND hwnd = CreateWindowExW(0, windowClass.lpszClassName, title, WS_OVERLAPPEDWINDOW, x, y, width, height, nullptr, nullptr,
                                windowClass.hInstance, nullptr);
    if (hwnd) {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SetTimer(hwnd, 1, 50, nullptr);
    }
    return hwnd;
}

int testMove(const std::wstring &target)
{
    HWND hwnd = nullptr;
    bool own = false;
    if (target == L"self") {
        hwnd = createColorWindow(L"wid-probe move test", 200, 200, 480, 320);
        own = true;
        pumpFor(300, [](const MSG &) {});
    } else {
        hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(wcstoull(target.c_str(), nullptr, 16)));
    }
    if (!hwnd || !IsWindow(hwnd)) {
        line(L"No such window.");
        return 1;
    }
    TopLevel window = describe(hwnd);
    line(format(L"Window %ls  %ls  %ls  elevated %ls", hwndText(hwnd).c_str(), window.className.c_str(), baseName(window.exePath).c_str(),
                window.elevatedKnown ? (window.elevated ? L"yes" : L"no") : L"unknown"));
    const POINT offset = frameOffset(hwnd);
    line(format(L"Frame %ls, window rect %ls, frame offset %ld,%ld", rectText(window.frame).c_str(), rectText(window.windowRect).c_str(),
                offset.x, offset.y));
    const POINT original = {window.frame.left, window.frame.top};
    const RECT virtualScreen = {GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                                GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN)};
    const POINT targets[] = {{virtualScreen.right + 200, original.y},
                             {29000, original.y},
                             {-29000, original.y},
                             {original.x, 29000},
                             {original.x, -29000},
                             {40000, original.y}};
    for (const POINT &point : targets) {
        DWORD error = 0;
        LARGE_INTEGER before;
        LARGE_INTEGER after;
        QueryPerformanceCounter(&before);
        const bool moved = moveFrameTo(hwnd, point, true, &error);
        QueryPerformanceCounter(&after);
        pumpFor(100, [](const MSG &) {});
        const TopLevel now = describe(hwnd);
        line(format(L"  move to %6ld,%6ld: %ls (error %lu, %.2f ms) -> frame at %ld,%ld, monitor %ls", point.x, point.y, moved ? L"ok" : L"failed",
                    error, milliseconds(before, after), now.frame.left, now.frame.top,
                    MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL) ? L"some" : L"none"));
    }
    // Cost of moving one window per frame, as live panning would.
    LARGE_INTEGER before;
    LARGE_INTEGER after;
    QueryPerformanceCounter(&before);
    for (int i = 0; i < 60; ++i) {
        moveFrameTo(hwnd, {original.x + i * 4, original.y}, true);
    }
    QueryPerformanceCounter(&after);
    line(format(L"  60 synchronous moves: %.2f ms each", milliseconds(before, after) / 60.0));
    QueryPerformanceCounter(&before);
    for (int i = 0; i < 60; ++i) {
        moveFrameTo(hwnd, {original.x + i * 4, original.y}, false);
    }
    QueryPerformanceCounter(&after);
    line(format(L"  60 asynchronous moves: %.3f ms each (call only)", milliseconds(before, after) / 60.0));
    moveFrameTo(hwnd, original, true);
    pumpFor(100, [](const MSG &) {});
    const TopLevel restored = describe(hwnd);
    line(format(L"  restored to %ld,%ld", restored.frame.left, restored.frame.top));
    if (own) {
        DestroyWindow(hwnd);
    }
    return 0;
}

// --- DWM thumbnails -----------------------------------------------------------------------

/// Samples the screen pixel in the middle of `rect` a few times; true if it changed.
bool pixelChanges(const RECT &rect, int samples)
{
    HDC screen = GetDC(nullptr);
    const int x = (rect.left + rect.right) / 2;
    const int y = (rect.top + rect.bottom) / 2;
    COLORREF first = CLR_INVALID;
    bool changed = false;
    for (int i = 0; i < samples; ++i) {
        pumpFor(120, [](const MSG &) {});
        const COLORREF color = GetPixel(screen, x, y);
        if (first == CLR_INVALID) {
            first = color;
        } else if (color != first) {
            changed = true;
        }
    }
    ReleaseDC(nullptr, screen);
    return changed;
}

int testThumbnail()
{
    HWND source = createColorWindow(L"wid-probe thumbnail source", 120, 120, 360, 240);
    HWND viewer = createColorWindow(L"wid-probe thumbnail viewer", 520, 120, 360, 280);
    if (!source || !viewer) {
        line(L"Could not create the test windows.");
        return 1;
    }
    // The viewer only shows the thumbnail, it does not animate itself.
    KillTimer(viewer, 1);
    HTHUMBNAIL thumbnail = nullptr;
    if (FAILED(DwmRegisterThumbnail(viewer, source, &thumbnail))) {
        line(L"DwmRegisterThumbnail failed.");
        DestroyWindow(source);
        DestroyWindow(viewer);
        return 1;
    }
    RECT client;
    GetClientRect(viewer, &client);
    DWM_THUMBNAIL_PROPERTIES properties = {};
    properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;
    properties.rcDestination = client;
    properties.fVisible = TRUE;
    properties.opacity = 255;
    properties.fSourceClientAreaOnly = TRUE;
    DwmUpdateThumbnailProperties(thumbnail, &properties);
    pumpFor(400, [](const MSG &) {});

    POINT corner = {client.left, client.top};
    ClientToScreen(viewer, &corner);
    const RECT viewed = {corner.x, corner.y, corner.x + client.right, corner.y + client.bottom};
    const RECT original = describe(source).frame;

    const auto check = [&](const wchar_t *what) {
        line(format(L"  %-34ls thumbnail %ls", what, pixelChanges(viewed, 8) ? L"LIVE" : L"frozen"));
    };
    line(L"DWM thumbnail of an animated window (keep both test windows uncovered):");
    check(L"source on screen");
    moveFrameTo(source, {29000, 100}, true);
    check(L"source parked at x = 29000");
    moveFrameTo(source, {GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN) - 40, original.top}, true);
    check(L"source mostly off screen (40 px left)");
    moveFrameTo(source, {original.left, original.top}, true);
    BOOL cloak = TRUE;
    DwmSetWindowAttribute(source, DWMWA_CLOAK, &cloak, sizeof(cloak));
    check(L"source cloaked");
    cloak = FALSE;
    DwmSetWindowAttribute(source, DWMWA_CLOAK, &cloak, sizeof(cloak));
    ShowWindow(source, SW_SHOWMINNOACTIVE);
    check(L"source minimized");
    ShowWindow(source, SW_SHOWNOACTIVATE);
    SetWindowPos(viewer, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(source, viewer, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    moveFrameTo(source, {original.left + 400, original.top}, true);
    check(L"source behind the viewer (occluded)");

    DwmUnregisterThumbnail(thumbnail);
    DestroyWindow(source);
    DestroyWindow(viewer);
    return 0;
}

/// The top-level window the user sees at a screen point (physical pixels): which one is on top.
int windowAt(int x, int y)
{
    const HWND hwnd = WindowFromPoint(POINT{x, y});
    const HWND root = hwnd ? GetAncestor(hwnd, GA_ROOT) : nullptr;
    if (!root) {
        line(L"none");
        return 1;
    }
    const TopLevel top = describe(root);
    line(hwndText(root) + L" " + top.className + L" " + baseName(top.exePath));
    return 0;
}

void usage()
{
    line(L"wid-probe " WID_VERSION_STRING L" - checks the Windows interfaces WIN Infinit Desktop uses");
    line();
    line(L"  wid-probe [--json] [--titles] [--all]   system, monitors, settings, top-level windows");
    line(L"  wid-probe --watch-input SECONDS         wheel vs. touchpad, pinch, injected input");
    line(L"  wid-probe --test-winkey SECONDS         Win + wheel without opening Start");
    line(L"  wid-probe --watch-events SECONDS        window events (create, move, minimize, cloak, ...)");
    line(L"  wid-probe --test-move self|HWND         off-screen limits and the cost of moving a window");
    line(L"  wid-probe --test-thumbnail              are DWM thumbnails live for parked/cloaked/minimized windows?");
    line(L"  wid-probe --window-at X Y               the top-level window on top at a screen point (class, program)");
    line();
    line(L"Titles are left out unless --titles is given; paths in the profile become %USERPROFILE%.");
}

DWORD secondsArgument(int argc, wchar_t **argv, int index, DWORD fallback)
{
    if (index + 1 < argc) {
        const unsigned long value = wcstoul(argv[index + 1], nullptr, 10);
        if (value > 0 && value <= 600) {
            return static_cast<DWORD>(value);
        }
    }
    return fallback;
}

} // namespace

int wmain(int argc, wchar_t **argv)
{
    SetConsoleOutputCP(CP_UTF8);
    // Physical pixels everywhere, like the canvas.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    Options options;
    int result = -1;
    for (int i = 1; i < argc && result < 0; ++i) {
        const std::wstring argument = argv[i];
        if (argument == L"--help" || argument == L"-h" || argument == L"/?") {
            usage();
            result = 0;
        } else if (argument == L"--json") {
            options.json = true;
        } else if (argument == L"--titles") {
            options.titles = true;
        } else if (argument == L"--all") {
            options.all = true;
        } else if (argument == L"--watch-input") {
            result = watchInput(secondsArgument(argc, argv, i, 15));
        } else if (argument == L"--test-winkey") {
            result = testWinKey(secondsArgument(argc, argv, i, 20));
        } else if (argument == L"--watch-events") {
            result = watchEvents(secondsArgument(argc, argv, i, 30));
        } else if (argument == L"--test-move") {
            result = testMove(i + 1 < argc ? argv[i + 1] : L"self");
        } else if (argument == L"--test-thumbnail") {
            result = testThumbnail();
        } else if (argument == L"--window-at" && i + 2 < argc) {
            result = windowAt(_wtoi(argv[i + 1]), _wtoi(argv[i + 2]));
        }
    }
    if (result < 0) {
        result = report(options);
    }
    CoUninitialize();
    fflush(stdout);
    return result;
}
