/*
    KDE Infinite Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "navigation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace InfiniteDesktop
{

int findNeighbor(const QRectF &from, const QList<QRectF> &candidates, Direction direction)
{
    const QPointF origin = from.center();
    int best = -1;
    qreal bestScore = std::numeric_limits<qreal>::max();

    for (int i = 0; i < candidates.size(); ++i) {
        const QRectF &candidate = candidates[i];
        if (candidate == from) {
            continue;
        }
        const QPointF delta = candidate.center() - origin;
        qreal along = 0;
        qreal across = 0;
        // Edge based test: the candidate has to start beyond the middle of the
        // current window in the requested direction.
        bool inDirection = false;
        switch (direction) {
        case Direction::Left:
            along = -delta.x();
            across = delta.y();
            inDirection = candidate.center().x() < origin.x() - 1;
            break;
        case Direction::Right:
            along = delta.x();
            across = delta.y();
            inDirection = candidate.center().x() > origin.x() + 1;
            break;
        case Direction::Up:
            along = -delta.y();
            across = delta.x();
            inDirection = candidate.center().y() < origin.y() - 1;
            break;
        case Direction::Down:
            along = delta.y();
            across = delta.x();
            inDirection = candidate.center().y() > origin.y() + 1;
            break;
        }
        if (!inDirection || along <= 0) {
            continue;
        }
        // Candidates in a 45 degree cone are preferred strongly; outside of it they
        // are still reachable but heavily penalised.
        const qreal coneFactor = std::abs(across) > along ? 4.0 : 1.5;
        const qreal score = along + std::abs(across) * coneFactor;
        if (score < bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

QRectF boundingRect(const QList<QRectF> &rects)
{
    QRectF result;
    for (const QRectF &rect : rects) {
        result = result.isNull() ? rect : result.united(rect);
    }
    return result;
}

Camera fitCamera(const QRectF &canvasRect, const QRectF &viewport, const QRectF &area, qreal margin, qreal minZoom, qreal maxZoom)
{
    const QRectF usable = area.adjusted(margin, margin, -margin, -margin);
    qreal zoom = 1.0;
    if (canvasRect.width() > 0 && canvasRect.height() > 0 && usable.width() > 0 && usable.height() > 0) {
        zoom = std::min(usable.width() / canvasRect.width(), usable.height() / canvasRect.height());
    }
    zoom = std::clamp(zoom, std::min(minZoom, maxZoom), std::max(minZoom, maxZoom));
    Camera camera;
    camera.zoom = zoom;
    // canvasToScreen(rect.center) == area.center()
    camera.center = canvasRect.center() - (area.center() - viewport.center()) / zoom;
    return camera;
}

Camera revealCamera(const Camera &current, const QRectF &canvasRect, const QRectF &viewport, const QRectF &area, RevealMode mode, qreal margin, qreal zoom)
{
    Camera camera{current.center, zoom};
    if (mode == RevealMode::Center) {
        camera.center = canvasRect.center() - (area.center() - viewport.center()) / zoom;
        return camera;
    }

    const QRectF usable = area.adjusted(margin, margin, -margin, -margin);
    const QRectF screen = camera.canvasToScreen(canvasRect, viewport);
    qreal dx = 0;
    qreal dy = 0;
    if (screen.width() > usable.width()) {
        dx = usable.left() - screen.left();
    } else if (screen.left() < usable.left()) {
        dx = usable.left() - screen.left();
    } else if (screen.right() > usable.right()) {
        dx = usable.right() - screen.right();
    }
    if (screen.height() > usable.height()) {
        dy = usable.top() - screen.top();
    } else if (screen.top() < usable.top()) {
        dy = usable.top() - screen.top();
    } else if (screen.bottom() > usable.bottom()) {
        dy = usable.bottom() - screen.bottom();
    }
    // Moving the content by (dx, dy) on screen moves the camera the other way.
    camera.center -= QPointF(dx, dy) / zoom;
    return camera;
}

bool isFullyVisible(const Camera &camera, const QRectF &canvasRect, const QRectF &viewport, const QRectF &area)
{
    const QRectF screen = camera.canvasToScreen(canvasRect, viewport);
    // Allow sub pixel differences caused by rounding of window positions.
    return area.adjusted(-1, -1, 1, 1).contains(screen);
}

QPointF findFreeTileSpot(const QPointF &preferredCenter, const QSizeF &size, const QList<QRectF> &occupied, qreal gap)
{
    const auto isFree = [&](const QRectF &rect) {
        const QRectF padded = rect.adjusted(-gap / 2, -gap / 2, gap / 2, gap / 2);
        return std::none_of(occupied.cbegin(), occupied.cend(), [&padded](const QRectF &other) {
            return other.intersects(padded);
        });
    };

    QRectF rect(QPointF(preferredCenter.x() - size.width() / 2, preferredCenter.y() - size.height() / 2), size);
    if (isFree(rect)) {
        return rect.topLeft();
    }
    // Walk outwards on rings of increasing radius.
    const qreal step = std::max(size.width(), size.height()) + gap;
    for (int ring = 1; ring <= 12; ++ring) {
        const int samples = 8 * ring;
        for (int i = 0; i < samples; ++i) {
            const qreal angle = 2.0 * std::numbers::pi * i / samples;
            const QPointF center = preferredCenter + QPointF(std::cos(angle), std::sin(angle)) * step * ring;
            rect.moveCenter(center);
            if (isFree(rect)) {
                return rect.topLeft();
            }
        }
    }
    return QPointF(preferredCenter.x() - size.width() / 2, preferredCenter.y() - size.height() / 2);
}

} // namespace InfiniteDesktop
