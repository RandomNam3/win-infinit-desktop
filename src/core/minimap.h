/*
    KDE Infinit Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QList>
#include <QRectF>

namespace InfinitDesktop
{

/**
 * The map of a canvas: a uniformly scaled picture of everything on it (windows, groups,
 * areas, notes) and of the part the camera shows, drawn into a rect of the screen.
 */
struct MapView
{
    QRectF canvas; ///< part of the canvas the map shows
    QRectF screen; ///< where the map is drawn, same aspect ratio as `canvas`
    qreal scale = 1; ///< screen pixels per canvas pixel

    QPointF toScreen(const QPointF &canvasPoint) const;
    QRectF toScreen(const QRectF &canvasRect) const;
    QPointF toCanvas(const QPointF &screenPoint) const;
    bool isValid() const
    {
        return scale > 0 && !screen.isEmpty();
    }
};

/**
 * Map that shows all `content` and `visible` (the camera's rect) inside `available`
 * (screen rect), with `padding` screen pixels around and at most `maxScale`. The shown
 * part is at least `minimumCanvas` wide and high, so a nearly empty canvas does not
 * become a huge zoom.
 */
MapView mapView(const QList<QRectF> &content, const QRectF &visible, const QRectF &available, qreal padding, qreal maxScale,
                const QSizeF &minimumCanvas);

/// Camera centre after dragging the visible rect on the map by `screenDelta`.
QPointF mapDragCenter(const MapView &view, const QPointF &centerAtStart, const QPointF &screenDelta);

} // namespace InfinitDesktop
