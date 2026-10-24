/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "markerlayout.h"

#include <QLineF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Wid
{

QList<MarkerPlace> layoutMarkers(const QRectF &area, const QList<QRectF> &windows, qreal size, qreal margin)
{
    QList<MarkerPlace> markers;
    const qreal inset = size / 2.0 + margin;
    const QRectF inner = area.adjusted(inset, inset, -inset, -inset);
    if (inner.width() < size || inner.height() < size) {
        return markers;
    }
    const QPointF center = inner.center();
    for (int i = 0; i < windows.size(); ++i) {
        const QRectF &visual = windows[i];
        if (visual.isEmpty() || visual.intersects(area)) {
            continue;
        }
        const QPointF direction = visual.center() - center;
        qreal t = std::numeric_limits<qreal>::max();
        if (direction.x() > 0) {
            t = std::min(t, (inner.right() - center.x()) / direction.x());
        } else if (direction.x() < 0) {
            t = std::min(t, (inner.left() - center.x()) / direction.x());
        }
        if (direction.y() > 0) {
            t = std::min(t, (inner.bottom() - center.y()) / direction.y());
        } else if (direction.y() < 0) {
            t = std::min(t, (inner.top() - center.y()) / direction.y());
        }
        const qreal length = std::hypot(direction.x(), direction.y());
        if (!std::isfinite(t) || length <= 0) {
            continue;
        }
        markers.append(MarkerPlace{i, center + direction * t, direction / length});
    }
    std::sort(markers.begin(), markers.end(), [](const MarkerPlace &a, const MarkerPlace &b) {
        return std::atan2(a.direction.y(), a.direction.x()) < std::atan2(b.direction.y(), b.direction.x());
    });
    for (int i = 1; i < markers.size(); ++i) {
        MarkerPlace &marker = markers[i];
        const bool vertical = std::abs(marker.position.x() - inner.left()) < 0.5 || std::abs(marker.position.x() - inner.right()) < 0.5;
        const QPointF tangent = vertical ? QPointF(0, 1) : QPointF(1, 0);
        for (int attempt = 0; attempt < 12; ++attempt) {
            const bool overlaps = std::any_of(markers.cbegin(), markers.cbegin() + i, [&](const MarkerPlace &other) {
                return QLineF(other.position, marker.position).length() < size + 4.0;
            });
            if (!overlaps) {
                break;
            }
            marker.position += tangent * (size + 4.0);
            marker.position.setX(std::clamp(marker.position.x(), inner.left(), inner.right()));
            marker.position.setY(std::clamp(marker.position.y(), inner.top(), inner.bottom()));
        }
    }
    return markers;
}

} // namespace Wid
