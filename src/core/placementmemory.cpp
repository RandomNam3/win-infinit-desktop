/*
    KDE Infinit Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "placementmemory.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <limits>

namespace InfinitDesktop
{

namespace
{

bool finite(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y()) && std::abs(point.x()) < 1e8 && std::abs(point.y()) < 1e8;
}

QJsonObject pointJson(const QPointF &point)
{
    return QJsonObject{{QStringLiteral("x"), point.x()}, {QStringLiteral("y"), point.y()}};
}

QPointF pointFromJson(const QJsonValue &value)
{
    const QJsonObject object = value.toObject();
    return QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
}

} // namespace

QJsonObject OutputIdentity::toJson() const
{
    return QJsonObject{
        {QStringLiteral("name"), name},
        {QStringLiteral("manufacturer"), manufacturer},
        {QStringLiteral("model"), model},
        {QStringLiteral("serial"), serial},
    };
}

OutputIdentity OutputIdentity::fromJson(const QJsonObject &object)
{
    return OutputIdentity{
        object.value(QStringLiteral("name")).toString(),
        object.value(QStringLiteral("manufacturer")).toString(),
        object.value(QStringLiteral("model")).toString(),
        object.value(QStringLiteral("serial")).toString(),
    };
}

int outputMatchScore(const OutputIdentity &stored, const OutputIdentity &candidate)
{
    const bool sameName = !stored.name.isEmpty() && stored.name == candidate.name;
    if (stored.hasEdid() && candidate.hasEdid()) {
        const bool sameModel = stored.manufacturer == candidate.manufacturer && stored.model == candidate.model;
        if (!sameModel) {
            return 0;
        }
        if (!stored.serial.isEmpty() && !candidate.serial.isEmpty()) {
            if (stored.serial != candidate.serial) {
                return 0;
            }
            return sameName ? 4 : 3;
        }
        // Identical model without serial numbers (two identical monitors): only the connector can tell.
        return sameName ? 3 : 1;
    }
    return sameName ? 1 : 0;
}

int bestOutputMatch(const OutputIdentity &stored, const QList<OutputIdentity> &candidates)
{
    int best = -1;
    int bestScore = 0;
    for (int i = 0; i < candidates.size(); ++i) {
        const int score = outputMatchScore(stored, candidates[i]);
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

QJsonObject WindowPlacement::toJson() const
{
    return QJsonObject{
        {QStringLiteral("app"), appKey},
        {QStringLiteral("output"), output.toJson()},
        {QStringLiteral("desktop"), desktopId},
        {QStringLiteral("canvas"), pointJson(canvasPos)},
        {QStringLiteral("screen"), pointJson(screenOffset)},
        {QStringLiteral("width"), size.width()},
        {QStringLiteral("height"), size.height()},
        {QStringLiteral("maximized"), maximized},
        {QStringLiteral("pinned"), pinned},
        {QStringLiteral("keepAbove"), keepAbove},
        {QStringLiteral("time"), double(timestamp)},
    };
}

std::optional<WindowPlacement> WindowPlacement::fromJson(const QJsonObject &object)
{
    WindowPlacement placement;
    placement.appKey = object.value(QStringLiteral("app")).toString();
    placement.output = OutputIdentity::fromJson(object.value(QStringLiteral("output")).toObject());
    placement.desktopId = object.value(QStringLiteral("desktop")).toString();
    placement.canvasPos = pointFromJson(object.value(QStringLiteral("canvas")));
    placement.screenOffset = pointFromJson(object.value(QStringLiteral("screen")));
    placement.size = QSizeF(object.value(QStringLiteral("width")).toDouble(), object.value(QStringLiteral("height")).toDouble());
    placement.maximized = object.value(QStringLiteral("maximized")).toBool();
    placement.pinned = object.value(QStringLiteral("pinned")).toBool();
    placement.keepAbove = object.value(QStringLiteral("keepAbove")).toBool();
    placement.timestamp = qint64(object.value(QStringLiteral("time")).toDouble());
    // Do not trust the file: a broken entry must never place a window at an absurd spot.
    if (placement.appKey.isEmpty() || !finite(placement.canvasPos) || !finite(placement.screenOffset)
        || !std::isfinite(placement.size.width()) || !std::isfinite(placement.size.height()) || placement.size.width() < 0
        || placement.size.height() < 0 || placement.size.width() > 1e5 || placement.size.height() > 1e5) {
        return std::nullopt;
    }
    return placement;
}

QString PlacementMemory::applicationKey(const QString &desktopFileName, const QString &resourceClass)
{
    QString key = desktopFileName.trimmed();
    if (key.endsWith(QLatin1String(".desktop"))) {
        key.chop(8);
    }
    if (key.isEmpty()) {
        key = resourceClass.trimmed();
    }
    return key.toLower();
}

std::optional<WindowPlacement> PlacementMemory::take(const QString &appKey, quint64 *entryId)
{
    auto it = m_entries.find(appKey);
    if (it == m_entries.end()) {
        return std::nullopt;
    }
    for (Entry &entry : *it) {
        if (!entry.taken) {
            entry.taken = true;
            if (entryId) {
                *entryId = entry.id;
            }
            return entry.placement;
        }
    }
    return std::nullopt;
}

void PlacementMemory::release(quint64 entryId)
{
    if (entryId == 0) {
        return;
    }
    for (QList<Entry> &entries : m_entries) {
        for (Entry &entry : entries) {
            if (entry.id == entryId) {
                entry.taken = false;
                return;
            }
        }
    }
}

void PlacementMemory::remember(const WindowPlacement &placement, quint64 replacesEntry)
{
    if (placement.appKey.isEmpty()) {
        return;
    }
    QList<Entry> &entries = m_entries[placement.appKey];
    if (replacesEntry != 0) {
        entries.removeIf([replacesEntry](const Entry &entry) {
            return entry.id == replacesEntry;
        });
    }
    entries.prepend(Entry{m_nextId++, placement, false});
    // Keep the newest entries, but never drop entries other open windows still hold.
    while (entries.size() > MaxPerApplication) {
        auto oldestFree = std::find_if(entries.rbegin(), entries.rend(), [](const Entry &entry) {
            return !entry.taken;
        });
        if (oldestFree == entries.rend()) {
            break;
        }
        entries.erase(std::next(oldestFree).base());
    }
    trimApplications();
}

void PlacementMemory::trimApplications()
{
    while (m_entries.size() > MaxApplications) {
        auto oldest = m_entries.end();
        qint64 oldestTime = std::numeric_limits<qint64>::max();
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            const qint64 newest = it->isEmpty() ? 0 : it->first().placement.timestamp;
            if (newest < oldestTime) {
                oldestTime = newest;
                oldest = it;
            }
        }
        if (oldest == m_entries.end()) {
            break;
        }
        m_entries.erase(oldest);
    }
}

bool PlacementMemory::forget(const QString &appKey)
{
    return m_entries.remove(appKey);
}

void PlacementMemory::clear()
{
    m_entries.clear();
}

int PlacementMemory::placementCount(const QString &appKey) const
{
    auto it = m_entries.constFind(appKey);
    return it == m_entries.constEnd() ? 0 : it->size();
}

QJsonObject PlacementMemory::toJson() const
{
    QJsonObject applications;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        QJsonArray placements;
        for (const Entry &entry : *it) {
            placements.append(entry.placement.toJson());
        }
        applications.insert(it.key(), placements);
    }
    return QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("applications"), applications}};
}

void PlacementMemory::fromJson(const QJsonObject &object)
{
    m_entries.clear();
    const QJsonObject applications = object.value(QStringLiteral("applications")).toObject();
    for (auto it = applications.constBegin(); it != applications.constEnd(); ++it) {
        QList<Entry> entries;
        const QJsonArray placements = it.value().toArray();
        for (const QJsonValue &value : placements) {
            if (auto placement = WindowPlacement::fromJson(value.toObject()); placement && placement->appKey == it.key()) {
                entries.append(Entry{m_nextId++, *placement, false});
            }
            if (entries.size() >= MaxPerApplication) {
                break;
            }
        }
        if (!entries.isEmpty()) {
            m_entries.insert(it.key(), entries);
        }
    }
    trimApplications();
}

} // namespace InfinitDesktop
