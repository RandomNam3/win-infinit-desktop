/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "windowevents.h"

#include <utility>

namespace Wid
{

namespace
{

// WinEventProc has no user data. One instance per thread is enough (the canvas runs all
// window handling on its main thread), so the active instance lives in a thread local.
thread_local WindowEvents *t_instance = nullptr;

struct EventRange {
    DWORD first;
    DWORD last;
};

// Narrow ranges: every event in a range costs a context switch per event system-wide.
constexpr EventRange s_ranges[] = {
    {EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND},
    {EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND},
    {EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND},
    {EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE},
    {EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE},
    {EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE},
    {EVENT_OBJECT_CLOAKED, EVENT_OBJECT_UNCLOAKED},
};

} // namespace

WindowEvents::~WindowEvents()
{
    stop();
}

bool WindowEvents::start(Callback callback)
{
    if (isRunning() || (t_instance && t_instance != this)) {
        return false;
    }
    m_callback = std::move(callback);
    t_instance = this;
    for (const EventRange &range : s_ranges) {
        HWINEVENTHOOK hook = SetWinEventHook(range.first, range.last, nullptr, &WindowEvents::dispatch, 0, 0,
                                             WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        if (!hook) {
            stop();
            return false;
        }
        m_hooks.push_back(hook);
    }
    return true;
}

void WindowEvents::stop()
{
    for (HWINEVENTHOOK hook : m_hooks) {
        UnhookWinEvent(hook);
    }
    m_hooks.clear();
    if (t_instance == this) {
        t_instance = nullptr;
    }
    m_callback = nullptr;
}

bool WindowEvents::isRunning() const
{
    return !m_hooks.empty();
}

void CALLBACK WindowEvents::dispatch(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
    WindowEvents *self = t_instance;
    if (!self || !self->m_callback || !hwnd || idObject != OBJID_WINDOW || idChild != CHILDID_SELF) {
        return;
    }
    // A destroyed window has no ancestor any more; the receiver only knows its own windows.
    if (event != EVENT_OBJECT_DESTROY && GetAncestor(hwnd, GA_ROOT) != hwnd) {
        return;
    }
    // Copy: the callback may call stop(), which resets m_callback.
    const Callback callback = self->m_callback;
    callback(event, hwnd);
}

const wchar_t *windowEventName(DWORD event)
{
    switch (event) {
    case EVENT_SYSTEM_FOREGROUND:
        return L"foreground";
    case EVENT_SYSTEM_MOVESIZESTART:
        return L"movesize-start";
    case EVENT_SYSTEM_MOVESIZEEND:
        return L"movesize-end";
    case EVENT_SYSTEM_MINIMIZESTART:
        return L"minimize-start";
    case EVENT_SYSTEM_MINIMIZEEND:
        return L"minimize-end";
    case EVENT_OBJECT_CREATE:
        return L"create";
    case EVENT_OBJECT_DESTROY:
        return L"destroy";
    case EVENT_OBJECT_SHOW:
        return L"show";
    case EVENT_OBJECT_HIDE:
        return L"hide";
    case EVENT_OBJECT_LOCATIONCHANGE:
        return L"location";
    case EVENT_OBJECT_NAMECHANGE:
        return L"title";
    case EVENT_OBJECT_CLOAKED:
        return L"cloaked";
    case EVENT_OBJECT_UNCLOAKED:
        return L"uncloaked";
    default:
        return L"other";
    }
}

} // namespace Wid
