/*
    KDE Infinite Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "noteplacements.h"

#include <QJsonArray>
#include <QSet>

#include <cmath>

namespace InfiniteDesktop
{

namespace
{

bool finite(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y()) && std::abs(point.x()) < 1e8 && std::abs(point.y()) < 1e8;
}

} // namespace

QJsonObject NotePlacement::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("output"), output.toJson()},
        {QStringLiteral("desktop"), desktopId},
        {QStringLiteral("x"), canvasPos.x()},
        {QStringLiteral("y"), canvasPos.y()},
        {QStringLiteral("screenX"), screenOffset.x()},
        {QStringLiteral("screenY"), screenOffset.y()},
        {QStringLiteral("pinned"), pinned},
        {QStringLiteral("keepAbove"), keepAbove},
    };
}

std::optional<NotePlacement> NotePlacement::fromJson(const QJsonObject &object)
{
    NotePlacement placement;
    placement.id = object.value(QStringLiteral("id")).toString();
    placement.output = OutputIdentity::fromJson(object.value(QStringLiteral("output")).toObject());
    placement.desktopId = object.value(QStringLiteral("desktop")).toString();
    placement.canvasPos = QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
    placement.screenOffset = QPointF(object.value(QStringLiteral("screenX")).toDouble(), object.value(QStringLiteral("screenY")).toDouble());
    placement.pinned = object.value(QStringLiteral("pinned")).toBool();
    placement.keepAbove = object.value(QStringLiteral("keepAbove")).toBool();
    if (placement.id.isEmpty() || placement.id.size() > 64 || !finite(placement.canvasPos) || !finite(placement.screenOffset)) {
        return std::nullopt;
    }
    return placement;
}

std::optional<NotePlacement> NotePlacements::find(const QString &id) const
{
    auto it = m_placements.constFind(id);
    if (it == m_placements.cend()) {
        return std::nullopt;
    }
    return *it;
}

bool NotePlacements::store(const NotePlacement &placement)
{
    if (placement.id.isEmpty() || placement.id.size() > 64 || !finite(placement.canvasPos) || !finite(placement.screenOffset)
        || (!m_placements.contains(placement.id) && m_placements.size() >= MaxNotes)) {
        return false;
    }
    m_placements.insert(placement.id, placement);
    return true;
}

bool NotePlacements::remove(const QString &id)
{
    return m_placements.remove(id);
}

int NotePlacements::retain(const QStringList &existing)
{
    const QSet<QString> keep(existing.cbegin(), existing.cend());
    int removed = 0;
    for (auto it = m_placements.begin(); it != m_placements.end();) {
        if (keep.contains(it.key())) {
            ++it;
        } else {
            it = m_placements.erase(it);
            ++removed;
        }
    }
    return removed;
}

QJsonObject NotePlacements::toJson() const
{
    QJsonArray array;
    for (const NotePlacement &placement : m_placements) {
        array.append(placement.toJson());
    }
    return QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("notes"), array}};
}

void NotePlacements::fromJson(const QJsonObject &object)
{
    m_placements.clear();
    const QJsonArray array = object.value(QStringLiteral("notes")).toArray();
    for (const QJsonValue &value : array) {
        if (m_placements.size() >= MaxNotes) {
            break;
        }
        if (auto placement = NotePlacement::fromJson(value.toObject())) {
            m_placements.insert(placement->id, *placement);
        }
    }
}

} // namespace InfiniteDesktop
