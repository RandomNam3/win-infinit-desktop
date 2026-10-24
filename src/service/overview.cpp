/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "overview.h"
#include "texts.h"

#include <QLineF>

#include <windowsx.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace Wid
{

namespace
{

const wchar_t *const s_windowClass = L"WinInfinitDesktopOverview";
constexpr COLORREF Background = RGB(24, 26, 30);
constexpr BYTE WallpaperOpacity = 90;
constexpr qreal TitleHeight = 32; ///< canvas pixels, scaled with the zoom
constexpr qreal EdgeSize = 6; ///< screen pixels
constexpr qreal ClickDistance = 4;

enum MenuId : UINT {
    MenuZoomReset = 1,
    MenuShowAll,
    MenuSettings,
    MenuQuit,
};

RECT toRect(const QRectF &rect, const RECT &origin)
{
    return RECT{LONG(std::lround(rect.left() - origin.left)), LONG(std::lround(rect.top() - origin.top)), LONG(std::lround(rect.right() - origin.left)),
                LONG(std::lround(rect.bottom() - origin.top))};
}

bool sameRect(const RECT &a, const RECT &b)
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

std::chrono::microseconds now()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
}

} // namespace

Overview::Overview(OverviewHost *host, int monitor)
    : m_host(host)
    , m_monitor(monitor)
{
}

Overview::~Overview()
{
    m_thumbnails.clear();
    m_wallpaper.reset();
    detachWindowObject(m_window.get());
}

void Overview::show(const RECT &area)
{
    if (!m_window) {
        // Double clicks are needed for the title bars.
        registerWindowClass(s_windowClass, &Overview::windowProc, CS_DBLCLKS);
        m_window.reset(CreateWindowExW(WS_EX_TOOLWINDOW, s_windowClass, L"WIN Infinit Desktop", WS_POPUP, area.left, area.top, area.right - area.left,
                                       area.bottom - area.top, nullptr, nullptr, GetModuleHandleW(nullptr), this));
        if (!m_window) {
            return;
        }
    }
    const bool moved = !sameRect(area, m_area);
    m_area = area;
    if (!m_visible || moved) {
        SetWindowPos(m_window.get(), HWND_TOP, area.left, area.top, area.right - area.left, area.bottom - area.top, SWP_SHOWWINDOW);
        SetForegroundWindow(m_window.get());
        m_visible = true;
        updateBackground();
    }
}

void Overview::hide()
{
    if (!m_visible) {
        return;
    }
    if (m_drag != Drag::None) {
        ReleaseCapture();
        m_drag = Drag::None;
    }
    // Free the DWM resources while hidden.
    m_thumbnails.clear();
    m_wallpaper.reset();
    m_items.clear();
    ShowWindow(m_window.get(), SW_HIDE);
    m_visible = false;
}

void Overview::updateBackground()
{
    if (!m_window) {
        return;
    }
    // The wallpaper, dimmed: the desktop window drawn with low opacity over the dark background.
    if (!m_wallpaper) {
        if (HWND desktop = FindWindowW(L"Progman", nullptr)) {
            HTHUMBNAIL thumbnail = nullptr;
            if (SUCCEEDED(DwmRegisterThumbnail(m_window.get(), desktop, &thumbnail))) {
                m_wallpaper.reset(thumbnail);
            }
        }
    }
    if (m_wallpaper) {
        RECT desktopRect = {};
        GetWindowRect(FindWindowW(L"Progman", nullptr), &desktopRect);
        DWM_THUMBNAIL_PROPERTIES properties = {};
        properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE | DWM_TNP_VISIBLE | DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;
        properties.rcDestination = RECT{0, 0, m_area.right - m_area.left, m_area.bottom - m_area.top};
        properties.rcSource = RECT{m_area.left - desktopRect.left, m_area.top - desktopRect.top, m_area.right - desktopRect.left, m_area.bottom - desktopRect.top};
        properties.fVisible = TRUE;
        properties.opacity = WallpaperOpacity;
        properties.fSourceClientAreaOnly = FALSE;
        DwmUpdateThumbnailProperties(m_wallpaper.get(), &properties);
    }
    InvalidateRect(m_window.get(), nullptr, FALSE);
}

void Overview::setGrid(const Grid &grid)
{
    const bool changed = grid.visible != m_grid.visible || std::abs(grid.spacing - m_grid.spacing) > 0.01 || (grid.origin - m_grid.origin).manhattanLength() > 0.5
        || grid.color != m_grid.color || std::abs(grid.opacity - m_grid.opacity) > 0.001;
    m_grid = grid;
    if (changed && m_window && m_visible) {
        InvalidateRect(m_window.get(), nullptr, FALSE);
    }
}

void Overview::setItems(const QList<OverviewItem> &items, qreal zoom)
{
    if (!m_window || !m_visible) {
        return;
    }
    m_items = items;
    m_zoom = zoom;
    // Same windows in the same order? Then only the rects change.
    bool sameOrder = m_thumbnails.size() == std::size_t(items.size());
    for (std::size_t i = 0; sameOrder && i < m_thumbnails.size(); ++i) {
        sameOrder = m_thumbnails[i].window == items[int(i)].window;
    }
    if (!sameOrder) {
        m_thumbnails.clear();
        m_thumbnails.reserve(items.size());
        for (const OverviewItem &item : items) {
            Thumbnail thumbnail;
            thumbnail.window = item.window;
            HTHUMBNAIL handle = nullptr;
            if (IsWindow(item.window) && SUCCEEDED(DwmRegisterThumbnail(m_window.get(), item.window, &handle))) {
                thumbnail.handle.reset(handle);
            }
            thumbnail.destination = RECT{-1, -1, -1, -1};
            m_thumbnails.push_back(std::move(thumbnail));
        }
    }
    for (int i = 0; i < items.size(); ++i) {
        Thumbnail &thumbnail = m_thumbnails[std::size_t(i)];
        if (!thumbnail.handle) {
            continue;
        }
        const RECT destination = toRect(items[i].visual, m_area);
        if (sameRect(destination, thumbnail.destination) && sameRect(items[i].source, thumbnail.source)) {
            continue;
        }
        thumbnail.destination = destination;
        thumbnail.source = items[i].source;
        DWM_THUMBNAIL_PROPERTIES properties = {};
        properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE | DWM_TNP_VISIBLE | DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;
        properties.rcDestination = destination;
        properties.rcSource = items[i].source;
        properties.fVisible = TRUE;
        properties.opacity = 255;
        properties.fSourceClientAreaOnly = FALSE;
        DwmUpdateThumbnailProperties(thumbnail.handle.get(), &properties);
    }
}

void Overview::removeWindow(HWND window)
{
    m_thumbnails.erase(std::remove_if(m_thumbnails.begin(), m_thumbnails.end(),
                                      [window](const Thumbnail &thumbnail) {
                                          return thumbnail.window == window;
                                      }),
                       m_thumbnails.end());
    m_items.removeIf([window](const OverviewItem &item) {
        return item.window == window;
    });
    if (m_dragWindow == window) {
        m_dragWindow = nullptr;
        if (m_drag == Drag::Move || m_drag == Drag::Resize || m_drag == Drag::Press) {
            m_drag = Drag::None;
            ReleaseCapture();
        }
    }
}

void Overview::paint()
{
    PAINTSTRUCT paint;
    HDC dc = BeginPaint(m_window.get(), &paint);
    RECT client;
    GetClientRect(m_window.get(), &client);
    HBRUSH background = CreateSolidBrush(Background);
    FillRect(dc, &paint.rcPaint, background);
    DeleteObject(background);
    if (m_grid.visible && m_grid.opacity > 0 && m_grid.spacing > 0) {
        // Too dense grids are thinned out instead of turning into a grey surface.
        qreal spacing = m_grid.spacing;
        while (spacing < 12) {
            spacing *= 2;
        }
        const qreal alpha = std::clamp(m_grid.opacity, 0.0, 1.0);
        const auto mix = [alpha](int value, int base) {
            return int(base + (value - base) * alpha);
        };
        HBRUSH dot = CreateSolidBrush(
            RGB(mix(m_grid.color.red(), GetRValue(Background)), mix(m_grid.color.green(), GetGValue(Background)), mix(m_grid.color.blue(), GetBValue(Background))));
        const QPointF origin = m_grid.origin - QPointF(m_area.left, m_area.top);
        const qreal startX = origin.x() - std::ceil(origin.x() / spacing) * spacing;
        const qreal startY = origin.y() - std::ceil(origin.y() / spacing) * spacing;
        const int size = spacing >= 40 ? 3 : 2;
        for (qreal y = startY; y < client.bottom; y += spacing) {
            if (y + size < paint.rcPaint.top || y > paint.rcPaint.bottom) {
                continue;
            }
            for (qreal x = startX; x < client.right; x += spacing) {
                const RECT r = {LONG(x), LONG(y), LONG(x) + size, LONG(y) + size};
                FillRect(dc, &r, dot);
            }
        }
        DeleteObject(dot);
    }
    EndPaint(m_window.get(), &paint);
}

QPointF Overview::screenPoint(LPARAM lParam) const
{
    return QPointF(m_area.left + GET_X_LPARAM(lParam), m_area.top + GET_Y_LPARAM(lParam));
}

int Overview::itemAt(const QPointF &point) const
{
    for (int i = int(m_items.size()) - 1; i >= 0; --i) {
        if (m_items[i].visual.contains(point)) {
            return i;
        }
    }
    return -1;
}

unsigned Overview::edgesAt(int item, const QPointF &point) const
{
    if (item < 0 || !m_items[item].movable) {
        return 0;
    }
    const QRectF &rect = m_items[item].visual;
    unsigned edges = 0;
    if (point.x() < rect.left() + EdgeSize) {
        edges |= OverviewHost::EdgeLeft;
    } else if (point.x() >= rect.right() - EdgeSize) {
        edges |= OverviewHost::EdgeRight;
    }
    if (point.y() < rect.top() + EdgeSize) {
        edges |= OverviewHost::EdgeTop;
    } else if (point.y() >= rect.bottom() - EdgeSize) {
        edges |= OverviewHost::EdgeBottom;
    }
    return edges;
}

bool Overview::inTitle(int item, const QPointF &point) const
{
    if (item < 0) {
        return false;
    }
    const QRectF &rect = m_items[item].visual;
    return point.y() < rect.top() + std::max(8.0, TitleHeight * m_zoom);
}

void Overview::beginDrag(Drag drag, int item, const QPointF &point, unsigned edges)
{
    m_drag = drag;
    m_dragWindow = item >= 0 ? m_items[item].window : nullptr;
    m_dragEdges = edges;
    m_pressPoint = point;
    m_lastPoint = point;
    m_velocity.reset();
    m_velocity.addSample(point, now());
    SetCapture(m_window.get());
}

void Overview::endDrag(const QPointF &point, bool cancelled)
{
    const Drag drag = std::exchange(m_drag, Drag::None);
    const HWND window = std::exchange(m_dragWindow, nullptr);
    if (GetCapture() == m_window.get()) {
        ReleaseCapture();
    }
    if (cancelled) {
        return;
    }
    switch (drag) {
    case Drag::Pan:
        m_velocity.addSample(point, now());
        m_host->overviewPanEnd(m_monitor, m_velocity.flingVelocity(now()));
        break;
    case Drag::Move:
    case Drag::Resize:
        if (window) {
            m_host->overviewDragEnd(window);
        }
        break;
    case Drag::Press:
        if (window && QLineF(point, m_pressPoint).length() < ClickDistance) {
            m_host->overviewDive(window);
        }
        break;
    case Drag::None:
        break;
    }
}

void Overview::showMenu(const QPointF &point)
{
    UniqueMenu menu(CreatePopupMenu());
    if (!menu) {
        return;
    }
    AppendMenuW(menu.get(), MF_STRING, MenuZoomReset, text(L"Back to 100 %", L"Zurück auf 100 %"));
    AppendMenuW(menu.get(), MF_STRING, MenuShowAll, text(L"Show All Windows", L"Alle Fenster zeigen"));
    AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu.get(), MF_STRING, MenuSettings, text(L"Settings…", L"Einstellungen …"));
    AppendMenuW(menu.get(), MF_STRING, MenuQuit, text(L"Quit WIN Infinit Desktop", L"WIN Infinit Desktop beenden"));
    const UINT chosen = TrackPopupMenuEx(menu.get(), TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, int(point.x()), int(point.y()), m_window.get(), nullptr);
    switch (chosen) {
    case MenuZoomReset:
        m_host->overviewCommand(m_monitor, OverviewHost::Command::ZoomReset);
        break;
    case MenuShowAll:
        m_host->overviewCommand(m_monitor, OverviewHost::Command::ShowAll);
        break;
    case MenuSettings:
        m_host->overviewCommand(m_monitor, OverviewHost::Command::Settings);
        break;
    case MenuQuit:
        m_host->overviewCommand(m_monitor, OverviewHost::Command::Quit);
        break;
    default:
        break;
    }
}

LRESULT Overview::handle(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_PAINT:
        paint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR: {
        if (LOWORD(lParam) != HTCLIENT || m_drag != Drag::None) {
            break;
        }
        POINT cursor;
        GetCursorPos(&cursor);
        const QPointF point(cursor.x, cursor.y);
        const int item = itemAt(point);
        const unsigned edges = edgesAt(item, point);
        LPCWSTR shape = IDC_ARROW;
        if ((edges & (OverviewHost::EdgeLeft | OverviewHost::EdgeTop)) == (OverviewHost::EdgeLeft | OverviewHost::EdgeTop)
            || (edges & (OverviewHost::EdgeRight | OverviewHost::EdgeBottom)) == (OverviewHost::EdgeRight | OverviewHost::EdgeBottom)) {
            shape = IDC_SIZENWSE;
        } else if ((edges & (OverviewHost::EdgeRight | OverviewHost::EdgeTop)) == (OverviewHost::EdgeRight | OverviewHost::EdgeTop)
                   || (edges & (OverviewHost::EdgeLeft | OverviewHost::EdgeBottom)) == (OverviewHost::EdgeLeft | OverviewHost::EdgeBottom)) {
            shape = IDC_SIZENESW;
        } else if (edges & (OverviewHost::EdgeLeft | OverviewHost::EdgeRight)) {
            shape = IDC_SIZEWE;
        } else if (edges & (OverviewHost::EdgeTop | OverviewHost::EdgeBottom)) {
            shape = IDC_SIZENS;
        } else if (item >= 0 && inTitle(item, point) && m_items[item].movable) {
            shape = IDC_SIZEALL;
        }
        SetCursor(LoadCursorW(nullptr, shape));
        return TRUE;
    }
    case WM_LBUTTONDOWN: {
        const QPointF point = screenPoint(lParam);
        const int item = itemAt(point);
        const unsigned edges = edgesAt(item, point);
        if (item < 0) {
            beginDrag(Drag::Pan, -1, point);
        } else if (edges) {
            beginDrag(Drag::Resize, item, point, edges);
        } else if (inTitle(item, point) && m_items[item].movable) {
            beginDrag(Drag::Move, item, point);
        } else {
            beginDrag(Drag::Press, item, point);
        }
        return 0;
    }
    case WM_MBUTTONDOWN:
        beginDrag(Drag::Pan, -1, screenPoint(lParam));
        return 0;
    case WM_LBUTTONDBLCLK: {
        const QPointF point = screenPoint(lParam);
        const int item = itemAt(point);
        if (item >= 0) {
            m_host->overviewDive(m_items[item].window);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (m_drag == Drag::None) {
            return 0;
        }
        const QPointF point = screenPoint(lParam);
        const QPointF delta = point - m_lastPoint;
        m_lastPoint = point;
        m_velocity.addSample(point, now());
        if (delta.isNull()) {
            return 0;
        }
        if (m_drag == Drag::Press && QLineF(point, m_pressPoint).length() >= ClickDistance) {
            // Dragging on window content pans, like on empty space.
            m_drag = Drag::Pan;
            m_host->overviewPan(m_monitor, point - m_pressPoint);
            return 0;
        }
        switch (m_drag) {
        case Drag::Pan:
            m_host->overviewPan(m_monitor, delta);
            break;
        case Drag::Move:
            if (m_dragWindow) {
                m_host->overviewMoveWindow(m_dragWindow, delta);
            }
            break;
        case Drag::Resize:
            if (m_dragWindow) {
                m_host->overviewResizeWindow(m_dragWindow, m_dragEdges, delta);
            }
            break;
        default:
            break;
        }
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
        if (m_drag != Drag::None) {
            endDrag(screenPoint(lParam), false);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (m_drag != Drag::None && reinterpret_cast<HWND>(lParam) != m_window.get()) {
            endDrag(m_lastPoint, m_drag == Drag::Press);
        }
        return 0;
    case WM_MOUSEWHEEL: {
        POINT cursor = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        m_host->overviewZoom(m_monitor, QPointF(cursor.x, cursor.y), GET_WHEEL_DELTA_WPARAM(wParam) / 120.0);
        return 0;
    }
    case WM_RBUTTONUP: {
        const QPointF point = screenPoint(lParam);
        if (itemAt(point) < 0) {
            showMenu(point);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            m_host->overviewCommand(m_monitor, OverviewHost::Command::ZoomReset);
            return 0;
        }
        break;
    case WM_CLOSE:
        // Alt+F4 on the overview means "leave it", not "destroy it".
        m_host->overviewCommand(m_monitor, OverviewHost::Command::ZoomReset);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(m_window.get(), message, wParam, lParam);
}

LRESULT CALLBACK Overview::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Overview *self = windowObject<Overview>(hwnd, message, lParam);
    if (!self || !self->m_window) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return self->handle(message, wParam, lParam);
}

} // namespace Wid
