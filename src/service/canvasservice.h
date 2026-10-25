/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "camera.h"
#include "inputhook.h"
#include "inputrouter.h"
#include "markers.h"
#include "monitors.h"
#include "motion.h"
#include "organizer.h"
#include "overview.h"
#include "placementmemory.h"
#include "placementrules.h"
#include "settings.h"
#include "shortcuts.h"
#include "toplevel.h"
#include "touchpad.h"
#include "velocitytracker.h"
#include "virtualdesktops.h"
#include "win32util.h"
#include "windowevents.h"

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QTimer>

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

class QFileSystemWatcher;
class QWinEventNotifier;

namespace Wid
{

class Tray;

/**
 * The canvas: one per monitor and virtual desktop, keyed "<monitor identity>::<desktop GUID>"
 * (just the monitor identity without separate canvases per desktop).
 *
 * `camera` is what is shown; `committed` is the camera the physical window positions belong
 * to (both at 100 % unless the overview is open: then windows stay where they were and
 * only their thumbnails follow the camera).
 */
struct Canvas
{
    QString key;
    InfinitDesktop::Camera camera;
    InfinitDesktop::Camera committed;
    InfinitDesktop::CameraMotion motion;
    bool overview = false;
    std::optional<InfinitDesktop::Camera> beforeShowAll;
    std::optional<InfinitDesktop::Camera> beforeHome;
    std::array<std::optional<InfinitDesktop::Camera>, 4> bookmarks;
    HWND diveTarget = nullptr; ///< activated when the motion back to 100 % ends
    bool dirty = false; ///< the camera changed since the last flush
};

/// A window on the canvas. Looked up by HWND each time; never keep pointers to entries.
struct ManagedWindow
{
    HWND hwnd = nullptr;
    QString canvasKey;
    QString appKey; ///< placement memory key: AUMID or program path, lower case
    QString program; ///< file name without .exe, lower case (exclusion lists)
    QString windowClass; ///< lower case
    QString launch; ///< how to start the program again: AUMID or program path, as given
    // Notes, groups and areas (Organizer)
    QString noteId; ///< a note of the notes app
    QString groupId; ///< group it was last opened or arranged with
    int groupMember = -1;
    QString areaId; ///< area it was dropped into
    QPointF canvasPos; ///< visible frame, top left, canvas coordinates
    QSizeF size;
    bool pinned = false;
    bool pinnedAbove = false; ///< we made it topmost
    bool fullscreen = false;
    bool minimized = false;
    bool moving = false; ///< the user drags it (system move loop)
    bool hidden = false; ///< hidden, or on another virtual desktop
    bool unmovable = false; ///< elevated or refuses our moves
    bool maximized = false;
    bool parked = false;
    quint64 placementEntry = 0;
    // Our own recent moves, to tell them from foreign ones.
    MoveRequests requests;
    POINT lastRequest = {LONG_MIN, LONG_MIN};
    bool pendingMove = false; ///< lastRequest is held back: the program is behind with our moves
    int foreignWhileParked = 0;
    ULONGLONG foreignWindowStart = 0;

    bool onCanvas() const
    {
        return !pinned && !fullscreen && !minimized && !moving && !hidden && !unmovable;
    }
};

/// Monitor identity as the core compares it (placements, groups, areas, notes).
InfinitDesktop::OutputIdentity outputIdentity(const MonitorInfo &monitor);

class CanvasService : public QObject, public OverviewHost
{
    Q_OBJECT
    friend class Organizer;

public:
    explicit CanvasService(QObject *parent = nullptr);
    ~CanvasService() override;

    /// Starts everything; in safe mode only the tray and the IPC interface run.
    bool start(bool safeMode);
    /// Puts every window back on a screen, restores what was changed, stops the hooks.
    void shutdown();

    bool isPaused() const
    {
        return m_paused;
    }
    void setPaused(bool paused);
    bool isSafeMode() const
    {
        return m_safeMode;
    }
    void leaveSafeMode();
    void setTray(Tray *tray);
    void reloadSettings();
    /// Moves every window that is outside its monitor's work area into it.
    void bringWindowsBack();
    void openSettingsApp();

    QJsonValue handleIpc(const QString &method, const QJsonObject &params, QString *error);
    QJsonObject state() const;

    Organizer &organizer()
    {
        return *m_organizer;
    }
    /// Index of the monitor at (or nearest to) a screen point, -1 without monitors.
    int monitorIndexAt(const QPointF &point) const
    {
        return monitorAt(point);
    }

Q_SIGNALS:
    void statusChanged();
    void quitRequested();
    /// An event for every IPC client (groupsChanged, areasChanged, notePinnedChanged).
    void ipcEvent(const QString &event, const QJsonObject &params);

protected:
    // OverviewHost
    void overviewPan(int monitor, const QPointF &screenDelta) override;
    void overviewPanEnd(int monitor, const QPointF &velocity) override;
    void overviewZoom(int monitor, const QPointF &anchor, qreal steps) override;
    void overviewMoveWindow(HWND window, const QPointF &screenDelta) override;
    void overviewResizeWindow(HWND window, unsigned edges, const QPointF &screenDelta) override;
    void overviewDragEnd(HWND window) override;
    void overviewDive(HWND window) override;
    void overviewCommand(int monitor, Command command) override;
    void overviewContextMenu(int monitor, const QPointF &point) override;

private:
    struct MonitorState
    {
        MonitorInfo info;
        QString key;
        QRectF viewport; ///< bounds
        QRectF work;
        std::unique_ptr<Overview> overview;
        std::unique_ptr<Markers> markers;
    };
    struct UserMove
    {
        HWND hwnd = nullptr;
        bool isMove = false;
        int monitor = -1;
        QPointF edge;
    };

    // Startup and monitors
    bool createServiceWindow();
    static LRESULT CALLBACK serviceWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleServiceMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void updateMonitors();
    /// Index of the monitor of a monitor key or canvas key, -1 if it is gone.
    int monitorIndexOf(const QString &key) const;
    int monitorAt(const QPointF &point) const;
    int monitorOfWindow(HWND hwnd) const;
    Canvas &canvas(const QString &key);
    Canvas *canvasAt(const QPointF &point, int *monitor = nullptr);
    /// The canvas the monitor shows now (that of the current virtual desktop).
    Canvas *canvasOfMonitor(int monitor);
    Canvas &currentCanvas(const MonitorState &monitor);

    // Virtual desktops: the current one comes from Explorer's registry key (watched); a switch
    // keeps every canvas as it is and shows the other one.
    static QString canvasKeyFor(const QString &monitorKey, const QString &desktop);
    static QString monitorKeyOf(const QString &canvasKey);
    static QString desktopOf(const QString &canvasKey);
    /// The desktop part of current canvas keys: empty without canvases per desktop.
    QString currentDesktop() const;
    QString currentCanvasKey(const MonitorState &monitor) const;
    /// The canvas key for a window on this monitor: on the current desktop if it is visible.
    QString canvasKeyOn(const ManagedWindow &window, const MonitorState &monitor) const;
    void startDesktopWatch();
    /// The current desktop from the registry, else from a window shown on it; empty if unknown.
    QString readCurrentDesktop() const;
    void checkDesktop();
    void switchDesktop(const QString &desktop);
    /// Windows visible on this desktop that belong to another one (shown on all desktops,
    /// moved here): they keep their place on the screen, on the current canvas.
    void takeOverDesktopStrays();
    /// A window that shows up again: onto the canvas of the desktop Windows has it on.
    void followDesktop(ManagedWindow &window);
    /// Puts the window on another canvas where it is on the screen now.
    void moveToCanvas(ManagedWindow &window, const QString &key);
    void startHooks();
    void stopHooks();
    void publishSnapshot();

    // Windows
    void adoptExistingWindows();
    void tryAdopt(HWND hwnd, bool isNew);
    /// Handles windows that are gone without a destroy event as closed.
    void forgetDeadWindows();
    /// The common part of adopting: fills a record for a window on monitor `index`.
    ManagedWindow makeRecord(HWND hwnd, const TopLevel &top, int index);
    /// Adopts a note window announced by the notes app (tool windows are welcome here).
    void adoptNote(HWND hwnd, const QString &id);
    void forget(HWND hwnd);
    void onWindowEvent(DWORD event, HWND hwnd);
    void onLocationChanged(ManagedWindow &window);
    void onForeground(HWND hwnd);
    void onMoveSizeStart(HWND hwnd);
    void onMoveSizeEnd(HWND hwnd);
    void updateFullscreenLock();
    bool isExcluded(const ManagedWindow &window, const QStringList &list) const;
    QRectF frameOf(HWND hwnd) const;
    void restorePlacement(ManagedWindow &window, bool *reveal);
    /// Places a new window after `delayMs` and checks it again `attempts - 1` times.
    void settleNewWindow(HWND hwnd, int delayMs, int attempts);
    void rememberPlacement(const ManagedWindow &window);

    // Layout. Camera changes only mark the canvas; flush() applies them at most every 16 ms.
    void requestFlush();
    void flush();
    void applyAll();
    void applyCanvas(Canvas &canvas, HWND except = nullptr);
    /// On its screen if it shows there, else parked. A window that would reach into a neighbouring
    /// screen is parked too, unless it stays where it is or `asPlaced` (where the program put it).
    void placeWindow(ManagedWindow &window, const Canvas &canvas, const MonitorState &monitor, bool asPlaced = false);
    /// Moves asynchronously; while the program is busy with earlier moves, only remembers it.
    void requestMove(ManagedWindow &window, POINT frameTopLeft);
    /// The moves held back for programs that were busy, if they are free now.
    void sendPendingMoves();
    bool isOwnMove(ManagedWindow &window, POINT frameTopLeft, POINT windowTopLeft);
    QRectF canvasRect(const ManagedWindow &window) const;

    // Camera
    void setCamera(Canvas &canvas, const InfinitDesktop::Camera &camera);
    void animateTo(Canvas &canvas, const InfinitDesktop::Camera &target, std::chrono::milliseconds duration);
    void animateTo(Canvas &canvas, const InfinitDesktop::Camera &target);
    void zoomAt(Canvas &canvas, const QPointF &anchor, qreal steps);
    InfinitDesktop::Camera targetCamera(const Canvas &canvas) const;
    void enterOverview(Canvas &canvas);
    void leaveOverview(Canvas &canvas);
    void tick();
    void ensureTicking();
    void reveal(HWND hwnd, InfinitDesktop::RevealMode mode);
    void showAll(Canvas &canvas, bool toggle);
    void goHome(Canvas &canvas);
    void activate(HWND hwnd);
    void updateOverview(Canvas &canvas);
    void updateMarkers(bool navigating);
    void updateMarkersNow();
    void hideMarkersLater();
    QList<HWND> stackingOrder();

    // Input
    void drainInput();
    void handleCommand(const InputCommand &command);
    void triggerShortcut(ShortcutAction action, bool repeat);
    void togglePin(bool above);
    void setPinned(ManagedWindow &window, bool pinned, bool above);
    /// Gives the visible frame this size (asynchronously).
    void resizeFrame(ManagedWindow &window, const QSizeF &size);
    bool isLocked(int monitor) const;
    /// SetForegroundWindow that also works while another process has the input.
    void forceForeground(HWND hwnd);
    void edgeTick();

    // Persistence
    void loadState();
    void saveStateLater();
    void saveState();

    Settings m_settings;
    ShortcutMap m_shortcuts = ShortcutMap::defaults();
    bool m_started = false;
    bool m_safeMode = false;
    bool m_paused = false;
    bool m_shuttingDown = false;
    bool m_menuOpen = false; ///< the canvas menu runs its message loop (Organizer)
    Tray *m_tray = nullptr;

    UniqueWindow m_serviceWindow;
    std::vector<MonitorState> m_monitors;
    std::map<QString, Canvas> m_canvases; ///< stable addresses, never erased while running
    std::unordered_map<HWND, ManagedWindow> m_windows;
    QHash<HWND, ULONGLONG> m_created; ///< EVENT_OBJECT_CREATE tick of windows not shown yet
    QHash<HWND, QImage> m_icons;
    HWND m_foreground = nullptr;
    QList<HWND> m_stacking;
    bool m_stackingDirty = true;
    DWORD m_ownProcess = 0;
    bool m_selfElevated = false;

    WindowEvents m_events;
    std::unique_ptr<InputRouter> m_router;
    InputHook m_hook;
    TouchpadContacts m_touchpad;
    std::shared_ptr<const InputSnapshot> m_snapshot;

    // Hook thread -> main thread
    std::mutex m_inputMutex;
    std::vector<InputCommand> m_inputQueue;
    bool m_inputPosted = false;

    // Pan by drag or wheel
    QString m_panCanvas;
    InfinitDesktop::VelocityTracker m_panVelocity;
    QPointF m_panPosition;

    UserMove m_userMove;
    BOOL m_snapBefore = TRUE;
    bool m_snapChanged = false;
    std::chrono::steady_clock::time_point m_edgeLast;

    InfinitDesktop::PlacementMemory m_placements;

    bool m_markersDirty = false;
    bool m_markersNavigating = false;
    QTimer m_applyTimer; ///< coalesces moves, thumbnails and markers to about 60 Hz
    QTimer m_tickTimer; ///< runs only while a camera motion runs
    QTimer m_edgeTimer; ///< runs only while a dragged window touches an edge
    QTimer m_markerTimer; ///< hides the markers after navigating
    QTimer m_pendingTimer; ///< runs only while moves are held back for busy programs
    QTimer m_saveTimer;
    QTimer m_displayTimer; ///< settles monitor changes
    QFileSystemWatcher *m_settingsWatcher = nullptr;
    bool m_stateDirty = false;

    // Virtual desktops
    QString m_desktop; ///< GUID of the current virtual desktop, empty if unknown
    std::unique_ptr<VirtualDesktops> m_virtualDesktops; ///< which desktop a window is on
    DesktopRegistryWatch m_desktopWatch;
    QWinEventNotifier *m_desktopNotifier = nullptr;
    QTimer m_desktopTimer; ///< settles a desktop switch (registry change, cloak events)
    bool m_comInitialized = false;

    /// Last member: destroyed first, while everything it looks at still exists.
    std::unique_ptr<Organizer> m_organizer;
};

} // namespace Wid
