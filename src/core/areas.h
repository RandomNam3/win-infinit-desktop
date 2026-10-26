/*
    KDE Infinite Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "placementmemory.h"

#include <QJsonObject>
#include <QList>
#include <QRectF>
#include <QString>

#include <optional>

namespace InfiniteDesktop
{

/**
 * A named region of a canvas, drawn by the user.
 *
 * A dynamic area follows its windows: a window belongs to it once its frame centre is
 * dropped inside, and the area grows and shrinks to cover all its windows plus a margin.
 * Without windows it keeps the size it was drawn with. A fixed area keeps its rect.
 */
struct Area
{
    QString id;
    QString name;
    OutputIdentity output;
    QString desktopId;
    QRectF drawnRect; ///< as drawn (or last resized) by the user, canvas coordinates
    QRectF rect; ///< current rect, canvas coordinates
    bool dynamic = true;
    int color = 0; ///< index into the palette of the renderer

    QJsonObject toJson() const;
    static std::optional<Area> fromJson(const QJsonObject &object);
};

/// Settings of the dynamic rect.
struct AreaMetrics
{
    qreal padding = 48; ///< free space around the windows
    QSizeF minimumSize = QSizeF(320, 200);
};

/// The rect of a dynamic area for the given member windows (canvas rects).
QRectF dynamicAreaRect(const Area &area, const QList<QRectF> &members, const AreaMetrics &metrics);

/**
 * Index of the area a window with frame `frame` belongs to after it was dropped: the
 * smallest area containing the frame centre, -1 for none. Only areas of the same canvas
 * (the caller filters) are passed.
 */
int areaForWindow(const QList<Area> &areas, const QRectF &frame);

/// Topmost (smallest) area at a canvas point, -1 for none.
int areaAt(const QList<Area> &areas, const QPointF &canvasPoint);

class AreaStore
{
public:
    static constexpr int MaxAreas = 128;
    static constexpr int Colors = 8;

    const QList<Area> &areas() const
    {
        return m_areas;
    }
    QList<Area> &areas()
    {
        return m_areas;
    }
    Area *find(const QString &id);
    int indexOf(const QString &id) const;

    /// Adds an area (new id if needed, next color); nullptr when full or the rect is empty.
    Area *add(Area area);
    bool remove(const QString &id);
    void clear();
    QString nextName(const QString &pattern) const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &object);

private:
    QList<Area> m_areas;
};

} // namespace InfiniteDesktop
