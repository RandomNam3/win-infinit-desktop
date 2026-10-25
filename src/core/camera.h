/*
    KDE Infinit Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>

namespace InfinitDesktop
{

/**
 * A camera looking at an infinite 2D canvas.
 *
 * The canvas uses its own coordinate system (logical pixels at zoom 1.0).
 * @c center is the canvas point shown in the middle of the viewport, @c zoom
 * the scale factor from canvas to screen. All screen coordinates are KWin
 * global logical coordinates, @p viewport is the geometry of the output.
 */
struct Camera
{
    QPointF center;
    qreal zoom = 1.0;

    QPointF canvasToScreen(const QPointF &canvas, const QRectF &viewport) const
    {
        return viewport.center() + (canvas - center) * zoom;
    }

    QPointF screenToCanvas(const QPointF &screen, const QRectF &viewport) const
    {
        return center + (screen - viewport.center()) / zoom;
    }

    QRectF canvasToScreen(const QRectF &canvas, const QRectF &viewport) const
    {
        return QRectF(canvasToScreen(canvas.topLeft(), viewport), canvas.size() * zoom);
    }

    QRectF screenToCanvas(const QRectF &screen, const QRectF &viewport) const
    {
        return QRectF(screenToCanvas(screen.topLeft(), viewport), screen.size() / zoom);
    }

    /// The part of the canvas that is visible through @p viewport.
    QRectF visibleCanvasRect(const QRectF &viewport) const
    {
        return screenToCanvas(viewport, viewport);
    }

    /// Returns a camera with @p newZoom that keeps the canvas point under @p screenAnchor fixed.
    Camera zoomedAt(const QPointF &screenAnchor, qreal newZoom, const QRectF &viewport) const;

    /// Returns a camera that is moved by @p screenDelta (in screen pixels, content follows the delta).
    Camera pannedByScreenDelta(const QPointF &screenDelta) const
    {
        return Camera{center - screenDelta / zoom, zoom};
    }

    bool isUnitZoom() const;
    bool fuzzyEquals(const Camera &other, qreal positionEpsilon = 0.01, qreal zoomEpsilon = 1e-4) const;

    friend bool operator==(const Camera &a, const Camera &b)
    {
        return a.center == b.center && a.zoom == b.zoom;
    }
};

/// Interpolates between two cameras. Zoom is interpolated logarithmically so that
/// zoom transitions feel linear to the eye.
Camera interpolate(const Camera &from, const Camera &to, qreal t);

/// Clamps @p zoom into [minZoom, maxZoom] and snaps values close to 1.0 onto exactly 1.0.
qreal clampZoom(qreal zoom, qreal minZoom, qreal maxZoom, qreal snapRange = 0.02);

/// Rounds @p point to the closest point on the device pixel grid of an output with @p scale.
QPointF snapToPixelGrid(const QPointF &point, qreal scale);

} // namespace InfinitDesktop
