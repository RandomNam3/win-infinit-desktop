/*
    KDE Infinite Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "camera.h"

#include <algorithm>
#include <cmath>

namespace InfiniteDesktop
{

Camera Camera::zoomedAt(const QPointF &screenAnchor, qreal newZoom, const QRectF &viewport) const
{
    // The canvas point under the anchor has to stay under the anchor:
    //   anchor = viewport.center() + (canvasPoint - newCenter) * newZoom
    const QPointF canvasPoint = screenToCanvas(screenAnchor, viewport);
    Camera result;
    result.zoom = newZoom;
    result.center = canvasPoint - (screenAnchor - viewport.center()) / newZoom;
    return result;
}

bool Camera::isUnitZoom() const
{
    return std::abs(zoom - 1.0) < 1e-6;
}

bool Camera::fuzzyEquals(const Camera &other, qreal positionEpsilon, qreal zoomEpsilon) const
{
    return std::abs(center.x() - other.center.x()) <= positionEpsilon
        && std::abs(center.y() - other.center.y()) <= positionEpsilon
        && std::abs(zoom - other.zoom) <= zoomEpsilon;
}

Camera interpolate(const Camera &from, const Camera &to, qreal t)
{
    t = std::clamp(t, 0.0, 1.0);
    Camera result;
    const qreal logFrom = std::log(from.zoom);
    const qreal logTo = std::log(to.zoom);
    result.zoom = std::exp(logFrom + (logTo - logFrom) * t);

    // Interpolate the center in screen space relative to the zoom, so that a
    // combined zoom-and-move does not overshoot: weight the center path by the
    // inverse zoom (the screen-space distance) instead of linearly.
    if (std::abs(logTo - logFrom) < 1e-6) {
        result.center = from.center + (to.center - from.center) * t;
    } else {
        const qreal inverseFrom = 1.0 / from.zoom;
        const qreal inverseTo = 1.0 / to.zoom;
        const qreal inverseNow = 1.0 / result.zoom;
        const qreal u = (inverseNow - inverseFrom) / (inverseTo - inverseFrom);
        const qreal weight = std::isfinite(u) ? std::clamp(u, 0.0, 1.0) : t;
        result.center = from.center + (to.center - from.center) * weight;
    }
    if (t >= 1.0) {
        result = to;
    }
    return result;
}

qreal clampZoom(qreal zoom, qreal minZoom, qreal maxZoom, qreal snapRange)
{
    if (!std::isfinite(zoom) || zoom <= 0.0) {
        return 1.0;
    }
    if (minZoom > maxZoom) {
        std::swap(minZoom, maxZoom);
    }
    zoom = std::clamp(zoom, minZoom, maxZoom);
    if (snapRange > 0.0 && std::abs(zoom - 1.0) <= snapRange && minZoom <= 1.0 && maxZoom >= 1.0) {
        zoom = 1.0;
    }
    return zoom;
}

QPointF snapToPixelGrid(const QPointF &point, qreal scale)
{
    if (scale <= 0.0 || !std::isfinite(scale)) {
        return QPointF(std::round(point.x()), std::round(point.y()));
    }
    return QPointF(std::round(point.x() * scale) / scale, std::round(point.y() * scale) / scale);
}

} // namespace InfiniteDesktop
