/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "areas.h"
#include "groups.h"
#include "minimap.h"
#include "noteplacements.h"
#include "notesclient.h"
#include "overlays.h"
#include "overview.h"

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QTimer>

#include <windows.h>

#include <chrono>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace Wid
{

class CanvasService;
struct Canvas;
struct ManagedWindow;

/// Entries of the canvas menu (also the ids of the native menu).
enum class CanvasMenuItem : unsigned {
    Separator = 0,
    NewNote = 1,
    ToggleNotes,
    NewGroup,
    NewArea,
    RemoveArea,
    Map,
    ShowAll,
    ZoomReset,
    Settings,
    Quit,
};

/// The canvas menu as in KDE Infinite Desktop: notes (if enabled), group and area from a
/// selection, "remove this area" over an area, the map, then the view and the settings.
QList<CanvasMenuItem> canvasMenuItems(bool notesEnabled, bool overArea, bool zoomedOut);

/// How a group member is started: a program path, or an AppUserModelID (started through
/// shell:AppsFolder). Empty for an empty string.
struct LaunchCommand
{
    std::wstring file;
    bool isPath = false;
};
LaunchCommand launchCommand(const QString &launch);

/// Parses the window handle of an IPC request: decimal, or hexadecimal with 0x.
std::optional<quintptr> parseHandle(const QString &text);

/// A valid note id: 1 to 64 letters, digits, '-' or '_'.
bool isValidNoteId(const QString &id);

/**
 * Notes, groups, areas and the map in the canvas service: the Windows counterpart of the
 * KWin effect's Organizer. The logic lives in the shared core (groups.h, areas.h,
 * noteplacements.h, minimap.h); this class connects it to Win32 windows, the notes app
 * (NotesClient), persistence and its overlay windows.
 *
 * Lifetime rules: no window handle is kept across events except the announced notes
 * (removed when the window is destroyed, expiring after a few seconds). Windows are looked up
 * in the service by handle each time; overlay state holds the monitor's key, not its index.
 * Overlay windows are created once and only hidden, never destroyed from their handlers.
 */
class Organizer : public QObject
{
    Q_OBJECT

public:
    explicit Organizer(CanvasService &service);
    ~Organizer() override;

    void load();
    /// Areas and groups without a virtual desktop (from before canvases per desktop, or made
    /// while they were off) go to the current one.
    void claimForCurrentDesktop();
    /// Writes what changed (called by the service's save).
    void persist();
    /// After the service started (or left safe mode): running notes announce themselves.
    void start();
    void shutdown();

    // ---- Hooks from the service --------------------------------------------------------
    /// A new window started for a group: placed and tagged. True if it was one.
    bool placeLaunched(ManagedWindow &window);
    /// A note window announced by the notes app is being adopted: its place. True for a
    /// new note (it gets the focus).
    bool placeNote(ManagedWindow &window);
    /// After a window was adopted: area membership.
    void windowAdopted(ManagedWindow &window);
    /// The user moved or resized a window (move loop or overview drag ended).
    void windowArranged(ManagedWindow &window);
    /// The window changed its size by itself (coalesced).
    void windowResized(ManagedWindow &window);
    /// The window is hidden or about to be forgotten: keeps the place of notes, areas follow.
    void windowGone(ManagedWindow &window, bool destroyed);
    void pinChanged(ManagedWindow &window);
    /// Announced note window of this handle (shown now); removes the announcement.
    std::optional<QString> takeExpectedNote(HWND hwnd);
    void handleDestroyed(HWND hwnd);
    /// The view or the windows changed: the map follows.
    void refresh();
    void monitorsChanged();
    QList<OverviewArea> overviewAreas(const Canvas &canvas, const QRectF &viewport, qreal dpiScale) const;

    bool overlayOpen() const
    {
        return m_overlay != Overlay::None;
    }
    void closeOverlay();
    /// Puts the shown overlays back on top, in their order (after the overview opened).
    void raiseOverlays();

    // ---- Actions (shortcuts, menus, IPC) -------------------------------------------------
    /// The canvas menu at a screen point (runs the native menu; call it from the event loop,
    /// not from inside a window procedure).
    void showCanvasMenu(const QPointF &screenPos);
    void toggleMap(int monitor);
    void startSelection(bool group, int monitor);
    void newNote(const std::optional<QPointF> &screenPos);
    void toggleNotes();
    void openGroup(int index);
    void openGroup(const QString &id);
    void saveGroupLayout(const QString &id = QString());
    /// A short message on a monitor (-1: the one under the pointer); 0 ms: until replaced.
    void flash(const QString &text, int monitor = -1, std::chrono::milliseconds duration = std::chrono::milliseconds(1800));

    /// Group ids and names in shortcut order (tray menu).
    QList<std::pair<QString, QString>> groupEntries() const;

    /// IPC methods of notes, groups, areas and the map; `handled` false for other methods.
    QJsonValue handleIpc(const QString &method, const QJsonObject &params, QString *error, bool *handled);

Q_SIGNALS:
    /// groupsChanged, areasChanged, notePinnedChanged: for the IPC clients.
    void ipcEvent(const QString &event, const QJsonObject &params);

private:
    enum class Overlay {
        None,
        Selection,
        Map,
    };
    struct PendingLaunch
    {
        QString appKey;
        QString groupId;
        int member = -1;
        QString canvasKey;
        QRectF target;
        std::chrono::steady_clock::time_point deadline;
    };
    struct ExpectedNote
    {
        QString id;
        std::chrono::steady_clock::time_point deadline;
    };

    // Persistence
    void markGroupsDirty();
    void markAreasDirty();
    void markNotesDirty();
    void prunePending();

    // Areas
    QList<InfiniteDesktop::Area> areasOf(const Canvas &canvas) const;
    QList<InfiniteDesktop::OutputIdentity> monitorIdentities() const;
    /// Whether something of `stored` desktop is on a canvas of `canvasDesktop`: without
    /// canvases per desktop there is one canvas with everything.
    static bool onDesktop(const QString &stored, const QString &canvasDesktop);
    static bool areaBelongsTo(const InfiniteDesktop::Area &area, int monitor, const QString &desktop, const QList<InfiniteDesktop::OutputIdentity> &identities);
    void updateAreas(const Canvas &canvas);
    void scheduleAreaUpdate(const QString &canvasKey);
    void createAreaFromRect(const QRectF &canvasRect, int monitor);
    bool setAreasJson(const QString &json);

    // Groups
    QString groupsJson() const;
    bool setGroupsJson(const QString &json);
    int monitorForGroup(const InfiniteDesktop::Group &group) const;
    void fillMissingGeometry(InfiniteDesktop::Group &group, int monitor);
    void revealGroup(const InfiniteDesktop::Group &group, int monitor);
    void createGroupFromRect(const QRectF &canvasRect, int monitor);
    QList<QRectF> memberRects(const InfiniteDesktop::Group &group) const;
    void placeAgainLater(HWND hwnd, const QString &canvasKey, const QPointF &canvasPos);
    static void launch(const QString &command);

    // Notes
    bool expectNote(const QString &id, const QString &handle, QString *error);
    void storeNotePlacement(const ManagedWindow &window);
    QJsonObject noteState(const QString &id) const;
    void setNotePinned(const QString &id, bool pinned, bool keepAbove);

    // Overlays
    int overlayMonitor() const;
    void beginOverlay(Overlay overlay, int monitor);
    bool veilEvent(UINT message, WPARAM wParam, LPARAM lParam);
    bool panelEvent(UINT message, WPARAM wParam, LPARAM lParam);
    void finishSelection(const QRect &screenRect);
    void updateMapView();
    void renderMap();
    QImage renderLabel(const QString &text, qreal scale) const;
    bool isLocked(int monitor) const;

    CanvasService &m_service;
    InfiniteDesktop::GroupStore m_groups;
    InfiniteDesktop::AreaStore m_areas;
    InfiniteDesktop::NotePlacements m_notes;
    bool m_groupsDirty = false;
    bool m_areasDirty = false;
    bool m_notesDirty = false;

    NotesClient m_notesClient;
    QList<PendingLaunch> m_launches;
    QHash<HWND, ExpectedNote> m_expectedNotes;
    struct
    {
        QString monitorKey;
        QPointF screenPos;
        std::chrono::steady_clock::time_point deadline;
    } m_newNote;
    QString m_lastGroup;
    QSet<QString> m_areaUpdates;

    Overlay m_overlay = Overlay::None;
    QString m_overlayMonitor;
    std::unique_ptr<OverlayWindow> m_veil;
    std::unique_ptr<OverlayWindow> m_tint;
    std::unique_ptr<OverlayWindow> m_panel;
    std::unique_ptr<OverlayWindow> m_label;
    QTimer m_labelTimer;
    bool m_labelPersistent = false;
    // Selection
    bool m_selectGroup = true;
    bool m_selecting = false;
    QPoint m_selectStart;
    // Map
    InfiniteDesktop::MapView m_map;
    QRect m_mapPanel; ///< screen rect of the panel
    bool m_mapDragging = false;
    bool m_mapMoved = false;
    QPoint m_mapPressPos;
    QPointF m_mapStartCenter;
    QImage m_mapImage;
};

} // namespace Wid
