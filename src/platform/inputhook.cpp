/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "inputhook.h"

#include <utility>

namespace Wid
{

namespace
{

std::atomic<InputHook *> s_instance{nullptr};

constexpr UINT RearmTimerId = 1;

UINT modifierOf(DWORD vk)
{
    switch (vk) {
    case VK_LWIN:
    case VK_RWIN:
        return MOD_WIN;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return MOD_ALT;
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        return MOD_CONTROL;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return MOD_SHIFT;
    default:
        return 0;
    }
}

bool isKeyUp(WPARAM message)
{
    return message == WM_KEYUP || message == WM_SYSKEYUP;
}

} // namespace

InputHook::~InputHook()
{
    stop();
}

bool InputHook::start(MouseFilter mouse, KeyFilter key, unsigned rearmInterval)
{
    InputHook *expected = nullptr;
    if (isRunning() || !s_instance.compare_exchange_strong(expected, this)) {
        return false;
    }
    m_mouseFilter = std::move(mouse);
    m_keyFilter = std::move(key);
    // 0 = starting, 1 = hooks installed, -1 = failed
    std::atomic<int> started{0};
    m_thread = std::thread(&InputHook::run, this, &started, rearmInterval);
    while (started.load() == 0) {
        Sleep(1);
    }
    if (started.load() < 0) {
        m_thread.join();
        s_instance.store(nullptr);
        return false;
    }
    return true;
}

void InputHook::stop()
{
    if (m_thread.joinable()) {
        PostThreadMessageW(m_threadId.load(), WM_QUIT, 0, 0);
        m_thread.join();
    }
    m_threadId.store(0);
    InputHook *self = this;
    s_instance.compare_exchange_strong(self, nullptr);
    m_mouseFilter = nullptr;
    m_keyFilter = nullptr;
}

bool InputHook::isRunning() const
{
    return m_thread.joinable();
}

void InputHook::run(std::atomic<int> *started, unsigned rearmInterval)
{
    m_threadId.store(GetCurrentThreadId());
    // Create the message queue before anyone can post WM_QUIT to it.
    MSG message;
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    if (!install()) {
        uninstall();
        started->store(-1);
        return;
    }
    // Hooks run at the priority of the installing thread.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    if (rearmInterval > 0) {
        SetTimer(nullptr, RearmTimerId, rearmInterval, nullptr);
    }
    started->store(1);
    // `started` is not touched any more: it lives on the stack of start().
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_TIMER && message.hwnd == nullptr) {
            uninstall();
            install();
            continue;
        }
        DispatchMessageW(&message);
    }
    uninstall();
}

bool InputHook::install()
{
    HINSTANCE module = GetModuleHandleW(nullptr);
    if (m_mouseFilter) {
        m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, &InputHook::mouseProc, module, 0);
        if (!m_mouseHook) {
            return false;
        }
    }
    if (m_keyFilter || m_mouseFilter) {
        // The keyboard hook also serves the Windows key mask for the mouse filter.
        m_keyHook = SetWindowsHookExW(WH_KEYBOARD_LL, &InputHook::keyProc, module, 0);
        if (!m_keyHook) {
            return false;
        }
    }
    return true;
}

void InputHook::uninstall()
{
    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }
    if (m_keyHook) {
        UnhookWindowsHookEx(m_keyHook);
        m_keyHook = nullptr;
    }
}

LRESULT CALLBACK InputHook::mouseProc(int code, WPARAM wParam, LPARAM lParam)
{
    InputHook *self = s_instance.load();
    if (code != HC_ACTION || !self || !self->m_mouseFilter) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    const auto *data = reinterpret_cast<const MSLLHOOKSTRUCT *>(lParam);
    if (data->dwExtraInfo == injectedTag) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    MouseEvent event;
    event.message = wParam;
    event.position = data->pt;
    event.flags = data->flags;
    event.extraInfo = data->dwExtraInfo;
    event.time = data->time;
    if (wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) {
        event.wheelDelta = static_cast<short>(HIWORD(data->mouseData));
    }
    if (self->m_mouseFilter(event) == HookAction::Swallow) {
        self->swallowed();
        return 1;
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK InputHook::keyProc(int code, WPARAM wParam, LPARAM lParam)
{
    InputHook *self = s_instance.load();
    if (code != HC_ACTION || !self) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    const auto *data = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);
    if (data->dwExtraInfo == injectedTag) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    self->trackModifier(data->vkCode, !isKeyUp(wParam));
    if (self->m_keyFilter) {
        KeyEvent event;
        event.message = wParam;
        event.vk = data->vkCode;
        event.scanCode = data->scanCode;
        event.flags = data->flags;
        event.extraInfo = data->dwExtraInfo;
        event.time = data->time;
        if (self->m_keyFilter(event) == HookAction::Swallow) {
            self->swallowed();
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void InputHook::trackModifier(DWORD vk, bool down)
{
    const UINT modifier = modifierOf(vk);
    if (!modifier) {
        return;
    }
    const UINT before = m_modifiers.load();
    UINT after = down ? (before | modifier) : (before & ~modifier);
    // Left and right keys share one bit; good enough for shortcuts.
    m_modifiers.store(after);
    if (!(after & (MOD_WIN | MOD_ALT))) {
        m_masked = false;
    }
}

void InputHook::swallowed()
{
    if (m_masked || !(m_modifiers.load() & (MOD_WIN | MOD_ALT))) {
        return;
    }
    m_masked = true;
    INPUT inputs[2] = {};
    for (INPUT &input : inputs) {
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = maskKey;
        input.ki.dwExtraInfo = injectedTag;
    }
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

} // namespace Wid
