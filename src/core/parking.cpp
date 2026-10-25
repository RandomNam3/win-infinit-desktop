/*
    KDE Infinit Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "parking.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace InfinitDesktop
{

int nearestOutputIndex(const QList<QRectF> &outputs, const QPointF &point)
{
    int best = -1;
    qreal bestDistance = std::numeric_limits<qreal>::max();
    for (int i = 0; i < outputs.size(); ++i) {
        const QRectF &geo = outputs[i];
        // Mirrors Workspace::outputAt(): clamp into [x, x + width - 1].
        const QPointF closest(std::clamp(point.x(), geo.x(), geo.x() + geo.width() - 1),
                              std::clamp(point.y(), geo.y(), geo.y() + geo.height() - 1));
        const QPointF ray = closest - point;
        const qreal distance = ray.x() * ray.x() + ray.y() * ray.y();
        if (best < 0 || distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

bool intersectsOtherOutput(const QList<QRectF> &outputs, int exceptIndex, const QRectF &rect)
{
    for (int i = 0; i < outputs.size(); ++i) {
        if (i != exceptIndex && outputs[i].intersects(rect)) {
            return true;
        }
    }
    return false;
}

namespace
{

bool intersectsAnyOutput(const QList<QRectF> &outputs, const QRectF &rect)
{
    return std::any_of(outputs.cbegin(), outputs.cend(), [&rect](const QRectF &output) {
        return output.intersects(rect);
    });
}

enum class Side {
    Below,
    Right,
    Left,
    Above,
};

QRectF initialCandidate(const QRectF &owner, const QSizeF &size, Side side, qreal gap)
{
    switch (side) {
    case Side::Below:
        return QRectF(QPointF(owner.center().x() - size.width() / 2, owner.bottom() + gap), size);
    case Side::Above:
        return QRectF(QPointF(owner.center().x() - size.width() / 2, owner.top() - gap - size.height()), size);
    case Side::Right:
        return QRectF(QPointF(owner.right() + gap, owner.center().y() - size.height() / 2), size);
    case Side::Left:
        return QRectF(QPointF(owner.left() - gap - size.width(), owner.center().y() - size.height() / 2), size);
    }
    return QRectF();
}

// Pushes the candidate further away from the owner until it does not intersect any output.
QRectF pushOut(const QList<QRectF> &outputs, QRectF candidate, Side side, qreal gap)
{
    for (int iteration = 0; iteration < 64; ++iteration) {
        const auto blocking = std::find_if(outputs.cbegin(), outputs.cend(), [&candidate](const QRectF &output) {
            return output.intersects(candidate);
        });
        if (blocking == outputs.cend()) {
            return candidate;
        }
        switch (side) {
        case Side::Below:
            candidate.moveTop(blocking->bottom() + gap);
            break;
        case Side::Above:
            candidate.moveBottom(blocking->top() - gap);
            break;
        case Side::Right:
            candidate.moveLeft(blocking->right() + gap);
            break;
        case Side::Left:
            candidate.moveRight(blocking->left() - gap);
            break;
        }
    }
    return candidate;
}

qreal distanceToRect(const QRectF &rect, const QPointF &point)
{
    const qreal dx = std::max({rect.left() - point.x(), 0.0, point.x() - rect.right()});
    const qreal dy = std::max({rect.top() - point.y(), 0.0, point.y() - rect.bottom()});
    return std::hypot(dx, dy);
}

} // namespace

ParkingSpot computeParkingSpot(const QList<QRectF> &outputs, int ownerIndex, const QSizeF &size, qreal gap)
{
    if (outputs.isEmpty() || ownerIndex < 0 || ownerIndex >= outputs.size()) {
        return ParkingSpot{QPointF(-1000000, -1000000), false};
    }
    const QRectF owner = outputs[ownerIndex];
    const QSizeF safeSize(std::max<qreal>(size.width(), 1), std::max<qreal>(size.height(), 1));

    ParkingSpot best;
    qreal bestScore = std::numeric_limits<qreal>::max();
    bool haveBest = false;

    for (Side side : {Side::Below, Side::Right, Side::Left, Side::Above}) {
        const QRectF candidate = pushOut(outputs, initialCandidate(owner, safeSize, side, gap), side, gap);
        if (intersectsAnyOutput(outputs, candidate)) {
            continue;
        }
        const bool owned = nearestOutputIndex(outputs, candidate.center()) == ownerIndex;
        // Prefer spots owned by the owner output, then the ones closest to it.
        const qreal score = (owned ? 0.0 : 1e9) + distanceToRect(owner, candidate.center());
        if (!haveBest || score < bestScore) {
            best = ParkingSpot{candidate.topLeft(), owned};
            bestScore = score;
            haveBest = true;
        }
    }

    if (!haveBest) {
        // Pathological layout: go far beyond the bounding box of all outputs.
        QRectF bounds;
        for (const QRectF &output : outputs) {
            bounds = bounds.united(output);
        }
        const QRectF candidate(QPointF(owner.center().x() - safeSize.width() / 2, bounds.bottom() + gap * 4), safeSize);
        best = ParkingSpot{candidate.topLeft(), nearestOutputIndex(outputs, candidate.center()) == ownerIndex};
    }

    best.topLeft = QPointF(std::round(best.topLeft.x()), std::round(best.topLeft.y()));
    return best;
}

} // namespace InfinitDesktop
