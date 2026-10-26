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

/// When the arrangement of a group's windows is stored.
enum class GroupLayoutMode {
    Manual, ///< only on "save arrangement"; the group always opens the same way
    Automatic, ///< every move or resize of a member while the group is open
};

/// One application of a group and where its window lies in the group.
struct GroupMember
{
    QString appKey; ///< PlacementMemory::applicationKey()
    /// How to start it: desktop file name (KDE), AppUserModelID or program path (Windows).
    QString launch;
    /// Frame rect relative to the group origin, canvas pixels at 100 %.
    QRectF geometry;

    QJsonObject toJson() const;
    static std::optional<GroupMember> fromJson(const QJsonObject &object);
};

/**
 * A group of applications that opens, or comes together, at one place of the canvas.
 *
 * The arrangement (member geometries) is relative to the origin. The origin is where the
 * group was open last, on the canvas of `output`; it moves with the windows, the
 * arrangement only changes as `layoutMode` says.
 */
struct Group
{
    QString id;
    QString name;
    QList<GroupMember> members;
    OutputIdentity output;
    QString desktopId;
    QPointF origin;
    GroupLayoutMode layoutMode = GroupLayoutMode::Manual;

    /// Bounding rect of the arrangement, relative to the origin.
    QRectF layoutBounds() const;
    /// Bounding rect on the canvas.
    QRectF canvasBounds() const;

    QJsonObject toJson() const;
    static std::optional<Group> fromJson(const QJsonObject &object);
};

/// An open window as the group logic sees it.
struct GroupCandidate
{
    quint64 handle = 0; ///< caller's id of the window
    QString appKey;
    QRectF canvasRect; ///< frame on its canvas
    /// Group this window was last opened or arranged with, empty if none.
    QString groupId;
    bool movable = true; ///< false: fullscreen, elevated, …
};

/// What opening a group does.
struct GroupPlan
{
    struct Move
    {
        quint64 handle = 0;
        int member = -1;
        QRectF target; ///< canvas rect
    };
    QList<Move> moves;
    /// Members without a window: start them and place them at their target.
    QList<int> launches;
    /// Canvas rect of the whole group at its target place.
    QRectF bounds;

    QRectF targetOf(int member, const Group &group) const;
};

/**
 * Which windows open a group, and where they go.
 *
 * Per member, in this order:
 *  1. a window of that application already tagged with this group;
 *  2. the only open window of that application, even if it belongs to another group (a
 *     browser in several groups moves along with whichever group is opened);
 *  3. an untagged window of that application, the one nearest to the group;
 *  4. none: the member is started.
 * A window is used for one member only; two members of one application need two windows.
 */
GroupPlan planGroup(const Group &group, const QList<GroupCandidate> &windows);

/// Arrangement and origin from the current windows of a group (in member order; an
/// empty rect keeps that member's stored geometry).
void captureLayout(Group &group, const QList<QRectF> &memberRects);

/// Moves the origin by the mean displacement of the members' current windows from their place
/// in the arrangement (empty rects: no window). The arrangement itself stays.
void followWindows(Group &group, const QList<QRectF> &memberRects);

/**
 * Arrangement for applications without stored geometry: next to each other in rows of at
 * most `perRow`, top aligned, `gap` apart. Returns frame rects relative to (0, 0).
 */
QList<QRectF> autoArrange(const QList<QSizeF> &sizes, int perRow, qreal gap);

/// Windows whose frame centre lies inside `canvasRect` (indices into `windows`).
QList<int> windowsInside(const QRectF &canvasRect, const QList<QRectF> &windows);

/**
 * All groups, with JSON persistence. Ids are generated here, names stay as given.
 */
class GroupStore
{
public:
    static constexpr int MaxGroups = 64;
    static constexpr int MaxMembers = 32;

    const QList<Group> &groups() const
    {
        return m_groups;
    }
    Group *find(const QString &id);
    const Group *find(const QString &id) const;
    int indexOf(const QString &id) const;

    /// Adds a group (a new id if empty or taken); returns it, or nullptr when full.
    Group *add(Group group);
    bool remove(const QString &id);
    /// Replaces the stored group with the same id; false if there is none.
    bool update(const Group &group);
    /// Moves a group to another position in the list (its shortcut slot).
    bool move(const QString &id, int newIndex);
    /// Forgets every member of `appKey` in every group; returns whether any was removed.
    bool removeApplication(const QString &appKey);
    void clear();

    /// "Gruppe 3": the first name of the pattern not in use.
    QString nextName(const QString &pattern) const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &object);

private:
    QList<Group> m_groups;
};

} // namespace InfiniteDesktop
