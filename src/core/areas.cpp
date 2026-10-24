/*
    KDE Infinit Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "areas.h"

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

bool validRect(const QRectF &rect)
{
    const auto ok = [](qreal value, qreal limit) {
        return std::isfinite(value) && std::abs(value) < limit;
    };
    return ok(rect.x(), 1e8) && ok(rect.y(), 1e8) && ok(rect.width(), 1e6) && ok(rect.height(), 1e6) && rect.width() > 0
        && rect.height() > 0;
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

qreal areaOf(const QRectF &rect)
{
    return rect.width() * rect.height();
}

} // namespace

QJsonObject Area::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("output"), output.toJson()},
        {QStringLiteral("desktop"), desktopId},
        {QStringLiteral("drawn"), rectJson(drawnRect)},
        {QStringLiteral("rect"), rectJson(rect)},
        {QStringLiteral("dynamic"), dynamic},
        {QStringLiteral("color"), color},
    };
}

std::optional<Area> Area::fromJson(const QJsonObject &object)
{
    Area area;
    area.id = object.value(QStringLiteral("id")).toString();
    area.name = object.value(QStringLiteral("name")).toString().left(200);
    area.output = OutputIdentity::fromJson(object.value(QStringLiteral("output")).toObject());
    area.desktopId = object.value(QStringLiteral("desktop")).toString();
    area.drawnRect = rectFromJson(object.value(QStringLiteral("drawn")));
    area.rect = rectFromJson(object.value(QStringLiteral("rect")));
    area.dynamic = object.value(QStringLiteral("dynamic")).toBool(true);
    area.color = std::clamp(object.value(QStringLiteral("color")).toInt(), 0, AreaStore::Colors - 1);
    if (area.id.isEmpty() || !validRect(area.drawnRect)) {
        return std::nullopt;
    }
    if (!validRect(area.rect)) {
        area.rect = area.drawnRect;
    }
    return area;
}

QRectF dynamicAreaRect(const Area &area, const QList<QRectF> &members, const AreaMetrics &metrics)
{
    if (!area.dynamic) {
        return area.drawnRect;
    }
    QRectF covered;
    for (const QRectF &member : members) {
        if (!member.isEmpty()) {
            covered = covered.isNull() ? member : covered.united(member);
        }
    }
    if (covered.isNull()) {
        return area.drawnRect;
    }
    QRectF rect = covered.adjusted(-metrics.padding, -metrics.padding, metrics.padding, metrics.padding);
    // Never smaller than the minimum, growing evenly around the windows.
    if (rect.width() < metrics.minimumSize.width()) {
        const qreal grow = (metrics.minimumSize.width() - rect.width()) / 2;
        rect.adjust(-grow, 0, grow, 0);
    }
    if (rect.height() < metrics.minimumSize.height()) {
        const qreal grow = (metrics.minimumSize.height() - rect.height()) / 2;
        rect.adjust(0, -grow, 0, grow);
    }
    return rect;
}

int areaForWindow(const QList<Area> &areas, const QRectF &frame)
{
    return areaAt(areas, frame.center());
}

int areaAt(const QList<Area> &areas, const QPointF &canvasPoint)
{
    int best = -1;
    qreal bestSize = std::numeric_limits<qreal>::max();
    for (int i = 0; i < areas.size(); ++i) {
        if (!areas[i].rect.contains(canvasPoint)) {
            continue;
        }
        const qreal size = areaOf(areas[i].rect);
        if (size < bestSize) {
            bestSize = size;
            best = i;
        }
    }
    return best;
}

Area *AreaStore::find(const QString &id)
{
    const int index = indexOf(id);
    return index < 0 ? nullptr : &m_areas[index];
}

int AreaStore::indexOf(const QString &id) const
{
    for (int i = 0; i < m_areas.size(); ++i) {
        if (m_areas[i].id == id) {
            return i;
        }
    }
    return -1;
}

Area *AreaStore::add(Area area)
{
    area.drawnRect = area.drawnRect.normalized();
    if (m_areas.size() >= MaxAreas || !validRect(area.drawnRect)) {
        return nullptr;
    }
    if (area.id.isEmpty() || indexOf(area.id) >= 0) {
        area.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (!validRect(area.rect)) {
        area.rect = area.drawnRect;
    }
    area.color = m_areas.size() % Colors;
    m_areas.append(std::move(area));
    return &m_areas.last();
}

bool AreaStore::remove(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0) {
        return false;
    }
    m_areas.removeAt(index);
    return true;
}

void AreaStore::clear()
{
    m_areas.clear();
}

QString AreaStore::nextName(const QString &pattern) const
{
    const QString withNumber = pattern.contains(QLatin1String("%1")) ? pattern : pattern + QLatin1String(" %1");
    for (int n = 1;; ++n) {
        const QString name = withNumber.arg(n);
        if (n > MaxAreas) {
            return name;
        }
        const bool taken = std::any_of(m_areas.cbegin(), m_areas.cend(), [&name](const Area &area) {
            return area.name == name;
        });
        if (!taken) {
            return name;
        }
    }
}

QJsonObject AreaStore::toJson() const
{
    QJsonArray areaArray;
    for (const Area &area : m_areas) {
        areaArray.append(area.toJson());
    }
    return QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("areas"), areaArray}};
}

void AreaStore::fromJson(const QJsonObject &object)
{
    m_areas.clear();
    QSet<QString> ids;
    const QJsonArray areaArray = object.value(QStringLiteral("areas")).toArray();
    for (const QJsonValue &value : areaArray) {
        if (m_areas.size() >= MaxAreas) {
            break;
        }
        auto area = Area::fromJson(value.toObject());
        if (!area || ids.contains(area->id)) {
            continue;
        }
        ids.insert(area->id);
        m_areas.append(*area);
    }
}

} // namespace InfinitDesktop
