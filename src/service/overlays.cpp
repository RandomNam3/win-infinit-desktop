/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "overlays.h"
#include "layered.h"

#include <windowsx.h>

namespace Wid
{

namespace
{

const wchar_t *const s_windowClass = L"WinInfinitDesktopOverlay";

} // namespace

OverlayWindow::OverlayWindow(Style style, Handler handler)
    : m_style(style)
    , m_handler(std::move(handler))
{
}

OverlayWindow::~OverlayWindow()
{
    detachWindowObject(m_window.get());
}

bool OverlayWindow::create()
{
    if (m_window) {
        return true;
    }
    registerWindowClass(s_windowClass, &OverlayWindow::windowProc, 0, LoadCursorW(nullptr, IDC_ARROW));
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST;
    switch (m_style) {
    case Style::Veil:
        exStyle |= WS_EX_LAYERED;
        break;
    case Style::Tint:
    case Style::Label:
        // Clicks go to the window below (the veil while selecting).
        exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
        break;
    case Style::Panel:
        break;
    }
    m_window.reset(CreateWindowExW(exStyle, s_windowClass, L"WIN Infinit Desktop", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), this));
    if (m_window && (m_style == Style::Veil || m_style == Style::Tint)) {
        SetLayeredWindowAttributes(m_window.get(), 0, m_alpha, LWA_ALPHA);
    }
    return bool(m_window);
}

void OverlayWindow::show(const QRect &rect)
{
    if (!create() || rect.isEmpty()) {
        return;
    }
    if (m_visible && rect == m_rect) {
        return;
    }
    const bool resized = rect.size() != m_rect.size();
    m_rect = rect;
    if (m_style == Style::Label) {
        // The content decides the size; UpdateLayeredWindow moves it.
        if (!m_image.isNull()) {
            updateLayeredWindow(m_window.get(), m_image, POINT{rect.left(), rect.top()});
        }
        SetWindowPos(m_window.get(), HWND_TOPMOST, rect.left(), rect.top(), 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    } else {
        SetWindowPos(m_window.get(), HWND_TOPMOST, rect.left(), rect.top(), rect.width(), rect.height(), SWP_SHOWWINDOW | SWP_NOACTIVATE);
        if (resized) {
            InvalidateRect(m_window.get(), nullptr, FALSE);
        }
    }
    m_visible = true;
}

void OverlayWindow::hide()
{
    if (!m_window || !m_visible) {
        m_image = QImage();
        return;
    }
    if (GetCapture() == m_window.get()) {
        ReleaseCapture();
    }
    ShowWindow(m_window.get(), SW_HIDE);
    m_visible = false;
    m_image = QImage();
}

void OverlayWindow::setColor(COLORREF fill, COLORREF border, BYTE alpha)
{
    const bool changed = fill != m_fill || border != m_border;
    m_fill = fill;
    m_border = border;
    if (alpha != m_alpha || !m_window) {
        m_alpha = alpha;
        if (create()) {
            SetLayeredWindowAttributes(m_window.get(), 0, m_alpha, LWA_ALPHA);
        }
    }
    if (changed && m_window) {
        InvalidateRect(m_window.get(), nullptr, FALSE);
    }
}

void OverlayWindow::setImage(const QImage &image)
{
    if (!create()) {
        return;
    }
    if (m_style == Style::Label) {
        m_image = image;
        m_rect.setSize(image.size());
        if (m_visible && !image.isNull()) {
            updateLayeredWindow(m_window.get(), m_image, POINT{m_rect.left(), m_rect.top()});
        }
        return;
    }
    // Same layout as a top-down 32 bit DIB, painted as it is.
    m_image = image.format() == QImage::Format_RGB32 ? image : image.convertToFormat(QImage::Format_RGB32);
    if (m_visible) {
        InvalidateRect(m_window.get(), nullptr, FALSE);
    }
}

QPoint OverlayWindow::screenPoint(LPARAM lParam) const
{
    return QPoint(m_rect.left() + GET_X_LPARAM(lParam), m_rect.top() + GET_Y_LPARAM(lParam));
}

void OverlayWindow::paint()
{
    PAINTSTRUCT paint;
    HDC dc = BeginPaint(m_window.get(), &paint);
    RECT client;
    GetClientRect(m_window.get(), &client);
    switch (m_style) {
    case Style::Veil:
    case Style::Tint: {
        UniqueBrush fill(CreateSolidBrush(m_fill));
        FillRect(dc, &paint.rcPaint, fill.get());
        if (m_style == Style::Tint && client.right > 4 && client.bottom > 4) {
            UniqueBrush border(CreateSolidBrush(m_border));
            const RECT edges[] = {
                {0, 0, client.right, 2},
                {0, client.bottom - 2, client.right, client.bottom},
                {0, 2, 2, client.bottom - 2},
                {client.right - 2, 2, client.right, client.bottom - 2},
            };
            for (const RECT &edge : edges) {
                FillRect(dc, &edge, border.get());
            }
        }
        break;
    }
    case Style::Panel:
        if (!m_image.isNull()) {
            BITMAPINFO info = {};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = m_image.width();
            info.bmiHeader.biHeight = -m_image.height();
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            SetDIBitsToDevice(dc, 0, 0, DWORD(m_image.width()), DWORD(m_image.height()), 0, 0, 0, UINT(m_image.height()), m_image.constBits(), &info,
                              DIB_RGB_COLORS);
        } else {
            UniqueBrush fill(CreateSolidBrush(RGB(32, 34, 38)));
            FillRect(dc, &paint.rcPaint, fill.get());
        }
        break;
    case Style::Label:
        break;
    }
    EndPaint(m_window.get(), &paint);
}

LRESULT OverlayWindow::handle(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_MOUSEACTIVATE) {
        return MA_NOACTIVATE;
    }
    if (m_handler && m_handler(message, wParam, lParam)) {
        return 0;
    }
    switch (message) {
    case WM_PAINT:
        paint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        // Alt+F4 never destroys it; the owner hides it.
        return 0;
    default:
        break;
    }
    return DefWindowProcW(m_window.get(), message, wParam, lParam);
}

LRESULT CALLBACK OverlayWindow::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    OverlayWindow *self = windowObject<OverlayWindow>(hwnd, message, lParam);
    if (!self || !self->m_window) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return self->handle(message, wParam, lParam);
}

} // namespace Wid
