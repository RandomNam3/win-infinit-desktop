/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QList>
#include <QPointF>
#include <QRectF>

namespace Wid
{

struct MarkerPlace
{
    int window = -1; ///< index into the windows passed in
    QPointF position; ///< centre of the marker, screen coordinates
    QPointF direction; ///< unit vector towards the window
};

/**
 * Markers for windows outside `area` (screen rects in `windows`): on the line from the
 * middle of the area to each window, where it leaves the area inset by size/2 + margin;
 * markers in about the same direction are pushed apart along their edge. The same rules as
 * KDE Infinit Desktop's offscreenMarkers().
 */
QList<MarkerPlace> layoutMarkers(const QRectF &area, const QList<QRectF> &windows, qreal size, qreal margin);

} // namespace Wid
