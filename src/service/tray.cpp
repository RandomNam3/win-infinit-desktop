/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "tray.h"
#include "texts.h"

#include <shellapi.h>

#include <cwchar>

namespace Wid
{

namespace
{

constexpr UINT TrayMessage = WM_APP + 1;
constexpr UINT TrayId = 1;
const wchar_t *const s_windowClass = L"WinInfinitDesktopTray";

enum MenuId : UINT {
    MenuPause = 1,
    MenuBringBack,
    MenuSettings,
    MenuLeaveSafeMode,
    MenuQuit,
};

void copy(wchar_t *target, std::size_t size, const std::wstring &text)
{
    wcsncpy_s(target, size, text.c_str(), _TRUNCATE);
}

} // namespace

Tray::Tray(Handler handler)
    : m_handler(std::move(handler))
{
}

Tray::~Tray()
{
    if (m_added) {
        NOTIFYICONDATAW data = {};
        data.cbSize = sizeof(data);
        data.hWnd = m_window.get();
        data.uID = TrayId;
        Shell_NotifyIconW(NIM_DELETE, &data);
    }
    detachWindowObject(m_window.get());
}

bool Tray::show()
{
    if (!m_window) {
        registerWindowClass(s_windowClass, &Tray::windowProc);
        m_window.reset(CreateWindowExW(WS_EX_TOOLWINDOW, s_windowClass, L"WIN Infinit Desktop", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                                       GetModuleHandleW(nullptr), this));
        m_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
        const int size = GetSystemMetrics(SM_CXSMICON);
        m_icon.reset(static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, size, size, 0)));
    }
    return m_window && addIcon();
}

bool Tray::addIcon()
{
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = m_window.get();
    data.uID = TrayId;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = TrayMessage;
    data.hIcon = m_icon ? m_icon.get() : LoadIconW(nullptr, IDI_APPLICATION);
    copy(data.szTip, std::size(data.szTip), L"WIN Infinit Desktop");
    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        // Still there from before an Explorer restart.
        Shell_NotifyIconW(NIM_MODIFY, &data);
    }
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    m_added = true;
    updateTip();
    return true;
}

void Tray::setState(const State &state)
{
    m_state = state;
    updateTip();
}

void Tray::updateTip()
{
    if (!m_added) {
        return;
    }
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = m_window.get();
    data.uID = TrayId;
    data.uFlags = NIF_TIP | NIF_SHOWTIP;
    std::wstring tip = L"WIN Infinit Desktop";
    if (!m_state.status.empty()) {
        tip += L"\n" + m_state.status;
    }
    copy(data.szTip, std::size(data.szTip), tip);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void Tray::notify(const std::wstring &title, const std::wstring &message)
{
    if (!m_added) {
        return;
    }
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = m_window.get();
    data.uID = TrayId;
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_INFO;
    copy(data.szInfoTitle, std::size(data.szInfoTitle), title);
    copy(data.szInfo, std::size(data.szInfo), message);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void Tray::showMenu()
{
    UniqueMenu menu(CreatePopupMenu());
    if (!menu) {
        return;
    }
    if (!m_state.status.empty()) {
        AppendMenuW(menu.get(), MF_STRING | MF_GRAYED, 0, m_state.status.c_str());
        AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    }
    if (m_state.safeMode) {
        AppendMenuW(menu.get(), MF_STRING, MenuLeaveSafeMode, text(L"Leave Safe Mode", L"Abgesicherten Modus verlassen"));
    } else {
        AppendMenuW(menu.get(), MF_STRING | (m_state.paused ? MF_CHECKED : 0), MenuPause, text(L"Pause Gestures", L"Gesten pausieren"));
        AppendMenuW(menu.get(), MF_STRING, MenuBringBack, text(L"Bring All Windows Back", L"Alle Fenster zurückholen"));
    }
    AppendMenuW(menu.get(), MF_STRING, MenuSettings, text(L"Settings…", L"Einstellungen …"));
    AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu.get(), MF_STRING, MenuQuit, text(L"Quit", L"Beenden"));

    POINT cursor;
    GetCursorPos(&cursor);
    // Needed so that the menu closes when clicking elsewhere.
    SetForegroundWindow(m_window.get());
    const UINT chosen = TrackPopupMenuEx(menu.get(), TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, cursor.x, cursor.y, m_window.get(), nullptr);
    PostMessageW(m_window.get(), WM_NULL, 0, 0);
    if (!m_handler) {
        return;
    }
    switch (chosen) {
    case MenuPause:
        m_handler(Action::TogglePause);
        break;
    case MenuBringBack:
        m_handler(Action::BringWindowsBack);
        break;
    case MenuSettings:
        m_handler(Action::Settings);
        break;
    case MenuLeaveSafeMode:
        m_handler(Action::LeaveSafeMode);
        break;
    case MenuQuit:
        m_handler(Action::Quit);
        break;
    default:
        break;
    }
}

LRESULT Tray::handle(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == TrayMessage) {
        switch (LOWORD(lParam)) {
        case WM_CONTEXTMENU:
        case WM_RBUTTONUP:
            showMenu();
            return 0;
        case NIN_SELECT:
        case NIN_KEYSELECT:
        case WM_LBUTTONDBLCLK:
            if (m_handler) {
                m_handler(Action::Settings);
            }
            return 0;
        default:
            return 0;
        }
    }
    if (m_taskbarCreated && message == m_taskbarCreated) {
        m_added = false;
        addIcon();
        return 0;
    }
    return DefWindowProcW(m_window.get(), message, wParam, lParam);
}

LRESULT CALLBACK Tray::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Tray *self = windowObject<Tray>(hwnd, message, lParam);
    if (!self || !self->m_window) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return self->handle(message, wParam, lParam);
}

} // namespace Wid
