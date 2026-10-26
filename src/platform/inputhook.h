/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <windows.h>

#include <atomic>
#include <functional>
#include <thread>

namespace Wid
{

struct MouseEvent {
    WPARAM message = 0; ///< WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_MOUSEWHEEL, …
    POINT position = {};
    int wheelDelta = 0; ///< signed, 120 per classic notch; also for WM_MOUSEHWHEEL
    DWORD flags = 0; ///< LLMHF_INJECTED, LLMHF_LOWER_IL_INJECTED
    ULONG_PTR extraInfo = 0;
    DWORD time = 0;
};

struct KeyEvent {
    WPARAM message = 0; ///< WM_KEYDOWN, WM_KEYUP, WM_SYSKEYDOWN, WM_SYSKEYUP
    DWORD vk = 0;
    DWORD scanCode = 0;
    DWORD flags = 0; ///< LLKHF_INJECTED, LLKHF_UP, …
    ULONG_PTR extraInfo = 0;
    DWORD time = 0;
};

enum class HookAction {
    Pass,
    Swallow,
};

/**
 * System-wide low-level mouse and keyboard hooks (WH_MOUSE_LL, WH_KEYBOARD_LL) on a
 * dedicated thread.
 *
 * Windows removes a low-level hook without notice when it does not return within
 * LowLevelHooksTimeout (at most 1 s since Windows 10 1709). The filters therefore run on
 * this thread only and must decide at once: no window calls, no locks that the main
 * thread holds for long. Forward anything else to the main thread (PostMessage).
 * Because a removal cannot be detected, the hooks are installed again every
 * rearmInterval milliseconds (0 = never).
 *
 * Windows key: Start opens when Win is pressed and released alone, and a Win32 menu bar
 * gets the focus when Alt is. When a filter swallows an event while Win or Alt is held
 * (Win + wheel, Win + drag, a shortcut), the hook sends an unassigned key (VK 0xE8), like
 * AutoHotkey's menu mask key: once right away and once more right before the release of Win
 * or Alt passes on. The shell and the application then see a key combination and do
 * nothing on the release.
 *
 * Input from this class carries injectedTag in its extra info and is never filtered.
 * Events into elevated windows are not seen unless the process runs elevated or with
 * uiAccess (UIPI).
 */
class InputHook
{
public:
    using MouseFilter = std::function<HookAction(const MouseEvent &)>;
    using KeyFilter = std::function<HookAction(const KeyEvent &)>;

    static constexpr ULONG_PTR injectedTag = 0x57494400; // "WID\0"
    static constexpr WORD maskKey = 0xE8; // unassigned virtual key

    InputHook() = default;
    ~InputHook();
    InputHook(const InputHook &) = delete;
    InputHook &operator=(const InputHook &) = delete;

    /// Either filter may be empty (that hook is not installed). Only one instance per process.
    bool start(MouseFilter mouse, KeyFilter key, unsigned rearmInterval = 0);
    void stop();
    bool isRunning() const;

    /// Modifiers as the hook saw them (MOD_ALT, MOD_CONTROL, MOD_SHIFT, MOD_WIN). Safe to
    /// call from the filters and from other threads.
    UINT modifiers() const { return m_modifiers.load(); }

private:
    static LRESULT CALLBACK mouseProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK keyProc(int code, WPARAM wParam, LPARAM lParam);
    void run(std::atomic<int> *started, unsigned rearmInterval);
    bool install();
    void uninstall();
    void swallowed();
    void sendMask();
    void trackModifier(DWORD vk, bool down);

    MouseFilter m_mouseFilter;
    KeyFilter m_keyFilter;
    std::thread m_thread;
    std::atomic<DWORD> m_threadId{0};
    HHOOK m_mouseHook = nullptr;
    HHOOK m_keyHook = nullptr;
    std::atomic<UINT> m_modifiers{0};
    // Only touched on the hook thread: the mask key was sent while Win/Alt is held.
    bool m_masked = false;
};

} // namespace Wid
