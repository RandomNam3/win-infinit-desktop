/*
    KDE Infinite Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "placementmemory.h"

#include <QHash>
#include <QJsonObject>
#include <QPointF>
#include <QString>

#include <optional>

namespace InfiniteDesktop
{

/**
 * Where a note lies. Notes are windows of the companion app; only the canvas knows canvas
 * coordinates, so it stores their places by note id (the app stores text, size and color).
 */
struct NotePlacement
{
    QString id;
    OutputIdentity output;
    QString desktopId;
    QPointF canvasPos; ///< frame top left on the canvas
    QPointF screenOffset; ///< frame top left relative to the output, for pinned notes
    bool pinned = false;
    bool keepAbove = false;

    QJsonObject toJson() const;
    static std::optional<NotePlacement> fromJson(const QJsonObject &object);
};

class NotePlacements
{
public:
    static constexpr int MaxNotes = 2000;

    std::optional<NotePlacement> find(const QString &id) const;
    /// Stores or replaces; false for broken values or when full (existing notes can always be updated).
    bool store(const NotePlacement &placement);
    bool remove(const QString &id);
    /// Keeps only the ids in `existing` (notes deleted while the canvas was not running).
    int retain(const QStringList &existing);
    int count() const
    {
        return m_placements.size();
    }

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &object);

private:
    QHash<QString, NotePlacement> m_placements;
};

} // namespace InfiniteDesktop
