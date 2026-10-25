/*
    KDE Infinit Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "groups.h"

#include <QJsonArray>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>

namespace InfinitDesktop
{

namespace
{

constexpr qreal MaxCoordinate = 1e8;
constexpr qreal MaxExtent = 1e5;

bool finite(qreal value, qreal limit)
{
    return std::isfinite(value) && std::abs(value) < limit;
}

bool validRect(const QRectF &rect)
{
    return finite(rect.x(), MaxCoordinate) && finite(rect.y(), MaxCoordinate) && finite(rect.width(), MaxExtent)
        && finite(rect.height(), MaxExtent) && rect.width() >= 0 && rect.height() >= 0;
}

QJsonObject rectJson(const QRectF &rect)
{
    return QJsonObject{
        {QStringLiteral("x"), rect.x()},
        {QStringLiteral("y"), rect.y()},
        {QStringLiteral("width"), rect.width()},
        {QStringLiteral("height"), rect.height()},
    };
}

QRectF rectFromJson(const QJsonValue &value)
{
    const QJsonObject object = value.toObject();
    return QRectF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble(),
                  object.value(QStringLiteral("width")).toDouble(), object.value(QStringLiteral("height")).toDouble());
}

qreal distanceSquared(const QPointF &a, const QPointF &b)
{
    const QPointF d = a - b;
    return d.x() * d.x() + d.y() * d.y();
}

QRectF bounds(const QList<QRectF> &rects)
{
    QRectF result;
    for (const QRectF &rect : rects) {
        if (rect.isEmpty()) {
            continue;
        }
        result = result.isNull() ? rect : result.united(rect);
    }
    return result;
}

} // namespace

QJsonObject GroupMember::toJson() const
{
    return QJsonObject{
        {QStringLiteral("app"), appKey},
        {QStringLiteral("launch"), launch},
        {QStringLiteral("geometry"), rectJson(geometry)},
    };
}

std::optional<GroupMember> GroupMember::fromJson(const QJsonObject &object)
{
    GroupMember member;
    member.appKey = object.value(QStringLiteral("app")).toString();
    member.launch = object.value(QStringLiteral("launch")).toString();
    member.geometry = rectFromJson(object.value(QStringLiteral("geometry")));
    if (member.appKey.isEmpty() || !validRect(member.geometry)) {
        return std::nullopt;
    }
    return member;
}

QRectF Group::layoutBounds() const
{
    QList<QRectF> rects;
    rects.reserve(members.size());
    for (const GroupMember &member : members) {
        rects.append(member.geometry);
    }
    return bounds(rects);
}

QRectF Group::canvasBounds() const
{
    return layoutBounds().translated(origin);
}

QJsonObject Group::toJson() const
{
    QJsonArray memberArray;
    for (const GroupMember &member : members) {
        memberArray.append(member.toJson());
    }
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("members"), memberArray},
        {QStringLiteral("output"), output.toJson()},
        {QStringLiteral("desktop"), desktopId},
        {QStringLiteral("x"), origin.x()},
        {QStringLiteral("y"), origin.y()},
        {QStringLiteral("layout"), layoutMode == GroupLayoutMode::Automatic ? QStringLiteral("automatic") : QStringLiteral("manual")},
    };
}

std::optional<Group> Group::fromJson(const QJsonObject &object)
{
    Group group;
    group.id = object.value(QStringLiteral("id")).toString();
    group.name = object.value(QStringLiteral("name")).toString().left(200);
    group.output = OutputIdentity::fromJson(object.value(QStringLiteral("output")).toObject());
    group.desktopId = object.value(QStringLiteral("desktop")).toString();
    group.origin = QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
    group.layoutMode = object.value(QStringLiteral("layout")).toString() == QLatin1String("automatic") ? GroupLayoutMode::Automatic
                                                                                                      : GroupLayoutMode::Manual;
    if (group.id.isEmpty() || !finite(group.origin.x(), MaxCoordinate) || !finite(group.origin.y(), MaxCoordinate)) {
        return std::nullopt;
    }
    const QJsonArray memberArray = object.value(QStringLiteral("members")).toArray();
    for (const QJsonValue &value : memberArray) {
        if (group.members.size() >= GroupStore::MaxMembers) {
            break;
        }
        if (auto member = GroupMember::fromJson(value.toObject())) {
            group.members.append(*member);
        }
    }
    return group;
}

QRectF GroupPlan::targetOf(int member, const Group &group) const
{
    if (member < 0 || member >= group.members.size()) {
        return {};
    }
    return group.members[member].geometry.translated(group.origin);
}

GroupPlan planGroup(const Group &group, const QList<GroupCandidate> &windows)
{
    GroupPlan plan;
    plan.bounds = group.canvasBounds();
    QSet<quint64> used;
    const QPointF groupCenter = plan.bounds.center();

    for (int m = 0; m < group.members.size(); ++m) {
        const GroupMember &member = group.members[m];
        QList<int> ofApp;
        for (int w = 0; w < windows.size(); ++w) {
            if (windows[w].appKey == member.appKey && !used.contains(windows[w].handle)) {
                ofApp.append(w);
            }
        }
        int chosen = -1;
        // 1. already tagged with this group
        for (int w : std::as_const(ofApp)) {
            if (windows[w].groupId == group.id) {
                chosen = w;
                break;
            }
        }
        // 2. the only window of this application
        if (chosen < 0 && ofApp.size() == 1) {
            chosen = ofApp.first();
        }
        // 3. the nearest untagged window
        if (chosen < 0) {
            qreal best = std::numeric_limits<qreal>::max();
            for (int w : std::as_const(ofApp)) {
                if (!windows[w].groupId.isEmpty()) {
                    continue;
                }
                const qreal distance = distanceSquared(windows[w].canvasRect.center(), groupCenter);
                if (distance < best) {
                    best = distance;
                    chosen = w;
                }
            }
        }
        if (chosen < 0) {
            plan.launches.append(m);
            continue;
        }
        used.insert(windows[chosen].handle);
        if (!windows[chosen].movable) {
            continue;
        }
        plan.moves.append(GroupPlan::Move{windows[chosen].handle, m, member.geometry.translated(group.origin)});
    }
    return plan;
}

void captureLayout(Group &group, const QList<QRectF> &memberRects)
{
    const int count = std::min<int>(group.members.size(), memberRects.size());
    QList<QRectF> rects;
    for (int i = 0; i < count; ++i) {
        rects.append(memberRects[i]);
    }
    const QRectF current = bounds(rects);
    if (current.isNull()) {
        return;
    }
    // Members without a window keep their place relative to the old origin.
    const QPointF oldOrigin = group.origin;
    group.origin = current.topLeft();
    for (int i = 0; i < group.members.size(); ++i) {
        GroupMember &member = group.members[i];
        if (i < count && !memberRects[i].isEmpty() && validRect(memberRects[i])) {
            member.geometry = memberRects[i].translated(-group.origin);
        } else {
            member.geometry.translate(oldOrigin - group.origin);
        }
    }
}

void followWindows(Group &group, const QList<QRectF> &memberRects)
{
    // The mean displacement of the windows from their place in the arrangement: moving the
    // whole group moves the origin exactly, moving one of three windows a third of the way.
    QPointF sum;
    int count = 0;
    for (int i = 0; i < std::min<int>(group.members.size(), memberRects.size()); ++i) {
        if (memberRects[i].isEmpty() || !validRect(memberRects[i])) {
            continue;
        }
        sum += memberRects[i].topLeft() - group.members[i].geometry.topLeft();
        ++count;
    }
    if (count == 0) {
        return;
    }
    const QPointF origin = sum / count;
    if (finite(origin.x(), MaxCoordinate) && finite(origin.y(), MaxCoordinate)) {
        group.origin = origin;
    }
}

QList<QRectF> autoArrange(const QList<QSizeF> &sizes, int perRow, qreal gap)
{
    QList<QRectF> result;
    result.reserve(sizes.size());
    perRow = std::max(1, perRow);
    qreal y = 0;
    for (int start = 0; start < sizes.size(); start += perRow) {
        qreal x = 0;
        qreal rowHeight = 0;
        for (int i = start; i < std::min<int>(start + perRow, sizes.size()); ++i) {
            const QSizeF size = sizes[i].isValid() ? sizes[i] : QSizeF(800, 600);
            result.append(QRectF(QPointF(x, y), size));
            x += size.width() + gap;
            rowHeight = std::max(rowHeight, size.height());
        }
        y += rowHeight + gap;
    }
    return result;
}

QList<int> windowsInside(const QRectF &canvasRect, const QList<QRectF> &windows)
{
    QList<int> result;
    const QRectF area = canvasRect.normalized();
    for (int i = 0; i < windows.size(); ++i) {
        if (area.contains(windows[i].center())) {
            result.append(i);
        }
    }
    return result;
}

Group *GroupStore::find(const QString &id)
{
    const int index = indexOf(id);
    return index < 0 ? nullptr : &m_groups[index];
}

const Group *GroupStore::find(const QString &id) const
{
    const int index = indexOf(id);
    return index < 0 ? nullptr : &m_groups[index];
}

int GroupStore::indexOf(const QString &id) const
{
    for (int i = 0; i < m_groups.size(); ++i) {
        if (m_groups[i].id == id) {
            return i;
        }
    }
    return -1;
}

Group *GroupStore::add(Group group)
{
    if (m_groups.size() >= MaxGroups) {
        return nullptr;
    }
    if (group.id.isEmpty() || indexOf(group.id) >= 0) {
        group.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (group.members.size() > MaxMembers) {
        group.members.resize(MaxMembers);
    }
    m_groups.append(std::move(group));
    return &m_groups.last();
}

bool GroupStore::remove(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0) {
        return false;
    }
    m_groups.removeAt(index);
    return true;
}

bool GroupStore::update(const Group &group)
{
    Group *stored = find(group.id);
    if (!stored) {
        return false;
    }
    *stored = group;
    if (stored->members.size() > MaxMembers) {
        stored->members.resize(MaxMembers);
    }
    return true;
}

bool GroupStore::move(const QString &id, int newIndex)
{
    const int index = indexOf(id);
    if (index < 0 || newIndex < 0 || newIndex >= m_groups.size()) {
        return false;
    }
    m_groups.move(index, newIndex);
    return true;
}

bool GroupStore::removeApplication(const QString &appKey)
{
    bool removed = false;
    for (Group &group : m_groups) {
        removed |= group.members.removeIf([&appKey](const GroupMember &member) {
            return member.appKey == appKey;
        }) > 0;
    }
    return removed;
}

void GroupStore::clear()
{
    m_groups.clear();
}

QString GroupStore::nextName(const QString &pattern) const
{
    const QString withNumber = pattern.contains(QLatin1String("%1")) ? pattern : pattern + QLatin1String(" %1");
    // One more than there can be groups, so the loop always ends.
    for (int n = 1;; ++n) {
        const QString name = withNumber.arg(n);
        if (n > MaxGroups) {
            return name;
        }
        const bool taken = std::any_of(m_groups.cbegin(), m_groups.cend(), [&name](const Group &group) {
            return group.name == name;
        });
        if (!taken) {
            return name;
        }
    }
}

QJsonObject GroupStore::toJson() const
{
    QJsonArray groupArray;
    for (const Group &group : m_groups) {
        groupArray.append(group.toJson());
    }
    return QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("groups"), groupArray}};
}

void GroupStore::fromJson(const QJsonObject &object)
{
    m_groups.clear();
    const QJsonArray groupArray = object.value(QStringLiteral("groups")).toArray();
    QSet<QString> ids;
    for (const QJsonValue &value : groupArray) {
        if (m_groups.size() >= MaxGroups) {
            break;
        }
        auto group = Group::fromJson(value.toObject());
        if (!group || ids.contains(group->id)) {
            continue;
        }
        ids.insert(group->id);
        m_groups.append(*group);
    }
}

} // namespace InfinitDesktop
