/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "velocitytracker.h"
#include "win32util.h"

#include <QColor>
#include <QList>
#include <QPointF>
#include <QRectF>

#include <vector>

namespace Wid
{

/// What the overview asks the canvas to do. All points are screen coordinates.
class OverviewHost
{
public:
    enum class Command {
        ZoomReset,
        ShowAll,
        Settings,
        Quit,
    };
    enum Edge : unsigned {
        EdgeLeft = 1,
        EdgeTop = 2,
        EdgeRight = 4,
        EdgeBottom = 8,
    };

    virtual ~OverviewHost() = default;
    virtual void overviewPan(int monitor, const QPointF &screenDelta) = 0;
    /// `velocity`: screen px/s of the release, null for no glide.
    virtual void overviewPanEnd(int monitor, const QPointF &velocity) = 0;
    virtual void overviewZoom(int monitor, const QPointF &anchor, qreal steps) = 0;
    /// Moves a window on the canvas by a screen delta (title bar drag).
    virtual void overviewMoveWindow(HWND window, const QPointF &screenDelta) = 0;
    /// Resizes from `edges` by a screen delta.
    virtual void overviewResizeWindow(HWND window, unsigned edges, const QPointF &screenDelta) = 0;
    virtual void overviewDragEnd(HWND window) = 0;
    /// Back to 100 % with this window in the middle, then activate it.
    virtual void overviewDive(HWND window) = 0;
    virtual void overviewCommand(int monitor, Command command) = 0;
};

/// One window in the overview, bottom to top.
struct OverviewItem
{
    HWND window = nullptr;
    QRectF visual; ///< where it is drawn, screen coordinates
    RECT source = {}; ///< the visible frame relative to the window rect
    bool movable = true; ///< canvas windows; pinned ones are only shown
};

/**
 * The view of one monitor while zoomed out: a tool window over the work area (tool windows
 * do not count as covering for Chromium's occlusion tracking, so the windows behind keep
 * drawing) with a dimmed wallpaper, the dot grid and live DWM thumbnails of the windows.
 *
 * Thumbnails stack in the order they are registered, so a changed stacking order registers
 * them again; otherwise only changed rects are sent to DWM. Nothing runs while it is hidden.
 */
class Overview
{
public:
    struct Grid
    {
        bool visible = true;
        qreal spacing = 64; ///< screen pixels between dots
        QPointF origin; ///< screen position of a dot (the canvas origin)
        QColor color = Qt::white;
        qreal opacity = 0.35;
    };

    Overview(OverviewHost *host, int monitor);
    ~Overview();
    Overview(const Overview &) = delete;
    Overview &operator=(const Overview &) = delete;

    void show(const RECT &area);
    void hide();
    bool isVisible() const
    {
        return m_visible;
    }
    HWND window() const
    {
        return m_window.get();
    }
    void setMonitor(int monitor)
    {
        m_monitor = monitor;
    }

    /// The windows, bottom to top, and the zoom (for the title bar height).
    void setItems(const QList<OverviewItem> &items, qreal zoom);
    void setGrid(const Grid &grid);
    /// Forgets the thumbnail of a window that is gone.
    void removeWindow(HWND window);

private:
    struct Thumbnail
    {
        HWND window = nullptr;
        UniqueThumbnail handle;
        RECT destination = {};
        RECT source = {};
    };
    enum class Drag {
        None,
        Pan,
        Move,
        Resize,
        Press, ///< on window content: a click dives, a drag pans
    };

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT message, WPARAM wParam, LPARAM lParam);
    void paint();
    void updateBackground();
    int itemAt(const QPointF &point) const;
    unsigned edgesAt(int item, const QPointF &point) const;
    bool inTitle(int item, const QPointF &point) const;
    QPointF screenPoint(LPARAM lParam) const;
    void showMenu(const QPointF &point);
    void beginDrag(Drag drag, int item, const QPointF &point, unsigned edges = 0);
    void endDrag(const QPointF &point, bool cancelled);

    OverviewHost *m_host;
    int m_monitor;
    UniqueWindow m_window;
    RECT m_area = {};
    bool m_visible = false;
    QList<OverviewItem> m_items;
    std::vector<Thumbnail> m_thumbnails;
    UniqueThumbnail m_wallpaper;
    qreal m_zoom = 1;
    Grid m_grid;

    Drag m_drag = Drag::None;
    HWND m_dragWindow = nullptr;
    unsigned m_dragEdges = 0;
    QPointF m_pressPoint;
    QPointF m_lastPoint;
    InfinitDesktop::VelocityTracker m_velocity;
};

} // namespace Wid
