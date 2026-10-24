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
#include "overview.h"
#include "placementmemory.h"
#include "settings.h"
#include "shortcuts.h"
#include "touchpad.h"
#include "velocitytracker.h"
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

namespace Wid
{

class Tray;

/**
 * The canvas: one per monitor, keyed by the monitor's identity.
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
    struct Request
    {
        POINT position;
        ULONGLONG tick;
    };
    std::array<Request, 8> requests{};
    int requestCount = 0;
    int requestHead = 0;
    POINT lastRequest = {LONG_MIN, LONG_MIN};
    int foreignWhileParked = 0;
    ULONGLONG foreignWindowStart = 0;

    bool onCanvas() const
    {
        return !pinned && !fullscreen && !minimized && !moving && !hidden && !unmovable;
    }
};

class CanvasService : public QObject, public OverviewHost
{
    Q_OBJECT

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

Q_SIGNALS:
    void statusChanged();
    void quitRequested();

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
    int monitorIndexOf(const QString &key) const;
    int monitorAt(const QPointF &point) const;
    int monitorOfWindow(HWND hwnd) const;
    Canvas &canvas(const QString &key);
    Canvas *canvasAt(const QPointF &point, int *monitor = nullptr);
    Canvas *canvasOfMonitor(int monitor);
    void startHooks();
    void stopHooks();
    void publishSnapshot();

    // Windows
    void adoptExistingWindows();
    void tryAdopt(HWND hwnd, bool isNew);
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
    void rememberPlacement(const ManagedWindow &window);

    // Layout. Camera changes only mark the canvas; flush() applies them at most every 16 ms.
    void requestFlush();
    void flush();
    void applyAll();
    void applyCanvas(Canvas &canvas, HWND except = nullptr);
    void placeWindow(ManagedWindow &window, const Canvas &canvas, const MonitorState &monitor);
    void requestMove(ManagedWindow &window, POINT frameTopLeft);
    bool isOwnMove(ManagedWindow &window, POINT frameTopLeft);
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
    QTimer m_saveTimer;
    QTimer m_displayTimer; ///< settles monitor changes
    QFileSystemWatcher *m_settingsWatcher = nullptr;
    bool m_stateDirty = false;
};

} // namespace Wid
