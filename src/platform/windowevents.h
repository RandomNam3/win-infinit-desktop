/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <windows.h>

#include <functional>
#include <vector>

namespace Wid
{

/**
 * Top-level window events of all processes (SetWinEventHook, out of context).
 *
 * The callback runs on the thread that called start(), from its message loop, and only
 * for top-level windows (OBJID_WINDOW, CHILDID_SELF, GA_ROOT). Events are asynchronous:
 * the window may be gone by the time the callback runs, so never trust an HWND without
 * checking IsWindow().
 *
 * Not copyable; stop() (or the destructor) must run on the same thread as start().
 */
class WindowEvents
{
public:
    /// event is one of the EVENT_* constants (EVENT_OBJECT_SHOW, EVENT_SYSTEM_MOVESIZESTART, …).
    using Callback = std::function<void(DWORD event, HWND hwnd)>;

    WindowEvents() = default;
    ~WindowEvents();
    WindowEvents(const WindowEvents &) = delete;
    WindowEvents &operator=(const WindowEvents &) = delete;

    bool start(Callback callback);
    void stop();
    bool isRunning() const;

private:
    static void CALLBACK dispatch(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD thread, DWORD time);

    std::vector<HWINEVENTHOOK> m_hooks;
    Callback m_callback;
};

const wchar_t *windowEventName(DWORD event);

} // namespace Wid
