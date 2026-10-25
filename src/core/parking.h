/*
    KDE Infinit Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>

namespace InfinitDesktop
{

/**
 * Index of the output closest to @p point, using exactly the same metric as
 * KWin::Workspace::outputAt() (squared distance to the clamped point).
 * Returns -1 for an empty list.
 */
int nearestOutputIndex(const QList<QRectF> &outputs, const QPointF &point);

struct ParkingSpot
{
    QPointF topLeft;
    /// True if KWin will consider the owner the window's output at this spot.
    /// Only then the window keeps receiving frame callbacks from its own screen.
    bool ownedByOwner = false;
};

/**
 * Computes where a window of @p size that belongs to output @p ownerIndex can
 * be "parked" while it is not visible: a position in the void between/around
 * the outputs that does not intersect any output (so it can never receive
 * input), whose center is still closest to the owner output.
 */
ParkingSpot computeParkingSpot(const QList<QRectF> &outputs, int ownerIndex, const QSizeF &size, qreal gap = 64.0);

/// True if @p rect intersects any of @p outputs other than @p exceptIndex.
bool intersectsOtherOutput(const QList<QRectF> &outputs, int exceptIndex, const QRectF &rect);

} // namespace InfinitDesktop
