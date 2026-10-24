/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <windows.h>

#include <dwmapi.h>

#include <utility>

namespace Wid
{

/**
 * Owns one Win32 resource and releases it exactly once. Not copyable, movable.
 * Release(handle) is called for every non-null handle.
 */
template<typename Handle, auto Release>
class Unique
{
public:
    Unique() = default;
    explicit Unique(Handle handle)
        : m_handle(handle)
    {
    }
    ~Unique()
    {
        reset();
    }
    Unique(const Unique &) = delete;
    Unique &operator=(const Unique &) = delete;
    Unique(Unique &&other) noexcept
        : m_handle(std::exchange(other.m_handle, Handle{}))
    {
    }
    Unique &operator=(Unique &&other) noexcept
    {
        if (this != &other) {
            reset(std::exchange(other.m_handle, Handle{}));
        }
        return *this;
    }

    Handle get() const
    {
        return m_handle;
    }
    explicit operator bool() const
    {
        return m_handle != Handle{};
    }
    Handle release()
    {
        return std::exchange(m_handle, Handle{});
    }
    void reset(Handle handle = Handle{})
    {
        const Handle old = std::exchange(m_handle, handle);
        if (old != Handle{}) {
            Release(old);
        }
    }

private:
    Handle m_handle{};
};

namespace Release
{
inline void window(HWND hwnd)
{
    DestroyWindow(hwnd);
}
inline void icon(HICON icon)
{
    DestroyIcon(icon);
}
inline void gdiObject(HGDIOBJ object)
{
    DeleteObject(object);
}
inline void thumbnail(HTHUMBNAIL thumbnail)
{
    DwmUnregisterThumbnail(thumbnail);
}
inline void kernelHandle(HANDLE handle)
{
    CloseHandle(handle);
}
inline void memoryDc(HDC dc)
{
    DeleteDC(dc);
}
inline void menu(HMENU menu)
{
    DestroyMenu(menu);
}
} // namespace Release

using UniqueWindow = Unique<HWND, Release::window>;
using UniqueIcon = Unique<HICON, Release::icon>;
using UniqueBitmap = Unique<HBITMAP, Release::gdiObject>;
using UniqueThumbnail = Unique<HTHUMBNAIL, Release::thumbnail>;
using UniqueHandle = Unique<HANDLE, Release::kernelHandle>;
using UniqueMemoryDc = Unique<HDC, Release::memoryDc>;
using UniqueMenu = Unique<HMENU, Release::menu>;

/// Registers a window class once per process; returns its atom (0 on failure).
ATOM registerWindowClass(const wchar_t *name, WNDPROC proc, UINT style = 0, HCURSOR cursor = nullptr, HICON icon = nullptr);

/// Stores `object` as the window's user data at WM_NCCREATE (pass it as lpParam to
/// CreateWindowEx) and returns it for every later message; nullptr before.
template<typename T>
T *windowObject(HWND hwnd, UINT message, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return static_cast<T *>(create->lpCreateParams);
    }
    return reinterpret_cast<T *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

/// Clears the user data, so that messages after the owner's destruction find no object.
inline void detachWindowObject(HWND hwnd)
{
    if (hwnd) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    }
}

} // namespace Wid
