/*
    KDE Infinite Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "camera.h"

#include <QList>
#include <QRectF>

namespace InfiniteDesktop
{

enum class Direction {
    Left,
    Right,
    Up,
    Down,
};

enum class RevealMode {
    Minimal,
    Center,
};

/**
 * Finds the candidate (canvas rects) that is the best neighbour of @p from in
 * @p direction. Candidates must lie in the half plane of the direction; the
 * score prefers small distances along the direction and penalises sideways
 * offsets. Returns the index into @p candidates or -1.
 */
int findNeighbor(const QRectF &from, const QList<QRectF> &candidates, Direction direction);

QRectF boundingRect(const QList<QRectF> &rects);

/**
 * Camera that shows @p canvasRect completely inside @p area (screen rect, e.g.
 * the work area without panels) of the output @p viewport, keeping
 * @p margin screen pixels free around it. The zoom is limited to
 * [@p minZoom, @p maxZoom].
 */
Camera fitCamera(const QRectF &canvasRect, const QRectF &viewport, const QRectF &area, qreal margin, qreal minZoom, qreal maxZoom);

/**
 * Camera with @p zoom that brings @p canvasRect into @p area.
 * Minimal: move the camera as little as possible so the rect is fully visible
 * (if it is larger than the area, its top left corner is shown).
 * Center: center the rect in the area.
 */
Camera revealCamera(const Camera &current, const QRectF &canvasRect, const QRectF &viewport, const QRectF &area, RevealMode mode, qreal margin, qreal zoom = 1.0);

/// True if @p canvasRect seen through @p camera is fully inside @p area.
bool isFullyVisible(const Camera &camera, const QRectF &canvasRect, const QRectF &viewport, const QRectF &area);

/**
 * Picks a free spot for a tile of @p size near @p preferredCenter (canvas
 * coordinates) that does not overlap any of @p occupied. Tries a spiral of
 * positions around the preferred center.
 */
QPointF findFreeTileSpot(const QPointF &preferredCenter, const QSizeF &size, const QList<QRectF> &occupied, qreal gap);

} // namespace InfiniteDesktop
