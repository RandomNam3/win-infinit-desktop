/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "windowplacements.h"

#include "applicationindex.h"
#include "i18n.h"
#include "localizer.h"
#include "serviceconnection.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDateTime>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSet>

#include <algorithm>

namespace InfinitDesktop
{

namespace
{

WindowPlacements *s_placementsInstance = nullptr;
bool s_demoMode = false;


QString screenName(const QJsonObject &output)
{
    const QString name = output.value(QLatin1String("name")).toString();
    const QString manufacturer = output.value(QLatin1String("manufacturer")).toString().trimmed();
    const QString model = output.value(QLatin1String("model")).toString().trimmed();
    QString product = model;
    if (!manufacturer.isEmpty() && !model.startsWith(manufacturer, Qt::CaseInsensitive)) {
        product = QString(manufacturer + QLatin1Char(' ') + model).trimmed();
    }
    if (product.isEmpty()) {
        return name.isEmpty() ? i18nc("@label unknown screen", "Unknown screen") : name;
    }
    if (name.isEmpty()) {
        return product;
    }
    return i18nc("@label screen product name and connector, for example Dell U2720Q (DP-1)", "%1 (%2)", product, name);
}

/// The locale for dates and times: the region setting, or the UI language if the
/// process runs with the "C" locale.
QLocale formattingLocale()
{
    const QLocale system;
    if (system.language() != QLocale::C) {
        return system;
    }
    return QLocale(Wid::Localizer::instance().language());
}

QString formatTime(const QJsonValue &value)
{
    QDateTime time;
    if (value.isDouble()) {
        const double number = value.toDouble();
        time = number > 1e11 ? QDateTime::fromMSecsSinceEpoch(qint64(number)) : QDateTime::fromSecsSinceEpoch(qint64(number));
    } else if (value.isString()) {
        time = QDateTime::fromString(value.toString(), Qt::ISODate);
    }
    if (!time.isValid()) {
        return QString();
    }
    const QDateTime local = time.toLocalTime();
    const QLocale locale = formattingLocale();
    const QString timeText = locale.toString(local.time(), QLocale::ShortFormat);
    const QDate today = QDate::currentDate();
    if (local.date() == today) {
        return i18nc("@label when a window was closed, %1 is a time of day", "Today, %1", timeText);
    }
    if (local.date() == today.addDays(-1)) {
        return i18nc("@label when a window was closed, %1 is a time of day", "Yesterday, %1", timeText);
    }
    const QString dateFormat = i18nc("date format of the remembered windows list, see QDate::toString(), for example Oct 24, 2026", "MMM d, yyyy");
    return i18nc("@label %1 is a date, %2 a time of day", "%1, %2", locale.toString(local.date(), dateFormat), timeText);
}

QVariantMap lookupApplication(const QString &key)
{
    static const QHash<QString, QString> demoNames{
        {QStringLiteral("microsoft.windows.explorer"), QStringLiteral("File Explorer")},
        {QStringLiteral("microsoft.windowsterminal_8wekyb3d8bbwe!app"), QStringLiteral("Terminal")},
        {QStringLiteral("microsoft.windowsnotepad_8wekyb3d8bbwe!app"), QStringLiteral("Notepad")},
        {QStringLiteral("firefox-308046b0af4a39cb"), QStringLiteral("Firefox")},
    };
    QVariantMap info = ApplicationIndex::instance()->describe(key);
    if (s_demoMode && !info.value(QStringLiteral("known")).toBool()) {
        info.insert(QStringLiteral("name"), demoNames.value(key, key));
    }
    return info;
}

// Screenshot mode: applications "forgotten" in the demo data.
QSet<QString> s_demoForgotten;

} // namespace

WindowPlacements::WindowPlacements(QObject *parent)
    : QObject(parent)
{
}

WindowPlacements *WindowPlacements::instance()
{
    if (!s_placementsInstance) {
        s_placementsInstance = new WindowPlacements(QCoreApplication::instance());
    }
    return s_placementsInstance;
}

WindowPlacements *WindowPlacements::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    WindowPlacements *placements = instance();
    QJSEngine::setObjectOwnership(placements, QJSEngine::CppOwnership);
    return placements;
}

void WindowPlacements::setDemoMode(bool demo)
{
    s_demoMode = demo;
}

bool WindowPlacements::available() const
{
    return m_available;
}

bool WindowPlacements::loading() const
{
    return m_loading;
}

QVariantList WindowPlacements::applications() const
{
    return m_applications;
}

int WindowPlacements::windowCount() const
{
    return m_windowCount;
}

void WindowPlacements::refresh()
{
    if (s_demoMode) {
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const QByteArray json = QStringLiteral(R"({"version":1,"applications":{
            "microsoft.windows.explorer":[
                {"app":"microsoft.windows.explorer","output":{"name":"DISPLAY1","manufacturer":"DEL","model":"DELL U2720Q","serial":"1"},"canvas":{"x":-1840,"y":120},"screen":{"x":80,"y":120},"width":1280,"height":820,"maximized":false,"pinned":false,"keepAbove":false,"time":%1},
                {"app":"microsoft.windows.explorer","output":{"name":"DISPLAY2","manufacturer":"BOE","model":"0x0BCA","serial":""},"canvas":{"x":200,"y":90},"screen":{"x":200,"y":90},"width":1100,"height":700,"maximized":false,"pinned":false,"keepAbove":false,"time":%2}],
            "microsoft.windowsterminal_8wekyb3d8bbwe!app":[
                {"app":"microsoft.windowsterminal_8wekyb3d8bbwe!app","output":{"name":"DISPLAY1","manufacturer":"DEL","model":"DELL U2720Q","serial":"1"},"canvas":{"x":2600,"y":-400},"screen":{"x":640,"y":300},"width":960,"height":600,"maximized":false,"pinned":true,"keepAbove":true,"time":%3}],
            "firefox-308046b0af4a39cb":[
                {"app":"firefox-308046b0af4a39cb","output":{"name":"DISPLAY1","manufacturer":"DEL","model":"DELL U2720Q","serial":"1"},"canvas":{"x":0,"y":0},"screen":{"x":0,"y":0},"width":2560,"height":1400,"maximized":true,"pinned":false,"keepAbove":false,"time":%4}],
            "microsoft.windowsnotepad_8wekyb3d8bbwe!app":[
                {"app":"microsoft.windowsnotepad_8wekyb3d8bbwe!app","output":{"name":"DISPLAY2","manufacturer":"BOE","model":"0x0BCA","serial":""},"canvas":{"x":5200,"y":1800},"screen":{"x":300,"y":200},"width":1400,"height":900,"maximized":false,"pinned":false,"keepAbove":false,"time":%5}]
            }})")
                                   .arg(now - 600)
                                   .arg(now - 86400)
                                   .arg(now - 3600)
                                   .arg(now - 7200)
                                   .arg(now - 3 * 86400)
                                   .toUtf8();
        m_available = true;
        parse(json);
        return;
    }

    m_loading = true;
    Q_EMIT changed();
    ServiceConnection::instance()->call(QStringLiteral("windowPlacements"), {}, this, [this](const QJsonValue &result, const QString &error) {
        m_loading = false;
        if (!error.isEmpty()) {
            // The service is not running.
            m_available = false;
            m_applications.clear();
            m_windowCount = 0;
            Q_EMIT changed();
            return;
        }
        m_available = true;
        // The document itself, or (like the KDE effect) its JSON text.
        parse(result.isString() ? result.toString().toUtf8() : QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact));
    });
}

void WindowPlacements::parse(const QByteArray &json)
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const QJsonObject apps = root.value(QLatin1String("applications")).toObject();
    QLocale locale = formattingLocale();
    // "2560 × 1400", not "2.560 × 1.400".
    locale.setNumberOptions(QLocale::OmitGroupSeparator);

    QVariantList result;
    int windows = 0;
    for (auto it = apps.constBegin(); it != apps.constEnd(); ++it) {
        const QJsonArray entries = it.value().toArray();
        if (entries.isEmpty() || (s_demoMode && s_demoForgotten.contains(it.key()))) {
            continue;
        }
        const QVariantMap info = lookupApplication(it.key());
        QVariantList windowList;
        for (const QJsonValue &value : entries) {
            const QJsonObject entry = value.toObject();
            QStringList flags;
            if (entry.value(QLatin1String("maximized")).toBool()) {
                flags.append(i18nc("@label window state", "Maximized"));
            }
            if (entry.value(QLatin1String("pinned")).toBool()) {
                flags.append(i18nc("@label window state", "Pinned"));
            }
            if (entry.value(QLatin1String("keepAbove")).toBool()) {
                flags.append(i18nc("@label window state", "Kept above"));
            }
            const int width = entry.value(QLatin1String("width")).toInt();
            const int height = entry.value(QLatin1String("height")).toInt();
            windowList.append(QVariantMap{
                {QStringLiteral("screen"), screenName(entry.value(QLatin1String("output")).toObject())},
                {QStringLiteral("size"), width > 0 && height > 0 ? i18nc("@label window size in pixels", "%1 × %2", locale.toString(width), locale.toString(height)) : QString()},
                {QStringLiteral("flags"), flags},
                {QStringLiteral("time"), formatTime(entry.value(QLatin1String("time")))},
            });
        }
        windows += windowList.size();
        const QVariantMap first = windowList.constFirst().toMap();
        QStringList summary{i18ncp("@label number of remembered windows", "%1 window", "%1 windows", windowList.size()), first.value(QStringLiteral("screen")).toString()};
        if (!first.value(QStringLiteral("size")).toString().isEmpty()) {
            summary.append(first.value(QStringLiteral("size")).toString());
        }
        result.append(QVariantMap{
            {QStringLiteral("appKey"), it.key()},
            {QStringLiteral("name"), info.value(QStringLiteral("name"))},
            {QStringLiteral("iconName"), info.value(QStringLiteral("iconName"))},
            {QStringLiteral("hasIcon"), info.value(QStringLiteral("hasIcon"))},
            {QStringLiteral("windowCount"), int(windowList.size())},
            {QStringLiteral("summary"), summary.join(QStringLiteral(" · "))},
            {QStringLiteral("windows"), windowList},
        });
    }

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(result.begin(), result.end(), [&collator](const QVariant &a, const QVariant &b) {
        return collator.compare(a.toMap().value(QStringLiteral("name")).toString(), b.toMap().value(QStringLiteral("name")).toString()) < 0;
    });

    m_applications = result;
    m_windowCount = windows;
    Q_EMIT changed();
}

void WindowPlacements::forgetApplication(const QString &appKey)
{
    QString name = appKey;
    int index = -1;
    for (int i = 0; i < m_applications.size(); ++i) {
        const QVariantMap app = m_applications.at(i).toMap();
        if (app.value(QStringLiteral("appKey")).toString() == appKey) {
            name = app.value(QStringLiteral("name")).toString();
            index = i;
            break;
        }
    }
    if (index < 0) {
        return;
    }
    // Remove it right away, the refresh afterwards confirms the real state.
    m_windowCount -= m_applications.at(index).toMap().value(QStringLiteral("windowCount")).toInt();
    m_applications.removeAt(index);
    Q_EMIT changed();

    if (s_demoMode) {
        s_demoForgotten.insert(appKey);
        Q_EMIT applicationForgotten(name);
        return;
    }
    ServiceConnection::instance()->call(QStringLiteral("forgetApplicationPlacements"), {{QStringLiteral("app"), appKey}}, this,
                                        [this, name](const QJsonValue &, const QString &error) {
        if (!error.isEmpty()) {
            Q_EMIT failed(i18n("The remembered position of “%1” could not be removed: %2", name, error));
        } else {
            Q_EMIT applicationForgotten(name);
        }
        refresh();
    });
}

void WindowPlacements::forgetAll()
{
    if (s_demoMode) {
        for (const QVariant &app : std::as_const(m_applications)) {
            s_demoForgotten.insert(app.toMap().value(QStringLiteral("appKey")).toString());
        }
        m_applications.clear();
        m_windowCount = 0;
        Q_EMIT changed();
        Q_EMIT forgotten();
        return;
    }
    ServiceConnection::instance()->call(QStringLiteral("forgetWindowPlacements"), {}, this, [this](const QJsonValue &, const QString &error) {
        if (!error.isEmpty()) {
            Q_EMIT failed(i18n("The remembered positions could not be removed: %1", error));
        } else {
            Q_EMIT forgotten();
        }
        refresh();
    });
}

} // namespace InfinitDesktop
