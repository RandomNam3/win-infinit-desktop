/*
    KDE Infinite Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QString>

#include <optional>

namespace InfiniteDesktop
{

/**
 * Identifies a monitor across sessions. The connector name ("DP-1") can change
 * when cables are swapped, the EDID data (manufacturer, model, serial) moves
 * with the monitor; both are stored and combined when matching.
 */
struct OutputIdentity
{
    QString name;
    QString manufacturer;
    QString model;
    QString serial;

    bool hasEdid() const
    {
        return !manufacturer.isEmpty() || !model.isEmpty() || !serial.isEmpty();
    }
    QJsonObject toJson() const;
    static OutputIdentity fromJson(const QJsonObject &object);
};

/**
 * How well @p candidate matches @p stored:
 *  4 same monitor (EDID) on the same connector,
 *  3 same monitor on another connector,
 *  1 same connector, no EDID to compare (or an identical model without serial),
 *  0 no match (also when the EDID says it is a different monitor).
 */
int outputMatchScore(const OutputIdentity &stored, const OutputIdentity &candidate);
/// Index of the best matching candidate, or -1.
int bestOutputMatch(const OutputIdentity &stored, const QList<OutputIdentity> &candidates);

/// Where an application window was when it was closed.
struct WindowPlacement
{
    QString appKey;
    OutputIdentity output;
    QString desktopId;
    /// Frame top left on the canvas of that output.
    QPointF canvasPos;
    /// Frame top left relative to the output's top left as it was shown on screen.
    QPointF screenOffset;
    QSizeF size;
    bool maximized = false;
    bool pinned = false;
    bool keepAbove = false;
    qint64 timestamp = 0; ///< ms since epoch

    QJsonObject toJson() const;
    static std::optional<WindowPlacement> fromJson(const QJsonObject &object);
};

/**
 * Remembers the last placements of application windows.
 *
 * Several windows of one application are handled like a stack: a new window
 * takes the most recent placement that no other open window has taken yet; a
 * closing window replaces the placement it took with its current one. The
 * "taken" state only lives in memory.
 */
class PlacementMemory
{
public:
    static constexpr int MaxPerApplication = 8;
    static constexpr int MaxApplications = 300;

    /// Normalised key of an application (desktop file name, else window class).
    static QString applicationKey(const QString &desktopFileName, const QString &resourceClass);

    /// Takes the most recent free placement of @p appKey and returns its entry id in @p entryId.
    std::optional<WindowPlacement> take(const QString &appKey, quint64 *entryId);
    /// Gives back a placement that was taken but not used (window closed immediately, ...).
    void release(quint64 entryId);
    /// Stores the placement of a closing window; @p replacesEntry is the entry it had taken (or 0).
    void remember(const WindowPlacement &placement, quint64 replacesEntry);
    /// Forgets all placements of one application; returns whether there were any.
    bool forget(const QString &appKey);
    void clear();

    int applicationCount() const
    {
        return m_entries.size();
    }
    int placementCount(const QString &appKey) const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &object);

private:
    struct Entry
    {
        quint64 id = 0;
        WindowPlacement placement;
        bool taken = false;
    };
    void trimApplications();

    QHash<QString, QList<Entry>> m_entries;
    quint64 m_nextId = 1;
};

} // namespace InfiniteDesktop
