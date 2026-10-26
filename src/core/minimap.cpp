/*
    KDE Infinite Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "minimap.h"

#include <algorithm>
#include <cmath>

namespace InfiniteDesktop
{

QPointF MapView::toScreen(const QPointF &canvasPoint) const
{
    return screen.topLeft() + (canvasPoint - canvas.topLeft()) * scale;
}

QRectF MapView::toScreen(const QRectF &canvasRect) const
{
    return QRectF(toScreen(canvasRect.topLeft()), canvasRect.size() * scale);
}

QPointF MapView::toCanvas(const QPointF &screenPoint) const
{
    if (scale <= 0) {
        return canvas.center();
    }
    return canvas.topLeft() + (screenPoint - screen.topLeft()) / scale;
}

MapView mapView(const QList<QRectF> &content, const QRectF &visible, const QRectF &available, qreal padding, qreal maxScale,
                const QSizeF &minimumCanvas)
{
    MapView view;
    const QRectF space = available.adjusted(padding, padding, -padding, -padding);
    if (space.width() <= 1 || space.height() <= 1) {
        return view;
    }
    QRectF shown = visible;
    for (const QRectF &rect : content) {
        if (rect.isValid() && std::isfinite(rect.x()) && std::isfinite(rect.y()) && std::isfinite(rect.width())
            && std::isfinite(rect.height())) {
            shown = shown.isNull() ? rect : shown.united(rect);
        }
    }
    if (shown.isNull()) {
        return view;
    }
    // A little air around the outermost things.
    const qreal margin = std::max(shown.width(), shown.height()) * 0.05;
    shown.adjust(-margin, -margin, margin, margin);
    if (shown.width() < minimumCanvas.width()) {
        const qreal grow = (minimumCanvas.width() - shown.width()) / 2;
        shown.adjust(-grow, 0, grow, 0);
    }
    if (shown.height() < minimumCanvas.height()) {
        const qreal grow = (minimumCanvas.height() - shown.height()) / 2;
        shown.adjust(0, -grow, 0, grow);
    }
    qreal scale = std::min(space.width() / shown.width(), space.height() / shown.height());
    scale = std::min(scale, maxScale);
    if (!(scale > 0) || !std::isfinite(scale)) {
        return view;
    }
    // Fill the space in the direction that has room, keep the content centred.
    const QSizeF canvasSize(space.width() / scale, space.height() / scale);
    view.canvas = QRectF(shown.center() - QPointF(canvasSize.width() / 2, canvasSize.height() / 2), canvasSize);
    view.screen = space;
    view.scale = scale;
    return view;
}

QPointF mapDragCenter(const MapView &view, const QPointF &centerAtStart, const QPointF &screenDelta)
{
    if (view.scale <= 0) {
        return centerAtStart;
    }
    return centerAtStart + screenDelta / view.scale;
}

} // namespace InfiniteDesktop
