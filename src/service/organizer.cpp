/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "organizer.h"
#include "canvasservice.h"
#include "layered.h"
#include "logging.h"
#include "navigation.h"
#include "texts.h"
#include "windowmover.h"

#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>

#include <objbase.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <system_error>
#include <thread>

using namespace std::chrono_literals;
using InfinitDesktop::Area;
using InfinitDesktop::Camera;
using InfinitDesktop::Group;

namespace Wid
{

namespace
{

using SteadyClock = std::chrono::steady_clock;

constexpr auto LaunchTimeout = 30s;
constexpr auto ExpectedNoteTimeout = 10s;
constexpr auto NewNoteTimeout = 15s;
constexpr qreal GroupMargin = 32.0;
constexpr int MinimumSelection = 12;
constexpr int MaxExpectedNotes = 64;
constexpr qint64 MaxStateFile = 16 * 1024 * 1024;

// Area colors as in KDE Infinit Desktop, readable on light and dark wallpapers alike.
const QColor AreaColors[InfinitDesktop::AreaStore::Colors] = {
    QColor(0x3d, 0xae, 0xe9), // blue
    QColor(0x27, 0xae, 0x60), // green
    QColor(0xf6, 0x74, 0x00), // orange
    QColor(0x9b, 0x59, 0xb6), // purple
    QColor(0xda, 0x44, 0x53), // red
    QColor(0x16, 0xa0, 0x85), // teal
    QColor(0xc9, 0xce, 0x3b), // olive
    QColor(0xe9, 0x3d, 0x8f), // pink
};
const QColor AccentColor(0x3d, 0xae, 0xe9);
const QColor NoteFill(0xf7, 0xd5, 0x4e);
const QColor PanelColor(32, 34, 38);

QString localized(const wchar_t *english, const wchar_t *german)
{
    return QString::fromWCharArray(text(english, german));
}

QColor areaColor(int index)
{
    return AreaColors[std::clamp(index, 0, InfinitDesktop::AreaStore::Colors - 1)];
}

COLORREF colorRef(const QColor &color)
{
    return RGB(color.red(), color.green(), color.blue());
}

QRect toRect(const RECT &rect)
{
    return QRect(QPoint(rect.left, rect.top), QPoint(rect.right - 1, rect.bottom - 1));
}

POINT rounded(const QPointF &point)
{
    return POINT{LONG(std::lround(point.x())), LONG(std::lround(point.y()))};
}

QJsonObject loadJson(const QString &name)
{
    QFile file(QDir(stateDirectory()).filePath(name));
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxStateFile) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

void saveJson(const QString &name, const QJsonObject &object)
{
    QSaveFile file(QDir(stateDirectory()).filePath(name));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
        if (!file.commit()) {
            qCWarning(lcService) << "cannot write" << name;
        }
    }
}

std::optional<QJsonObject> parseObject(const QString &json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

QString menuText(CanvasMenuItem item)
{
    switch (item) {
    case CanvasMenuItem::Separator:
        return {};
    case CanvasMenuItem::NewNote:
        return localized(L"New Note Here", L"Neue Notiz hier");
    case CanvasMenuItem::ToggleNotes:
        return localized(L"Show / Hide Notes", L"Notizen ein-/ausblenden");
    case CanvasMenuItem::NewGroup:
        return localized(L"New Group from Selection…", L"Neue Gruppe aus Auswahl …");
    case CanvasMenuItem::NewArea:
        return localized(L"New Area from Selection…", L"Neuer Bereich aus Auswahl …");
    case CanvasMenuItem::RemoveArea:
        return localized(L"Remove This Area", L"Diesen Bereich entfernen");
    case CanvasMenuItem::Map:
        return localized(L"Show Map", L"Karte zeigen");
    case CanvasMenuItem::ShowAll:
        return localized(L"Show All Windows", L"Alle Fenster zeigen");
    case CanvasMenuItem::ZoomReset:
        return localized(L"Zoom to 100 %", L"Auf 100 % zoomen");
    case CanvasMenuItem::Settings:
        return localized(L"WIN Infinit Desktop Settings…", L"Einstellungen von WIN Infinit Desktop …");
    case CanvasMenuItem::Quit:
        return localized(L"Quit WIN Infinit Desktop", L"WIN Infinit Desktop beenden");
    }
    return {};
}

} // namespace

// ---- Pure helpers ---------------------------------------------------------------------------

QList<CanvasMenuItem> canvasMenuItems(bool notesEnabled, bool overArea, bool zoomedOut)
{
    QList<CanvasMenuItem> items;
    if (notesEnabled) {
        items << CanvasMenuItem::NewNote << CanvasMenuItem::ToggleNotes << CanvasMenuItem::Separator;
    }
    items << CanvasMenuItem::NewGroup << CanvasMenuItem::NewArea;
    if (overArea) {
        items << CanvasMenuItem::RemoveArea;
    }
    items << CanvasMenuItem::Map << CanvasMenuItem::Separator << CanvasMenuItem::ShowAll;
    if (zoomedOut) {
        items << CanvasMenuItem::ZoomReset;
    }
    items << CanvasMenuItem::Settings << CanvasMenuItem::Quit;
    return items;
}

LaunchCommand launchCommand(const QString &launch)
{
    const QString trimmed = launch.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    const bool isPath = trimmed.contains(QLatin1Char('\\')) || trimmed.contains(QLatin1Char('/')) || trimmed.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive);
    if (isPath) {
        return LaunchCommand{QDir::toNativeSeparators(trimmed).toStdWString(), true};
    }
    // An AppUserModelID: the Start menu's way of starting it (Store apps and most others).
    return LaunchCommand{L"shell:AppsFolder\\" + trimmed.toStdWString(), false};
}

std::optional<quintptr> parseHandle(const QString &text)
{
    const QString trimmed = text.trimmed();
    bool ok = false;
    const qulonglong value = trimmed.startsWith(QLatin1String("0x"), Qt::CaseInsensitive) ? trimmed.mid(2).toULongLong(&ok, 16) : trimmed.toULongLong(&ok, 10);
    if (!ok || value == 0 || value > std::numeric_limits<quintptr>::max()) {
        return std::nullopt;
    }
    return quintptr(value);
}

bool isValidNoteId(const QString &id)
{
    if (id.isEmpty() || id.size() > 64) {
        return false;
    }
    return std::all_of(id.cbegin(), id.cend(), [](QChar c) {
        return (c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('A') && c <= QLatin1Char('Z')) || (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
            || c == QLatin1Char('-') || c == QLatin1Char('_');
    });
}

// ---- Organizer ------------------------------------------------------------------------------

Organizer::Organizer(CanvasService &service)
    : m_service(service)
{
    m_labelTimer.setSingleShot(true);
    connect(&m_labelTimer, &QTimer::timeout, this, [this]() {
        if (m_label) {
            m_label->hide();
        }
        m_labelPersistent = false;
    });
    connect(&m_notesClient, &NotesClient::unavailable, this, [this]() {
        flash(localized(L"The notes app is not installed.", L"Die Notiz-App ist nicht installiert."));
    });
}

Organizer::~Organizer()
{
    m_labelTimer.stop();
}

void Organizer::load()
{
    m_groups.fromJson(loadJson(QStringLiteral("groups.json")));
    m_areas.fromJson(loadJson(QStringLiteral("areas.json")));
    m_notes.fromJson(loadJson(QStringLiteral("notes.json")));
}

void Organizer::persist()
{
    if (m_groupsDirty) {
        saveJson(QStringLiteral("groups.json"), m_groups.toJson());
        m_groupsDirty = false;
    }
    if (m_areasDirty) {
        saveJson(QStringLiteral("areas.json"), m_areas.toJson());
        m_areasDirty = false;
    }
    if (m_notesDirty) {
        saveJson(QStringLiteral("notes.json"), m_notes.toJson());
        m_notesDirty = false;
    }
}

void Organizer::start()
{
    // Notes that are open belong to a notes app that ran before the service: it announces
    // them again. Never started for that.
    m_notesClient.call(QStringLiteral("reannounceNotes"), {}, false);
}

void Organizer::shutdown()
{
    closeOverlay();
    m_labelTimer.stop();
    if (m_label) {
        m_label->hide();
    }
    for (const auto &[hwnd, window] : m_service.m_windows) {
        if (!window.noteId.isEmpty()) {
            storeNotePlacement(window);
        }
    }
    persist();
}

void Organizer::markGroupsDirty()
{
    m_groupsDirty = true;
    m_service.saveStateLater();
}

void Organizer::markAreasDirty()
{
    m_areasDirty = true;
    m_service.saveStateLater();
}

void Organizer::markNotesDirty()
{
    m_notesDirty = true;
    m_service.saveStateLater();
}

void Organizer::prunePending()
{
    const auto now = SteadyClock::now();
    m_launches.removeIf([now](const PendingLaunch &launch) {
        return launch.deadline < now;
    });
    m_expectedNotes.removeIf([now](const QHash<HWND, ExpectedNote>::iterator entry) {
        return entry.value().deadline < now;
    });
}

bool Organizer::isLocked(int monitor) const
{
    return m_service.isLocked(monitor);
}

// ---- Windows --------------------------------------------------------------------------------

bool Organizer::placeLaunched(ManagedWindow &window)
{
    prunePending();
    for (int i = 0; i < m_launches.size(); ++i) {
        const PendingLaunch launch = m_launches[i];
        if (launch.appKey != window.appKey) {
            continue;
        }
        const int monitor = m_service.monitorIndexOf(launch.canvasKey);
        if (monitor < 0 || !window.onCanvas()) {
            return false;
        }
        m_launches.removeAt(i);
        const Canvas &canvas = m_service.canvas(launch.canvasKey);
        window.groupId = launch.groupId;
        window.groupMember = launch.member;
        if (launch.target.width() >= 50 && launch.target.height() >= 50 && !window.maximized) {
            const QSizeF limit = m_service.m_monitors[std::size_t(monitor)].work.size() / std::max(canvas.camera.zoom, 0.1);
            m_service.resizeFrame(window, launch.target.size().boundedTo(limit));
        }
        window.canvasKey = launch.canvasKey;
        window.canvasPos = launch.target.topLeft();
        window.lastRequest = POINT{LONG_MIN, LONG_MIN};
        qCDebug(lcService) << "placed" << window.appKey << "in group" << launch.groupId << "at" << launch.target;
        return true;
    }
    return false;
}

bool Organizer::placeNote(ManagedWindow &window)
{
    prunePending();
    const auto &monitors = m_service.m_monitors;
    if (const std::optional<InfinitDesktop::NotePlacement> placement = m_notes.find(window.noteId)) {
        int index = InfinitDesktop::bestOutputMatch(placement->output, monitorIdentities());
        if (index < 0) {
            index = m_service.monitorIndexOf(window.canvasKey);
        }
        if (index < 0) {
            return false;
        }
        const auto &monitor = monitors[std::size_t(index)];
        window.canvasKey = monitor.key;
        if (placement->pinned) {
            const QRectF &work = monitor.work;
            QPointF screenPos = monitor.viewport.topLeft() + placement->screenOffset;
            screenPos.setX(std::clamp(screenPos.x(), work.left(), std::max(work.left(), work.right() - window.size.width())));
            screenPos.setY(std::clamp(screenPos.y(), work.top(), std::max(work.top(), work.bottom() - window.size.height())));
            window.pinned = true;
            if (placement->keepAbove) {
                SetWindowPos(window.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                window.pinnedAbove = true;
            }
            moveFrameTo(window.hwnd, rounded(screenPos));
            window.lastRequest = rounded(screenPos);
            return false;
        }
        window.canvasPos = placement->canvasPos;
        window.lastRequest = POINT{LONG_MIN, LONG_MIN};
        return false;
    }

    // A new note: at the pointer, or where the canvas menu was opened.
    POINT cursor = {};
    GetCursorPos(&cursor);
    QPointF screenPos(cursor.x, cursor.y);
    int index = m_service.monitorAt(screenPos);
    if (!m_newNote.monitorKey.isEmpty() && m_newNote.deadline > SteadyClock::now()) {
        const int named = m_service.monitorIndexOf(m_newNote.monitorKey);
        if (named >= 0) {
            index = named;
            screenPos = m_newNote.screenPos;
        }
    }
    m_newNote = {};
    if (index < 0) {
        return true;
    }
    const auto &monitor = monitors[std::size_t(index)];
    const Canvas &canvas = m_service.canvas(monitor.key);
    const Camera camera = canvas.overview ? canvas.camera : canvas.committed;
    const QRectF &work = monitor.work;
    if (!work.contains(screenPos)) {
        // From the tray icon: in the middle instead of on the task bar.
        screenPos = work.center() - QPointF(window.size.width() / 2, window.size.height() / 2) * camera.zoom + QPointF(24, 16);
    }
    // The note's top left a little above and left of the pointer, fully on screen.
    QPointF topLeft = screenPos - QPointF(24, 16);
    topLeft.setX(std::clamp(topLeft.x(), work.left(), std::max(work.left(), work.right() - window.size.width() * camera.zoom)));
    topLeft.setY(std::clamp(topLeft.y(), work.top(), std::max(work.top(), work.bottom() - window.size.height() * camera.zoom)));
    window.canvasKey = monitor.key;
    window.canvasPos = camera.screenToCanvas(topLeft, monitor.viewport);
    window.lastRequest = POINT{LONG_MIN, LONG_MIN};
    return true;
}

void Organizer::windowAdopted(ManagedWindow &window)
{
    if (!window.onCanvas()) {
        return;
    }
    const auto it = m_service.m_canvases.find(window.canvasKey);
    if (it == m_service.m_canvases.end()) {
        return;
    }
    const QList<Area> areas = areasOf(it->second);
    const int index = InfinitDesktop::areaForWindow(areas, m_service.canvasRect(window));
    if (index >= 0) {
        window.areaId = areas[index].id;
        scheduleAreaUpdate(window.canvasKey);
    }
}

void Organizer::windowArranged(ManagedWindow &window)
{
    if (!window.noteId.isEmpty()) {
        storeNotePlacement(window);
    }
    if (!window.onCanvas()) {
        return;
    }
    const auto canvasIt = m_service.m_canvases.find(window.canvasKey);
    if (canvasIt == m_service.m_canvases.end()) {
        return;
    }
    const Canvas &canvas = canvasIt->second;
    // Areas: the window belongs to the area its centre was dropped in.
    const QList<Area> areas = areasOf(canvas);
    const int index = InfinitDesktop::areaForWindow(areas, m_service.canvasRect(window));
    const QString areaId = index >= 0 ? areas[index].id : QString();
    if (areaId != window.areaId || !areaId.isEmpty()) {
        window.areaId = areaId;
        updateAreas(canvas);
    }

    // Groups: the place follows the windows, the arrangement as the group says.
    if (window.groupId.isEmpty()) {
        return;
    }
    const Group *stored = m_groups.find(window.groupId);
    if (!stored) {
        window.groupId.clear();
        window.groupMember = -1;
        return;
    }
    Group group = *stored;
    const QList<QRectF> rects = memberRects(group);
    if (group.layoutMode == InfinitDesktop::GroupLayoutMode::Automatic) {
        InfinitDesktop::captureLayout(group, rects);
    } else {
        InfinitDesktop::followWindows(group, rects);
    }
    const int monitor = m_service.monitorIndexOf(canvas.key);
    if (monitor >= 0) {
        group.output = outputIdentity(m_service.m_monitors[std::size_t(monitor)].info);
    }
    m_groups.update(group);
    markGroupsDirty();
    Q_EMIT ipcEvent(QStringLiteral("groupsChanged"), {});
}

void Organizer::windowResized(ManagedWindow &window)
{
    if (!window.areaId.isEmpty()) {
        scheduleAreaUpdate(window.canvasKey);
    }
}

void Organizer::windowGone(ManagedWindow &window, bool destroyed)
{
    if (!window.noteId.isEmpty()) {
        storeNotePlacement(window);
    }
    if (!window.areaId.isEmpty()) {
        if (destroyed) {
            window.areaId.clear();
        }
        // Recomputed once the window is out of the list of open windows.
        scheduleAreaUpdate(window.canvasKey);
    }
}

void Organizer::pinChanged(ManagedWindow &window)
{
    if (window.noteId.isEmpty()) {
        return;
    }
    storeNotePlacement(window);
    Q_EMIT ipcEvent(QStringLiteral("notePinnedChanged"), QJsonObject{
                                                             {QStringLiteral("id"), window.noteId},
                                                             {QStringLiteral("pinned"), window.pinned},
                                                             {QStringLiteral("keepAbove"), window.pinnedAbove},
                                                         });
}

std::optional<QString> Organizer::takeExpectedNote(HWND hwnd)
{
    if (m_expectedNotes.isEmpty()) {
        return std::nullopt;
    }
    prunePending();
    const auto it = m_expectedNotes.find(hwnd);
    if (it == m_expectedNotes.end()) {
        return std::nullopt;
    }
    const QString id = it.value().id;
    m_expectedNotes.erase(it);
    return id;
}

void Organizer::handleDestroyed(HWND hwnd)
{
    // Handles are reused: an announcement never outlives its window.
    m_expectedNotes.remove(hwnd);
}

QList<QRectF> Organizer::memberRects(const Group &group) const
{
    QList<QRectF> rects(group.members.size());
    for (const auto &[hwnd, window] : m_service.m_windows) {
        if (window.groupId != group.id || window.groupMember < 0 || window.groupMember >= rects.size() || !window.onCanvas()) {
            continue;
        }
        rects[window.groupMember] = m_service.canvasRect(window);
    }
    return rects;
}

// ---- Areas ----------------------------------------------------------------------------------

QList<InfinitDesktop::OutputIdentity> Organizer::monitorIdentities() const
{
    QList<InfinitDesktop::OutputIdentity> identities;
    for (const auto &monitor : m_service.m_monitors) {
        identities.append(outputIdentity(monitor.info));
    }
    return identities;
}

bool Organizer::areaBelongsTo(const Area &area, int monitor, const QList<InfinitDesktop::OutputIdentity> &identities)
{
    if (monitor < 0 || monitor >= identities.size()) {
        return false;
    }
    if (identities.size() == 1) {
        return InfinitDesktop::outputMatchScore(area.output, identities.front()) > 0;
    }
    // Two monitors of the same model match equally well: the best one wins, the first of them.
    return InfinitDesktop::bestOutputMatch(area.output, identities) == monitor;
}

QList<Area> Organizer::areasOf(const Canvas &canvas) const
{
    QList<Area> result;
    if (m_areas.areas().isEmpty()) {
        return result;
    }
    const int monitor = m_service.monitorIndexOf(canvas.key);
    const QList<InfinitDesktop::OutputIdentity> identities = monitorIdentities();
    for (const Area &area : m_areas.areas()) {
        if (areaBelongsTo(area, monitor, identities)) {
            result.append(area);
        }
    }
    return result;
}

void Organizer::updateAreas(const Canvas &canvas)
{
    const int monitor = m_service.monitorIndexOf(canvas.key);
    if (monitor < 0) {
        return;
    }
    const InfinitDesktop::AreaMetrics metrics{qreal(m_service.m_settings.areaPadding), QSizeF(320, 200)};
    const QList<InfinitDesktop::OutputIdentity> identities = monitorIdentities();
    bool changed = false;
    for (Area &area : m_areas.areas()) {
        if (!areaBelongsTo(area, monitor, identities)) {
            continue;
        }
        QList<QRectF> members;
        for (const auto &[hwnd, window] : m_service.m_windows) {
            if (window.areaId == area.id && window.canvasKey == canvas.key && window.onCanvas()) {
                members.append(m_service.canvasRect(window));
            }
        }
        const QRectF rect = InfinitDesktop::dynamicAreaRect(area, members, metrics);
        if (rect != area.rect) {
            area.rect = rect;
            changed = true;
        }
    }
    if (changed) {
        markAreasDirty();
        m_service.canvas(canvas.key).dirty = true;
        m_service.requestFlush();
        Q_EMIT ipcEvent(QStringLiteral("areasChanged"), {});
    }
}

void Organizer::scheduleAreaUpdate(const QString &canvasKey)
{
    if (m_areaUpdates.isEmpty()) {
        // Applications resize themselves in bursts: one update per event loop round.
        QTimer::singleShot(0, this, [this]() {
            const QSet<QString> keys = std::exchange(m_areaUpdates, {});
            for (const QString &key : keys) {
                if (auto it = m_service.m_canvases.find(key); it != m_service.m_canvases.end()) {
                    updateAreas(it->second);
                }
            }
        });
    }
    m_areaUpdates.insert(canvasKey);
}

void Organizer::createAreaFromRect(const QRectF &canvasRect, int monitor)
{
    if (monitor < 0 || monitor >= int(m_service.m_monitors.size())) {
        return;
    }
    const auto &state = m_service.m_monitors[std::size_t(monitor)];
    Area area;
    area.name = m_areas.nextName(localized(L"Area %1", L"Bereich %1"));
    area.output = outputIdentity(state.info);
    area.drawnRect = canvasRect;
    area.dynamic = true;
    Area *added = m_areas.add(area);
    if (!added) {
        flash(localized(L"No more areas can be created.", L"Es können keine weiteren Bereiche angelegt werden."), monitor);
        return;
    }
    const QString id = added->id;
    const QString name = added->name;
    // Windows already inside belong to it.
    for (auto &[hwnd, window] : m_service.m_windows) {
        if (window.canvasKey == state.key && window.onCanvas() && canvasRect.contains(m_service.canvasRect(window).center())) {
            window.areaId = id;
        }
    }
    markAreasDirty();
    updateAreas(m_service.canvas(state.key));
    m_service.canvas(state.key).dirty = true;
    m_service.requestFlush();
    flash(localized(L"Area “%1” created", L"Bereich „%1“ angelegt").arg(name), monitor);
    Q_EMIT ipcEvent(QStringLiteral("areasChanged"), {});
}

bool Organizer::setAreasJson(const QString &json)
{
    const std::optional<QJsonObject> object = parseObject(json);
    if (!object) {
        return false;
    }
    m_areas.fromJson(*object);
    // Windows of areas that are gone belong to none.
    for (auto &[hwnd, window] : m_service.m_windows) {
        if (!window.areaId.isEmpty() && m_areas.indexOf(window.areaId) < 0) {
            window.areaId.clear();
        }
    }
    markAreasDirty();
    for (auto &[key, canvas] : m_service.m_canvases) {
        updateAreas(canvas);
        canvas.dirty = true;
    }
    m_service.requestFlush();
    Q_EMIT ipcEvent(QStringLiteral("areasChanged"), {});
    return true;
}

QList<OverviewArea> Organizer::overviewAreas(const Canvas &canvas, const QRectF &viewport, qreal dpiScale) const
{
    QList<OverviewArea> result;
    const qreal textHeight = std::clamp(14.0 * std::sqrt(canvas.camera.zoom), 10.0, 18.0) * dpiScale;
    for (const Area &area : areasOf(canvas)) {
        const QRectF rect = canvas.camera.canvasToScreen(area.rect, viewport);
        if (!rect.intersects(viewport.adjusted(-4, -4, 4, 4))) {
            continue;
        }
        result.append(OverviewArea{rect, areaColor(area.color), area.name, textHeight});
    }
    return result;
}

// ---- Groups ---------------------------------------------------------------------------------

QString Organizer::groupsJson() const
{
    QJsonObject root = m_groups.toJson();
    // How many windows of each group are open, for the settings app.
    QJsonArray groups = root.value(QStringLiteral("groups")).toArray();
    for (int i = 0; i < groups.size(); ++i) {
        QJsonObject group = groups[i].toObject();
        const QString id = group.value(QStringLiteral("id")).toString();
        int open = 0;
        for (const auto &[hwnd, window] : m_service.m_windows) {
            if (window.groupId == id) {
                ++open;
            }
        }
        group.insert(QStringLiteral("openWindows"), open);
        groups[i] = group;
    }
    root.insert(QStringLiteral("groups"), groups);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool Organizer::setGroupsJson(const QString &json)
{
    const std::optional<QJsonObject> object = parseObject(json);
    if (!object) {
        return false;
    }
    m_groups.fromJson(*object);
    for (auto &[hwnd, window] : m_service.m_windows) {
        const Group *group = window.groupId.isEmpty() ? nullptr : m_groups.find(window.groupId);
        if (!group || window.groupMember >= group->members.size()) {
            window.groupId.clear();
            window.groupMember = -1;
        }
    }
    markGroupsDirty();
    Q_EMIT ipcEvent(QStringLiteral("groupsChanged"), {});
    return true;
}

QList<std::pair<QString, QString>> Organizer::groupEntries() const
{
    QList<std::pair<QString, QString>> entries;
    for (const Group &group : m_groups.groups()) {
        entries.append({group.id, group.name});
    }
    return entries;
}

int Organizer::monitorForGroup(const Group &group) const
{
    if (!group.output.name.isEmpty() || group.output.hasEdid()) {
        const int index = InfinitDesktop::bestOutputMatch(group.output, monitorIdentities());
        if (index >= 0) {
            return index;
        }
    }
    POINT cursor = {};
    GetCursorPos(&cursor);
    return m_service.monitorAt(QPointF(cursor.x, cursor.y));
}

void Organizer::fillMissingGeometry(Group &group, int monitor)
{
    QList<int> missing;
    for (int i = 0; i < group.members.size(); ++i) {
        if (group.members[i].geometry.isEmpty()) {
            missing.append(i);
        }
    }
    if (missing.isEmpty()) {
        return;
    }
    const auto &state = m_service.m_monitors[std::size_t(monitor)];
    // Sizes of open windows of these applications, else a common default.
    QList<QSizeF> sizes;
    for (int i : std::as_const(missing)) {
        QSizeF size(1000, 700);
        for (const auto &[hwnd, window] : m_service.m_windows) {
            if (window.appKey == group.members[i].appKey && !window.size.isEmpty()) {
                size = window.size;
                break;
            }
        }
        sizes.append(size);
    }
    const QList<QRectF> arranged = InfinitDesktop::autoArrange(sizes, 3, 24);
    const QRectF existing = group.layoutBounds();
    const QPointF offset = existing.isNull() ? QPointF() : QPointF(existing.right() + 24, existing.top());
    for (int k = 0; k < missing.size() && k < arranged.size(); ++k) {
        group.members[missing[k]].geometry = arranged[k].translated(offset);
    }
    if (existing.isNull()) {
        // A new group without a place yet: in the middle of the view.
        const QRectF visible = m_service.targetCamera(m_service.canvas(state.key)).visibleCanvasRect(state.viewport);
        group.origin = visible.center() - group.layoutBounds().center();
        group.output = outputIdentity(state.info);
    }
    m_groups.update(group);
    markGroupsDirty();
}

void Organizer::openGroup(int index)
{
    if (index < 0 || index >= m_groups.groups().size()) {
        flash(localized(L"There is no group %1. Create one with Win+Alt+G or in the settings app.",
                 L"Es gibt keine Gruppe %1. Legen Sie eine mit Win+Alt+G oder in der Einstellungs-App an.")
                  .arg(index + 1));
        return;
    }
    openGroup(m_groups.groups()[index].id);
}

void Organizer::openGroup(const QString &id)
{
    const Group *stored = m_groups.find(id);
    if (!stored || stored->members.isEmpty() || m_service.m_safeMode) {
        return;
    }
    Group group = *stored;
    const int monitor = monitorForGroup(group);
    if (monitor < 0 || isLocked(monitor)) {
        return;
    }
    const QString key = m_service.m_monitors[std::size_t(monitor)].key;
    fillMissingGeometry(group, monitor);

    QList<InfinitDesktop::GroupCandidate> candidates;
    for (const auto &[hwnd, window] : m_service.m_windows) {
        if (!window.noteId.isEmpty() || window.hidden || window.moving || !IsWindow(hwnd)) {
            continue;
        }
        InfinitDesktop::GroupCandidate candidate;
        candidate.handle = quint64(reinterpret_cast<quintptr>(hwnd));
        candidate.appKey = window.appKey;
        candidate.canvasRect = m_service.canvasRect(window);
        candidate.groupId = window.groupId;
        // Pinned, fullscreen and elevated windows count as open, but stay where they are.
        candidate.movable = !window.pinned && !window.fullscreen && !window.unmovable;
        candidates.append(candidate);
    }
    const InfinitDesktop::GroupPlan plan = InfinitDesktop::planGroup(group, candidates);

    HWND first = nullptr;
    int firstMember = -1;
    for (const InfinitDesktop::GroupPlan::Move &move : plan.moves) {
        const HWND hwnd = reinterpret_cast<HWND>(quintptr(move.handle));
        auto it = m_service.m_windows.find(hwnd);
        if (it == m_service.m_windows.end() || !IsWindow(hwnd)) {
            continue;
        }
        ManagedWindow &window = it->second;
        bool restored = false;
        if (window.minimized) {
            // Asynchronous: a hung application cannot block the service.
            ShowWindowAsync(hwnd, SW_SHOWNOACTIVATE);
            window.minimized = false;
            restored = true;
        }
        if (!window.maximized && move.target.width() >= 50 && move.target.height() >= 50
            && (std::abs(window.size.width() - move.target.width()) > 2 || std::abs(window.size.height() - move.target.height()) > 2)) {
            m_service.resizeFrame(window, move.target.size());
        }
        window.groupId = group.id;
        window.groupMember = move.member;
        if (window.canvasKey != key) {
            m_service.canvas(window.canvasKey).dirty = true;
        }
        window.canvasKey = key;
        window.canvasPos = move.target.topLeft();
        window.lastRequest = POINT{LONG_MIN, LONG_MIN};
        if (restored) {
            // Windows puts it back where it was minimized, a moment later: once more then.
            placeAgainLater(hwnd, key, move.target.topLeft());
        }
        if (!first || move.member == 0 || (firstMember != 0 && move.member < firstMember)) {
            first = hwnd;
            firstMember = move.member;
        }
    }
    m_service.canvas(key).dirty = true;

    int started = 0;
    if (m_service.m_settings.groupStartsApplications) {
        prunePending();
        for (int member : plan.launches) {
            const InfinitDesktop::GroupMember &m = group.members[member];
            if (m.launch.isEmpty() || m_launches.size() >= 64) {
                continue;
            }
            launch(m.launch);
            m_launches.append(PendingLaunch{m.appKey, group.id, member, key, plan.targetOf(member, group), SteadyClock::now() + LaunchTimeout});
            ++started;
        }
    }

    m_lastGroup = group.id;
    revealGroup(group, monitor);
    m_service.requestFlush();
    if (first) {
        m_service.activate(first);
    }
    if (started > 0) {
        flash((started == 1 ? localized(L"Group “%2”: starting one application", L"Gruppe „%2“: eine Anwendung wird gestartet")
                            : localized(L"Group “%2”: starting %1 applications", L"Gruppe „%2“: %1 Anwendungen werden gestartet").arg(started))
                  .arg(group.name),
              monitor);
    } else {
        flash(localized(L"Group “%1”", L"Gruppe „%1“").arg(group.name), monitor, 1200ms);
    }
    qCDebug(lcService) << "opened group" << group.name << "moved" << plan.moves.size() << "started" << started;
}

void Organizer::placeAgainLater(HWND hwnd, const QString &canvasKey, const QPointF &canvasPos)
{
    QTimer::singleShot(300, this, [this, hwnd, canvasKey, canvasPos]() {
        // Looked up again: the window may be gone by now.
        auto it = m_service.m_windows.find(hwnd);
        if (it == m_service.m_windows.end() || !IsWindow(hwnd) || !it->second.onCanvas() || m_service.monitorIndexOf(canvasKey) < 0) {
            return;
        }
        it->second.canvasKey = canvasKey;
        it->second.canvasPos = canvasPos;
        it->second.lastRequest = POINT{LONG_MIN, LONG_MIN};
        m_service.canvas(canvasKey).dirty = true;
        m_service.requestFlush();
    });
}

void Organizer::revealGroup(const Group &group, int monitor)
{
    const QRectF bounds = group.canvasBounds();
    if (bounds.isEmpty() || monitor < 0) {
        return;
    }
    const auto &state = m_service.m_monitors[std::size_t(monitor)];
    Canvas &canvas = m_service.canvas(state.key);
    const Camera base = m_service.targetCamera(canvas);
    const QRectF &area = state.work;
    const bool fits = bounds.width() <= area.width() - 2 * GroupMargin && bounds.height() <= area.height() - 2 * GroupMargin;
    Camera target;
    if (fits || !m_service.m_settings.groupZoomToFit) {
        target = InfinitDesktop::revealCamera(base, bounds, state.viewport, area, InfinitDesktop::RevealMode::Center, GroupMargin, 1.0);
    } else {
        target = InfinitDesktop::fitCamera(bounds, state.viewport, area, GroupMargin, m_service.m_settings.minZoom, 1.0);
    }
    m_service.animateTo(canvas, target);
}

void Organizer::launch(const QString &command)
{
    const LaunchCommand launch = launchCommand(command);
    if (launch.file.empty()) {
        return;
    }
    // ShellExecuteEx can take a while (shell namespace, a slow disk): on a thread of its own
    // that only owns its copy of the command.
    try {
        std::thread([file = launch.file]() {
            const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
            SHELLEXECUTEINFOW info = {};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
            info.lpVerb = L"open";
            info.lpFile = file.c_str();
            info.nShow = SW_SHOWNORMAL;
            if (!ShellExecuteExW(&info)) {
                qCWarning(lcService) << "cannot start" << QString::fromStdWString(file) << "error" << GetLastError();
            }
            if (SUCCEEDED(com)) {
                CoUninitialize();
            }
        }).detach();
    } catch (const std::system_error &error) {
        qCWarning(lcService) << "cannot start a thread for" << command << error.what();
    }
}

void Organizer::createGroupFromRect(const QRectF &canvasRect, int monitor)
{
    if (monitor < 0 || monitor >= int(m_service.m_monitors.size())) {
        return;
    }
    const auto &state = m_service.m_monitors[std::size_t(monitor)];
    QList<HWND> windows;
    QList<QRectF> rects;
    for (const auto &[hwnd, window] : m_service.m_windows) {
        if (window.noteId.isEmpty() && window.canvasKey == state.key && window.onCanvas() && !window.appKey.isEmpty()) {
            windows.append(hwnd);
            rects.append(m_service.canvasRect(window));
        }
    }
    const QList<int> inside = InfinitDesktop::windowsInside(canvasRect, rects);
    if (inside.isEmpty()) {
        flash(localized(L"No windows in the selection", L"Keine Fenster in der Auswahl"), monitor);
        return;
    }
    Group group;
    group.name = m_groups.nextName(localized(L"Group %1", L"Gruppe %1"));
    group.output = outputIdentity(state.info);
    group.layoutMode = m_service.m_settings.groupLayoutAutomatic ? InfinitDesktop::GroupLayoutMode::Automatic : InfinitDesktop::GroupLayoutMode::Manual;
    QList<QRectF> memberRects;
    QList<HWND> members;
    for (int index : inside) {
        if (group.members.size() >= InfinitDesktop::GroupStore::MaxMembers) {
            break;
        }
        const ManagedWindow &window = m_service.m_windows.at(windows[index]);
        group.members.append(InfinitDesktop::GroupMember{window.appKey, window.launch, QRectF()});
        memberRects.append(rects[index]);
        members.append(windows[index]);
    }
    InfinitDesktop::captureLayout(group, memberRects);
    Group *added = m_groups.add(group);
    if (!added) {
        flash(localized(L"No more groups can be created.", L"Es können keine weiteren Gruppen angelegt werden."), monitor);
        return;
    }
    const QString id = added->id;
    const QString name = added->name;
    for (int k = 0; k < members.size() && k < added->members.size(); ++k) {
        ManagedWindow &window = m_service.m_windows.at(members[k]);
        window.groupId = id;
        window.groupMember = k;
    }
    const int slot = m_groups.indexOf(id) + 1;
    m_lastGroup = id;
    markGroupsDirty();
    if (slot <= GroupShortcutCount) {
        flash(localized(L"Group “%1” created: open it with Win+Alt+F%2", L"Gruppe „%1“ angelegt: öffnen mit Win+Alt+F%2").arg(name).arg(slot), monitor, 3000ms);
    } else {
        flash(localized(L"Group “%1” created", L"Gruppe „%1“ angelegt").arg(name), monitor);
    }
    Q_EMIT ipcEvent(QStringLiteral("groupsChanged"), {});
}

void Organizer::saveGroupLayout(const QString &id)
{
    QString groupId = id;
    if (groupId.isEmpty()) {
        const auto it = m_service.m_windows.find(GetForegroundWindow());
        groupId = it != m_service.m_windows.end() && !it->second.groupId.isEmpty() ? it->second.groupId : m_lastGroup;
    }
    const Group *stored = m_groups.find(groupId);
    if (!stored) {
        flash(localized(L"The active window is in no group", L"Das aktive Fenster gehört zu keiner Gruppe"));
        return;
    }
    Group group = *stored;
    InfinitDesktop::captureLayout(group, memberRects(group));
    m_groups.update(group);
    markGroupsDirty();
    flash(localized(L"Arrangement of group “%1” saved", L"Anordnung der Gruppe „%1“ gespeichert").arg(group.name));
    Q_EMIT ipcEvent(QStringLiteral("groupsChanged"), {});
}

// ---- Notes ----------------------------------------------------------------------------------

void Organizer::newNote(const std::optional<QPointF> &screenPos)
{
    if (!m_service.m_settings.notesEnabled) {
        flash(localized(L"Notes are switched off in the settings", L"Notizen sind in den Einstellungen ausgeschaltet"));
        return;
    }
    QPointF pos;
    if (screenPos) {
        pos = *screenPos;
    } else {
        POINT cursor = {};
        GetCursorPos(&cursor);
        pos = QPointF(cursor.x, cursor.y);
    }
    const int monitor = m_service.monitorAt(pos);
    m_newNote.monitorKey = monitor >= 0 ? m_service.m_monitors[std::size_t(monitor)].key : QString();
    m_newNote.screenPos = pos;
    m_newNote.deadline = SteadyClock::now() + NewNoteTimeout;
    m_notesClient.call(QStringLiteral("newNote"), QJsonObject{{QStringLiteral("x"), pos.x()}, {QStringLiteral("y"), pos.y()}});
}

void Organizer::toggleNotes()
{
    m_notesClient.call(QStringLiteral("toggleNotes"));
}

bool Organizer::expectNote(const QString &id, const QString &handle, QString *error)
{
    if (!isValidNoteId(id)) {
        *error = QStringLiteral("invalid note id");
        return false;
    }
    const std::optional<quintptr> value = parseHandle(handle);
    const HWND hwnd = value ? reinterpret_cast<HWND>(*value) : nullptr;
    if (!hwnd || !IsWindow(hwnd) || GetAncestor(hwnd, GA_ROOT) != hwnd) {
        *error = QStringLiteral("no such top-level window");
        return false;
    }
    if (m_service.m_safeMode) {
        return true;
    }
    if (m_service.m_windows.count(hwnd) || IsWindowVisible(hwnd)) {
        // Already shown (or announced again after a restart of the service).
        m_service.adoptNote(hwnd, id);
        return true;
    }
    prunePending();
    if (m_expectedNotes.size() >= MaxExpectedNotes && !m_expectedNotes.contains(hwnd)) {
        *error = QStringLiteral("too many announced notes");
        return false;
    }
    m_expectedNotes.insert(hwnd, ExpectedNote{id, SteadyClock::now() + ExpectedNoteTimeout});
    return true;
}

void Organizer::storeNotePlacement(const ManagedWindow &window)
{
    int monitor = m_service.monitorIndexOf(window.canvasKey);
    std::optional<QPointF> frameTopLeft;
    if (IsWindow(window.hwnd)) {
        const QRectF frame = m_service.frameOf(window.hwnd);
        if (!frame.isEmpty()) {
            frameTopLeft = frame.topLeft();
            if (window.pinned) {
                monitor = m_service.monitorAt(frame.center());
            }
        }
    }
    if (!frameTopLeft && window.lastRequest.x != LONG_MIN) {
        frameTopLeft = QPointF(window.lastRequest.x, window.lastRequest.y);
    }
    if (monitor < 0 || !frameTopLeft) {
        return;
    }
    const auto &state = m_service.m_monitors[std::size_t(monitor)];
    InfinitDesktop::NotePlacement placement;
    placement.id = window.noteId;
    placement.output = outputIdentity(state.info);
    placement.screenOffset = *frameTopLeft - state.viewport.topLeft();
    placement.canvasPos = window.pinned ? m_service.canvas(state.key).committed.screenToCanvas(*frameTopLeft, state.viewport) : window.canvasPos;
    placement.pinned = window.pinned;
    placement.keepAbove = window.pinnedAbove;
    if (m_notes.store(placement)) {
        markNotesDirty();
    }
}

QJsonObject Organizer::noteState(const QString &id) const
{
    for (const auto &[hwnd, window] : m_service.m_windows) {
        if (window.noteId == id) {
            return QJsonObject{
                {QStringLiteral("id"), id},
                {QStringLiteral("open"), true},
                {QStringLiteral("pinned"), window.pinned},
                {QStringLiteral("keepAbove"), window.pinnedAbove},
            };
        }
    }
    const std::optional<InfinitDesktop::NotePlacement> placement = m_notes.find(id);
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("open"), false},
        {QStringLiteral("pinned"), placement && placement->pinned},
        {QStringLiteral("keepAbove"), placement && placement->keepAbove},
    };
}

void Organizer::setNotePinned(const QString &id, bool pinned, bool keepAbove)
{
    for (auto &[hwnd, window] : m_service.m_windows) {
        if (window.noteId == id) {
            m_service.setPinned(window, pinned, keepAbove);
            return;
        }
    }
    if (std::optional<InfinitDesktop::NotePlacement> placement = m_notes.find(id)) {
        placement->pinned = pinned;
        placement->keepAbove = pinned && keepAbove;
        m_notes.store(*placement);
        markNotesDirty();
        Q_EMIT ipcEvent(QStringLiteral("notePinnedChanged"), QJsonObject{
                                                                 {QStringLiteral("id"), id},
                                                                 {QStringLiteral("pinned"), placement->pinned},
                                                                 {QStringLiteral("keepAbove"), placement->keepAbove},
                                                             });
    }
}

// ---- Overlays -------------------------------------------------------------------------------

int Organizer::overlayMonitor() const
{
    return m_overlayMonitor.isEmpty() ? -1 : m_service.monitorIndexOf(m_overlayMonitor);
}

void Organizer::beginOverlay(Overlay overlay, int monitor)
{
    closeOverlay();
    m_overlay = overlay;
    m_overlayMonitor = m_service.m_monitors[std::size_t(monitor)].key;
    m_service.publishSnapshot();
}

void Organizer::closeOverlay()
{
    if (m_overlay == Overlay::None) {
        return;
    }
    m_overlay = Overlay::None;
    m_overlayMonitor.clear();
    // Before hiding: hiding releases the capture, which must not look like a cancel.
    m_selecting = false;
    m_mapDragging = false;
    m_mapMoved = false;
    for (OverlayWindow *window : {m_veil.get(), m_tint.get(), m_panel.get()}) {
        if (window) {
            window->hide();
        }
    }
    if (m_labelPersistent && m_label) {
        m_label->hide();
        m_labelPersistent = false;
    }
    m_map = InfinitDesktop::MapView{};
    m_service.publishSnapshot();
}

void Organizer::raiseOverlays()
{
    for (OverlayWindow *window : {m_veil.get(), m_tint.get(), m_panel.get(), m_label.get()}) {
        if (window) {
            window->raise();
        }
    }
}

void Organizer::monitorsChanged()
{
    if (m_overlay != Overlay::None) {
        // The overlay covers a monitor that changed or went away: start again if needed.
        closeOverlay();
    }
    m_labelTimer.stop();
    if (m_label) {
        m_label->hide();
    }
    m_labelPersistent = false;
}

void Organizer::refresh()
{
    if (m_overlay == Overlay::Map && m_panel && m_panel->isVisible()) {
        updateMapView();
        renderMap();
    }
}

void Organizer::startSelection(bool group, int monitor)
{
    if (monitor < 0 || monitor >= int(m_service.m_monitors.size()) || isLocked(monitor) || m_service.m_safeMode) {
        return;
    }
    beginOverlay(Overlay::Selection, monitor);
    m_selectGroup = group;
    m_selecting = false;
    if (!m_veil) {
        m_veil = std::make_unique<OverlayWindow>(OverlayWindow::Style::Veil, [this](UINT message, WPARAM wParam, LPARAM lParam) {
            return veilEvent(message, wParam, lParam);
        });
    }
    const auto &state = m_service.m_monitors[std::size_t(monitor)];
    m_veil->setColor(RGB(0, 0, 0), RGB(0, 0, 0), 40);
    m_veil->show(toRect(state.info.bounds));
    flash(group ? localized(L"Draw a rectangle around the windows of the new group (Esc cancels)",
                     L"Ziehen Sie ein Rechteck um die Fenster der neuen Gruppe (Esc bricht ab)")
                : localized(L"Draw the new area (Esc cancels)", L"Ziehen Sie den neuen Bereich auf (Esc bricht ab)"),
          monitor, 0ms);
}

bool Organizer::veilEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
    Q_UNUSED(wParam)
    if (m_overlay == Overlay::Map) {
        if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN) {
            // A click next to the map closes it.
            closeOverlay();
            return true;
        }
        return false;
    }
    if (m_overlay != Overlay::Selection) {
        return false;
    }
    switch (message) {
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
            return true;
        }
        return false;
    case WM_LBUTTONDOWN: {
        m_selecting = true;
        m_selectStart = m_veil->screenPoint(lParam);
        SetCapture(m_veil->window());
        if (!m_tint) {
            m_tint = std::make_unique<OverlayWindow>(OverlayWindow::Style::Tint);
        }
        const QColor color = m_selectGroup ? AccentColor : areaColor(int(m_areas.areas().size() % InfinitDesktop::AreaStore::Colors));
        m_tint->setColor(colorRef(color), colorRef(color.lighter(150)), 110);
        m_tint->show(QRect(m_selectStart, QSize(1, 1)));
        return true;
    }
    case WM_MOUSEMOVE:
        if (m_selecting && m_tint) {
            m_tint->show(QRect(m_selectStart, m_veil->screenPoint(lParam)).normalized());
        }
        return true;
    case WM_LBUTTONUP:
        if (m_selecting) {
            m_selecting = false;
            finishSelection(QRect(m_selectStart, m_veil->screenPoint(lParam)).normalized());
        }
        return true;
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        closeOverlay();
        return true;
    case WM_CAPTURECHANGED:
        // Someone took the mouse (a dialog, Alt+Tab): the selection is cancelled.
        if (m_selecting) {
            closeOverlay();
        }
        return false;
    default:
        return false;
    }
}

void Organizer::finishSelection(const QRect &screenRect)
{
    const bool group = m_selectGroup;
    const int monitor = overlayMonitor();
    closeOverlay();
    if (monitor < 0 || screenRect.width() < MinimumSelection || screenRect.height() < MinimumSelection) {
        return;
    }
    const auto &state = m_service.m_monitors[std::size_t(monitor)];
    const Canvas &canvas = m_service.canvas(state.key);
    // What the user sees: the overview's camera while zoomed out.
    const QRectF canvasRect = canvas.camera.screenToCanvas(QRectF(screenRect), state.viewport);
    if (group) {
        createGroupFromRect(canvasRect, monitor);
    } else {
        createAreaFromRect(canvasRect, monitor);
    }
}

void Organizer::toggleMap(int monitor)
{
    if (m_overlay == Overlay::Map) {
        closeOverlay();
        return;
    }
    if (monitor < 0 || monitor >= int(m_service.m_monitors.size()) || isLocked(monitor) || m_service.m_safeMode) {
        return;
    }
    beginOverlay(Overlay::Map, monitor);
    const auto &state = m_service.m_monitors[std::size_t(monitor)];
    if (!m_veil) {
        m_veil = std::make_unique<OverlayWindow>(OverlayWindow::Style::Veil, [this](UINT message, WPARAM wParam, LPARAM lParam) {
            return veilEvent(message, wParam, lParam);
        });
    }
    if (!m_panel) {
        m_panel = std::make_unique<OverlayWindow>(OverlayWindow::Style::Panel, [this](UINT message, WPARAM wParam, LPARAM lParam) {
            return panelEvent(message, wParam, lParam);
        });
    }
    const QRectF &work = state.work;
    const qreal share = std::clamp(m_service.m_settings.mapSize, 30, 95) / 100.0;
    const QSize size(int(work.width() * share), int(work.height() * share));
    m_mapPanel = QRect(QPoint(int(work.center().x()) - size.width() / 2, int(work.center().y()) - size.height() / 2), size);
    m_mapDragging = false;
    m_veil->setColor(RGB(0, 0, 0), RGB(0, 0, 0), 90);
    m_veil->show(toRect(state.info.bounds));
    m_panel->show(m_mapPanel);
    updateMapView();
    renderMap();
}

void Organizer::updateMapView()
{
    const int monitor = overlayMonitor();
    if (monitor < 0) {
        m_map = InfinitDesktop::MapView{};
        return;
    }
    const auto &state = m_service.m_monitors[std::size_t(monitor)];
    const Canvas &canvas = m_service.canvas(state.key);
    QList<QRectF> content;
    for (const auto &[hwnd, window] : m_service.m_windows) {
        if (window.canvasKey == canvas.key && window.onCanvas()) {
            content.append(m_service.canvasRect(window));
        }
    }
    for (const Area &area : areasOf(canvas)) {
        content.append(area.rect);
    }
    for (const Group &group : m_groups.groups()) {
        if (monitorForGroup(group) == monitor) {
            content.append(group.canvasBounds());
        }
    }
    const QRectF visible = canvas.camera.visibleCanvasRect(state.viewport);
    // Do not jump around while the user drags inside the map: keep the view while the
    // camera stays in it.
    if (m_mapDragging && m_map.isValid() && m_map.canvas.contains(visible)) {
        return;
    }
    const qreal scale = state.info.dpi / 96.0;
    m_map = InfinitDesktop::mapView(content, visible, QRectF(QPointF(0, 0), QSizeF(m_mapPanel.size())), 16 * scale, 0.5, state.viewport.size() * 2);
}

void Organizer::renderMap()
{
    const int monitor = overlayMonitor();
    if (monitor < 0 || !m_panel || !m_map.isValid()) {
        return;
    }
    const auto &state = m_service.m_monitors[std::size_t(monitor)];
    const Canvas &canvas = m_service.canvas(state.key);
    const qreal scale = state.info.dpi / 96.0;
    QImage image(m_mapPanel.size(), QImage::Format_RGB32);
    image.fill(PanelColor);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF panel(QPointF(0, 0), QSizeF(m_mapPanel.size()));
    painter.setPen(QPen(QColor(255, 255, 255, 40), 1));
    painter.drawRect(panel.adjusted(0.5, 0.5, -0.5, -0.5));
    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(std::max(9, int(std::lround(12 * scale))));
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    const auto withAlpha = [](QColor color, qreal alpha) {
        color.setAlphaF(alpha);
        return color;
    };

    // Areas and groups first, windows on top, the visible rect over all.
    for (const Area &area : areasOf(canvas)) {
        const QRectF rect = m_map.toScreen(area.rect).intersected(panel);
        if (rect.isEmpty()) {
            continue;
        }
        const QColor color = areaColor(area.color);
        painter.fillRect(rect, withAlpha(color, 0.18));
        painter.setPen(QPen(withAlpha(color, 0.8), 1.5 * scale));
        painter.drawRect(rect);
        if (rect.width() > 50 * scale) {
            painter.setPen(color.lighter(130));
            painter.drawText(rect.adjusted(6 * scale, 4 * scale, -4 * scale, 0), Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine, area.name);
        }
    }
    for (const Group &group : m_groups.groups()) {
        if (monitorForGroup(group) != monitor) {
            continue;
        }
        const QRectF rect = m_map.toScreen(group.canvasBounds().adjusted(-12, -12, 12, 12)).intersected(panel);
        if (rect.isEmpty()) {
            continue;
        }
        painter.setPen(QPen(withAlpha(AccentColor, 0.9), 1.5 * scale, Qt::DashLine));
        painter.drawRect(rect);
        painter.setPen(AccentColor);
        painter.drawText(QRectF(rect.left() + 4 * scale, rect.top() - 18 * scale, std::max(rect.width(), 120 * scale), 16 * scale),
                         Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, group.name);
    }
    const HWND active = GetForegroundWindow();
    const QList<HWND> order = m_service.stackingOrder();
    for (auto it = order.crbegin(); it != order.crend(); ++it) {
        const auto found = m_service.m_windows.find(*it);
        if (found == m_service.m_windows.end()) {
            continue;
        }
        const ManagedWindow &window = found->second;
        if (window.canvasKey != canvas.key || !window.onCanvas()) {
            continue;
        }
        const QRectF rect = m_map.toScreen(m_service.canvasRect(window)).intersected(panel);
        if (rect.isEmpty()) {
            continue;
        }
        const bool note = !window.noteId.isEmpty();
        painter.fillRect(rect, note ? NoteFill : QColor(58, 61, 68));
        painter.setPen(QPen(*it == active ? AccentColor : QColor(255, 255, 255, 115), (*it == active ? 2.0 : 1.0) * scale));
        painter.drawRect(rect);
        if (!note && rect.width() >= 28 * scale && rect.height() >= 28 * scale) {
            auto icon = m_service.m_icons.find(*it);
            if (icon == m_service.m_icons.end()) {
                icon = m_service.m_icons.insert(*it, windowIcon(*it, 28));
            }
            if (!icon->isNull()) {
                const qreal size = std::min({rect.width(), rect.height(), 48.0 * scale}) * 0.6;
                painter.drawImage(QRectF(rect.center() - QPointF(size / 2, size / 2), QSizeF(size, size)), *icon);
            }
        }
    }
    const QRectF visible = m_map.toScreen(canvas.camera.visibleCanvasRect(state.viewport)).intersected(panel);
    painter.fillRect(visible, withAlpha(AccentColor, 0.12));
    painter.setPen(QPen(AccentColor, 2 * scale));
    painter.drawRect(visible);
    painter.end();
    m_panel->setImage(image);
}

bool Organizer::panelEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (m_overlay != Overlay::Map) {
        return false;
    }
    const int monitor = overlayMonitor();
    if (monitor < 0) {
        if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN) {
            closeOverlay();
            return true;
        }
        return false;
    }
    const auto &state = m_service.m_monitors[std::size_t(monitor)];
    Canvas &canvas = m_service.canvas(state.key);
    switch (message) {
    case WM_LBUTTONDOWN: {
        const QPoint local = m_panel->screenPoint(lParam) - m_mapPanel.topLeft();
        m_mapDragging = true;
        m_mapMoved = false;
        m_mapPressPos = local;
        const QRectF visible = m_map.toScreen(canvas.camera.visibleCanvasRect(state.viewport));
        // Grabbing the visible rect drags it; anywhere else first brings it there.
        m_mapStartCenter = visible.contains(local) ? canvas.camera.center : m_map.toCanvas(local);
        SetCapture(m_panel->window());
        return true;
    }
    case WM_MOUSEMOVE: {
        if (!m_mapDragging) {
            return true;
        }
        const QPoint local = m_panel->screenPoint(lParam) - m_mapPanel.topLeft();
        if ((local - m_mapPressPos).manhattanLength() > 3) {
            m_mapMoved = true;
        }
        if (m_mapMoved) {
            const QPointF center = InfinitDesktop::mapDragCenter(m_map, m_mapStartCenter, QPointF(local - m_mapPressPos));
            canvas.motion.stop();
            m_service.setCamera(canvas, Camera{center, canvas.camera.zoom});
            m_service.requestFlush();
        }
        return true;
    }
    case WM_LBUTTONUP: {
        if (!m_mapDragging) {
            return true;
        }
        const bool moved = m_mapMoved;
        m_mapDragging = false;
        m_mapMoved = false;
        if (GetCapture() == m_panel->window()) {
            ReleaseCapture();
        }
        if (moved) {
            updateMapView();
            renderMap();
            return true;
        }
        // A click: a window flies there and ends the map, empty space moves the view.
        const QPoint local = m_panel->screenPoint(lParam) - m_mapPanel.topLeft();
        const QPointF canvasPos = m_map.toCanvas(local);
        const QList<HWND> order = m_service.stackingOrder();
        for (HWND hwnd : order) {
            const auto found = m_service.m_windows.find(hwnd);
            if (found == m_service.m_windows.end() || found->second.canvasKey != canvas.key || !found->second.onCanvas()) {
                continue;
            }
            if (m_service.canvasRect(found->second).contains(canvasPos)) {
                closeOverlay();
                m_service.activate(hwnd);
                m_service.reveal(hwnd, InfinitDesktop::RevealMode::Center);
                return true;
            }
        }
        m_service.animateTo(canvas, Camera{canvasPos, m_service.targetCamera(canvas).zoom});
        return true;
    }
    case WM_CAPTURECHANGED:
        if (reinterpret_cast<HWND>(lParam) != m_panel->window()) {
            m_mapDragging = false;
            m_mapMoved = false;
        }
        return false;
    case WM_MOUSEWHEEL:
        m_service.zoomAt(canvas, state.viewport.center(), GET_WHEEL_DELTA_WPARAM(wParam) / 120.0);
        return true;
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        closeOverlay();
        return true;
    default:
        return false;
    }
}

QImage Organizer::renderLabel(const QString &message, qreal scale) const
{
    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(std::max(10, int(std::lround(15 * scale))));
    const QFontMetrics metrics(font);
    const QString elided = metrics.elidedText(message, Qt::ElideRight, font.pixelSize() * 40);
    const int padX = int(std::lround(18 * scale));
    const int padY = int(std::lround(10 * scale));
    const QSize size(metrics.horizontalAdvance(elided) + 2 * padX, metrics.height() + 2 * padY);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath frame;
    frame.addRoundedRect(QRectF(0.5, 0.5, size.width() - 1, size.height() - 1), 8 * scale, 8 * scale);
    painter.fillPath(frame, QColor(32, 34, 38, 235));
    painter.setPen(QPen(QColor(255, 255, 255, 40), 1));
    painter.drawPath(frame);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(QPoint(padX, padY), QSize(size.width() - 2 * padX, metrics.height())), Qt::AlignCenter | Qt::TextSingleLine, elided);
    painter.end();
    return image;
}

void Organizer::flash(const QString &message, int monitor, std::chrono::milliseconds duration)
{
    if (message.isEmpty() || m_service.m_monitors.empty()) {
        return;
    }
    if (monitor < 0 || monitor >= int(m_service.m_monitors.size())) {
        POINT cursor = {};
        GetCursorPos(&cursor);
        monitor = m_service.monitorAt(QPointF(cursor.x, cursor.y));
        if (monitor < 0) {
            return;
        }
    }
    const auto &state = m_service.m_monitors[std::size_t(monitor)];
    if (!m_label) {
        m_label = std::make_unique<OverlayWindow>(OverlayWindow::Style::Label);
    }
    const QImage image = renderLabel(message, state.info.dpi / 96.0);
    const QRectF &work = state.work;
    const QPoint topLeft(int(work.center().x()) - image.width() / 2, int(work.top() + work.height() * 0.8) - image.height() / 2);
    m_label->hide();
    m_label->setImage(image);
    m_label->show(QRect(topLeft, image.size()));
    m_labelPersistent = duration.count() <= 0;
    if (m_labelPersistent) {
        m_labelTimer.stop();
    } else {
        m_labelTimer.start(duration);
    }
}

void Organizer::showCanvasMenu(const QPointF &screenPos)
{
    if (m_service.m_safeMode || m_service.m_shuttingDown) {
        return;
    }
    const int monitor = m_service.monitorAt(screenPos);
    Canvas *canvasPtr = m_service.canvasOfMonitor(monitor);
    if (!canvasPtr) {
        return;
    }
    closeOverlay();
    const QString key = canvasPtr->key;
    const QRectF viewport = m_service.m_monitors[std::size_t(monitor)].viewport;
    const QPointF canvasPos = canvasPtr->camera.screenToCanvas(screenPos, viewport);
    const bool overArea = InfinitDesktop::areaAt(areasOf(*canvasPtr), canvasPos) >= 0;
    const bool zoomedOut = !m_service.targetCamera(*canvasPtr).isUnitZoom();
    const QList<CanvasMenuItem> items = canvasMenuItems(m_service.m_settings.notesEnabled, overArea, zoomedOut);

    UniqueMenu menu(CreatePopupMenu());
    if (!menu) {
        return;
    }
    for (CanvasMenuItem item : items) {
        if (item == CanvasMenuItem::Separator) {
            AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
        } else {
            const std::wstring label = menuText(item).toStdWString();
            AppendMenuW(menu.get(), MF_STRING, UINT_PTR(item), label.c_str());
        }
    }
    // A menu needs its owner in the foreground to close when clicking elsewhere; the
    // foreground goes back afterwards unless the choice activates something itself.
    const HWND owner = m_service.m_serviceWindow.get();
    const HWND previous = GetForegroundWindow();
    m_service.forceForeground(owner);
    // While it is open, every key and click belongs to the menu (Esc closes it, not the overview).
    m_service.m_menuOpen = true;
    m_service.publishSnapshot();
    const UINT chosen = TrackPopupMenuEx(menu.get(), TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, int(std::lround(screenPos.x())),
                                         int(std::lround(screenPos.y())), owner, nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
    m_service.m_menuOpen = false;
    m_service.publishSnapshot();
    menu.reset();
    if (chosen != UINT(CanvasMenuItem::NewNote) && previous && previous != owner && IsWindow(previous)) {
        SetForegroundWindow(previous);
    }
    if (m_service.m_shuttingDown || chosen == 0) {
        return;
    }

    // The menu ran its own message loop: everything is looked up again.
    const int index = m_service.monitorIndexOf(key);
    Canvas *canvas = index >= 0 ? &m_service.canvas(key) : nullptr;
    switch (CanvasMenuItem(chosen)) {
    case CanvasMenuItem::Separator:
        break;
    case CanvasMenuItem::NewNote:
        newNote(screenPos);
        break;
    case CanvasMenuItem::ToggleNotes:
        toggleNotes();
        break;
    case CanvasMenuItem::NewGroup:
        startSelection(true, index);
        break;
    case CanvasMenuItem::NewArea:
        startSelection(false, index);
        break;
    case CanvasMenuItem::RemoveArea: {
        if (!canvas) {
            break;
        }
        const QList<Area> areas = areasOf(*canvas);
        const int area = InfinitDesktop::areaAt(areas, canvasPos);
        if (area < 0) {
            break;
        }
        const QString areaId = areas[area].id;
        const QString name = areas[area].name;
        m_areas.remove(areaId);
        for (auto &[hwnd, window] : m_service.m_windows) {
            if (window.areaId == areaId) {
                window.areaId.clear();
            }
        }
        markAreasDirty();
        canvas->dirty = true;
        m_service.requestFlush();
        flash(localized(L"Area “%1” removed", L"Bereich „%1“ entfernt").arg(name), index);
        Q_EMIT ipcEvent(QStringLiteral("areasChanged"), {});
        break;
    }
    case CanvasMenuItem::Map:
        toggleMap(index);
        break;
    case CanvasMenuItem::ShowAll:
        if (canvas) {
            m_service.showAll(*canvas, false);
        }
        break;
    case CanvasMenuItem::ZoomReset:
        if (canvas) {
            m_service.animateTo(*canvas, m_service.targetCamera(*canvas).zoomedAt(screenPos, 1.0, m_service.m_monitors[std::size_t(index)].viewport));
        }
        break;
    case CanvasMenuItem::Settings:
        m_service.openSettingsApp();
        break;
    case CanvasMenuItem::Quit:
        QTimer::singleShot(0, &m_service, &CanvasService::quitRequested);
        break;
    }
}

// ---- IPC ------------------------------------------------------------------------------------

QJsonValue Organizer::handleIpc(const QString &method, const QJsonObject &params, QString *error, bool *handled)
{
    *handled = true;
    const auto string = [&params](const char *key) {
        return params.value(QLatin1String(key)).toString();
    };
    const auto pointer = [&params]() -> std::optional<QPointF> {
        if (!params.contains(QStringLiteral("x")) || !params.contains(QStringLiteral("y"))) {
            return std::nullopt;
        }
        const qreal x = params.value(QStringLiteral("x")).toDouble();
        const qreal y = params.value(QStringLiteral("y")).toDouble();
        if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 1e6 || std::abs(y) > 1e6) {
            return std::nullopt;
        }
        return QPointF(x, y);
    };
    const auto cursorMonitor = [this]() {
        POINT cursor = {};
        GetCursorPos(&cursor);
        return m_service.monitorAt(QPointF(cursor.x, cursor.y));
    };

    // Stored data: also in safe mode (the settings app edits it).
    if (method == QLatin1String("groups")) {
        return groupsJson();
    }
    if (method == QLatin1String("setGroups")) {
        if (!setGroupsJson(string("json"))) {
            *error = QStringLiteral("invalid groups");
        }
        return error->isEmpty();
    }
    if (method == QLatin1String("areas")) {
        return QString::fromUtf8(QJsonDocument(m_areas.toJson()).toJson(QJsonDocument::Compact));
    }
    if (method == QLatin1String("setAreas")) {
        if (!setAreasJson(string("json"))) {
            *error = QStringLiteral("invalid areas");
        }
        return error->isEmpty();
    }
    if (method == QLatin1String("noteState")) {
        return noteState(string("id"));
    }
    if (method == QLatin1String("forgetNote")) {
        if (m_notes.remove(string("id"))) {
            markNotesDirty();
        }
        return true;
    }
    if (method == QLatin1String("retainNotes")) {
        QStringList ids;
        const QJsonArray array = params.value(QStringLiteral("ids")).toArray();
        for (const QJsonValue &value : array) {
            ids.append(value.toString());
        }
        if (m_notes.retain(ids) > 0) {
            markNotesDirty();
        }
        return true;
    }
    if (method == QLatin1String("expectNote")) {
        // The handle may come as a number or as a string.
        const QJsonValue value = params.value(QStringLiteral("hwnd"));
        const QString handle = value.isDouble() ? QString::number(qulonglong(value.toDouble())) : value.toString();
        return expectNote(string("id"), handle, error);
    }
    if (method == QLatin1String("setNotePinned")) {
        setNotePinned(string("id"), params.value(QStringLiteral("pinned")).toBool(), params.value(QStringLiteral("keepAbove")).toBool());
        return noteState(string("id"));
    }
    if (method == QLatin1String("toggleNotes")) {
        toggleNotes();
        return true;
    }
    if (method == QLatin1String("applications")) {
        // Open programs for the settings app's picker, one entry per application.
        QJsonArray result;
        QSet<QString> seen;
        for (const auto &[hwnd, window] : m_service.m_windows) {
            if (window.appKey.isEmpty() || !window.noteId.isEmpty() || seen.contains(window.appKey)) {
                continue;
            }
            seen.insert(window.appKey);
            const TopLevel top = describe(hwnd);
            result.append(QJsonObject{
                {QStringLiteral("app"), window.appKey},
                {QStringLiteral("launch"), window.launch},
                {QStringLiteral("program"), window.program},
                {QStringLiteral("title"), QString::fromStdWString(top.title)},
            });
        }
        return result;
    }

    // Actions need a running canvas.
    static const QStringList actions{QStringLiteral("newNote"),         QStringLiteral("showMap"),  QStringLiteral("groupFromSelection"),
                                     QStringLiteral("areaFromSelection"), QStringLiteral("openGroup"), QStringLiteral("saveGroupLayout")};
    if (!actions.contains(method)) {
        *handled = false;
        return {};
    }
    if (m_service.m_safeMode) {
        *error = QStringLiteral("safe mode");
        return {};
    }
    if (method == QLatin1String("newNote")) {
        newNote(pointer());
        return true;
    }
    if (method == QLatin1String("showMap")) {
        toggleMap(cursorMonitor());
        return m_overlay == Overlay::Map;
    }
    if (method == QLatin1String("groupFromSelection") || method == QLatin1String("areaFromSelection")) {
        startSelection(method == QLatin1String("groupFromSelection"), cursorMonitor());
        return m_overlay == Overlay::Selection;
    }
    if (method == QLatin1String("openGroup")) {
        if (params.contains(QStringLiteral("index"))) {
            openGroup(params.value(QStringLiteral("index")).toInt(-1));
        } else if (m_groups.find(string("id"))) {
            openGroup(string("id"));
        } else {
            *error = QStringLiteral("no such group");
            return {};
        }
        return true;
    }
    if (method == QLatin1String("saveGroupLayout")) {
        saveGroupLayout(string("id"));
        return true;
    }
    *handled = false;
    return {};
}

} // namespace Wid
