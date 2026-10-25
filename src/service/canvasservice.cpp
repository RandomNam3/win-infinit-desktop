/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "canvasservice.h"
#include "fullscreen.h"
#include "layered.h"
#include "logging.h"
#include "markerlayout.h"
#include "navigation.h"
#include "parking.h"
#include "texts.h"
#include "toplevel.h"
#include "tray.h"
#include "windowmover.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineF>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>

#include <shellapi.h>

#include <algorithm>
#include <cmath>

using namespace std::chrono_literals;
using InfinitDesktop::Camera;

namespace Wid
{

namespace
{

constexpr UINT InputMessage = WM_APP + 10;
const wchar_t *const s_serviceClass = L"WinInfinitDesktopService";
constexpr auto ZoomStepDuration = 150ms;
constexpr ULONGLONG OwnMoveWindowMs = 1500;
constexpr ULONGLONG NewWindowMs = 5000;
constexpr qreal RevealMargin = 24;

QRectF toRectF(const RECT &rect)
{
    return QRectF(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}

POINT rounded(const QPointF &point)
{
    return POINT{LONG(std::lround(point.x())), LONG(std::lround(point.y()))};
}

InfinitDesktop::TimePoint now()
{
    return InfinitDesktop::Clock::now();
}

QJsonObject cameraJson(const Camera &camera)
{
    return QJsonObject{{QStringLiteral("x"), camera.center.x()}, {QStringLiteral("y"), camera.center.y()}, {QStringLiteral("zoom"), camera.zoom}};
}

std::optional<Camera> cameraFromJson(const QJsonValue &value)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    Camera camera{QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()),
                  object.value(QStringLiteral("zoom")).toDouble(1.0)};
    const bool finite = std::isfinite(camera.center.x()) && std::isfinite(camera.center.y()) && std::abs(camera.center.x()) < 1e8
        && std::abs(camera.center.y()) < 1e8 && std::isfinite(camera.zoom) && camera.zoom > 0.01 && camera.zoom < 10;
    return finite ? std::optional<Camera>(camera) : std::nullopt;
}

QJsonObject rectJson(const QRectF &rect)
{
    return QJsonObject{{QStringLiteral("x"), rect.x()}, {QStringLiteral("y"), rect.y()}, {QStringLiteral("width"), rect.width()}, {QStringLiteral("height"), rect.height()}};
}

QString hwndText(HWND hwnd)
{
    return QStringLiteral("0x%1").arg(quintptr(hwnd), 0, 16);
}

// Snap is switched off while a canvas window is dragged. A marker file remembers that, so
// that the next start switches it back on if the service crashed during a drag.
QString snapMarkerPath()
{
    return QDir(stateDirectory()).filePath(QStringLiteral("snap-off"));
}

void setSnap(bool on)
{
    SystemParametersInfoW(SPI_SETWINARRANGING, on ? TRUE : FALSE, nullptr, 0);
    if (on) {
        QFile::remove(snapMarkerPath());
    } else {
        QFile marker(snapMarkerPath());
        if (marker.open(QIODevice::WriteOnly)) {
            marker.write("1");
        }
    }
}

} // namespace

InfinitDesktop::OutputIdentity outputIdentity(const MonitorInfo &monitor)
{
    InfinitDesktop::OutputIdentity identity;
    identity.name = QString::fromStdWString(monitor.gdiName);
    identity.manufacturer = QString::fromStdWString(monitor.manufacturerCode());
    if (monitor.edidValid) {
        identity.model = QStringLiteral("%1").arg(monitor.edidProduct, 4, 16, QLatin1Char('0')).toUpper();
    }
    return identity;
}

CanvasService::CanvasService(QObject *parent)
    : QObject(parent)
{
    m_applyTimer.setSingleShot(true);
    m_applyTimer.setInterval(16);
    connect(&m_applyTimer, &QTimer::timeout, this, &CanvasService::flush);
    m_tickTimer.setInterval(16);
    m_tickTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_tickTimer, &QTimer::timeout, this, &CanvasService::tick);
    m_edgeTimer.setInterval(16);
    m_edgeTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_edgeTimer, &QTimer::timeout, this, &CanvasService::edgeTick);
    m_markerTimer.setSingleShot(true);
    m_markerTimer.setInterval(2000);
    connect(&m_markerTimer, &QTimer::timeout, this, [this]() {
        if (m_settings.offscreenMarkers != ShowMode::Always) {
            for (MonitorState &monitor : m_monitors) {
                if (monitor.markers) {
                    monitor.markers->hide();
                }
            }
        }
    });
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(3000);
    connect(&m_saveTimer, &QTimer::timeout, this, &CanvasService::saveState);
    m_displayTimer.setSingleShot(true);
    m_displayTimer.setInterval(400);
    connect(&m_displayTimer, &QTimer::timeout, this, [this]() {
        updateMonitors();
        applyAll();
        updateMarkers(false);
    });
    m_organizer = std::make_unique<Organizer>(*this);
    connect(m_organizer.get(), &Organizer::ipcEvent, this, &CanvasService::ipcEvent);
}

CanvasService::~CanvasService()
{
    shutdown();
    detachWindowObject(m_serviceWindow.get());
}

// --- Startup ---------------------------------------------------------------------------------

bool CanvasService::start(bool safeMode)
{
    m_safeMode = safeMode;
    m_ownProcess = GetCurrentProcessId();
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation = {};
        DWORD size = 0;
        m_selfElevated = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated;
        CloseHandle(token);
    }
    reloadSettings();
    if (QFile::exists(snapMarkerPath())) {
        qCInfo(lcService) << "switching Snap back on after an unclean exit";
        setSnap(true);
    }
    if (!createServiceWindow()) {
        qCWarning(lcService) << "cannot create the service window";
        return false;
    }
    updateMonitors();
    loadState();
    m_organizer->load();

    m_settingsWatcher = new QFileSystemWatcher(this);
    const QString settingsPath = settingsFilePath();
    QDir().mkpath(QFileInfo(settingsPath).absolutePath());
    m_settingsWatcher->addPath(QFileInfo(settingsPath).absolutePath());
    if (QFile::exists(settingsPath)) {
        m_settingsWatcher->addPath(settingsPath);
    }
    const auto settingsChanged = [this, settingsPath]() {
        // Editors replace the file: watch it again.
        if (QFile::exists(settingsPath) && !m_settingsWatcher->files().contains(settingsPath)) {
            m_settingsWatcher->addPath(settingsPath);
        }
        reloadSettings();
    };
    connect(m_settingsWatcher, &QFileSystemWatcher::fileChanged, this, settingsChanged);
    connect(m_settingsWatcher, &QFileSystemWatcher::directoryChanged, this, settingsChanged);

    m_started = true;
    if (!m_safeMode) {
        m_events.start([this](DWORD event, HWND hwnd) {
            onWindowEvent(event, hwnd);
        });
        adoptExistingWindows();
        startHooks();
        m_touchpad.start(m_serviceWindow.get());
        applyAll();
        updateMarkers(false);
        m_organizer->start();
    }
    publishSnapshot();
    qCInfo(lcService) << "started, safe mode" << m_safeMode << "windows" << m_windows.size() << "monitors" << m_monitors.size();
    return true;
}

bool CanvasService::createServiceWindow()
{
    registerWindowClass(s_serviceClass, &CanvasService::serviceWindowProc);
    // A hidden top-level window: message-only windows get neither WM_DISPLAYCHANGE nor
    // WM_ENDSESSION.
    m_serviceWindow.reset(CreateWindowExW(WS_EX_TOOLWINDOW, s_serviceClass, L"WIN Infinit Desktop", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                                          GetModuleHandleW(nullptr), this));
    return bool(m_serviceWindow);
}

LRESULT CALLBACK CanvasService::serviceWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    CanvasService *self = windowObject<CanvasService>(hwnd, message, lParam);
    if (!self || !self->m_serviceWindow) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return self->handleServiceMessage(message, wParam, lParam);
}

LRESULT CanvasService::handleServiceMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case InputMessage:
        drainInput();
        return 0;
    case WM_INPUT:
        if (m_touchpad.handleInput(lParam) && m_touchpad.lastContacts() >= 2 && m_router) {
            m_router->setTouchpadTick(GetTickCount());
        }
        // DefWindowProc frees the raw input data.
        break;
    case WM_DISPLAYCHANGE:
        m_displayTimer.start();
        return 0;
    case WM_SETTINGCHANGE:
        if (wParam == SPI_SETWORKAREA) {
            m_displayTimer.start();
        }
        break;
    case WM_QUERYENDSESSION:
        return TRUE;
    case WM_ENDSESSION:
        if (wParam) {
            shutdown();
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(m_serviceWindow.get(), message, wParam, lParam);
}

void CanvasService::reloadSettings()
{
    QSettings file(settingsFilePath(), QSettings::IniFormat);
    m_settings = Settings::load(file);
    m_shortcuts = ShortcutMap::load(file);
    setDebugLogging(m_settings.debugLogging);
    publishSnapshot();
    if (m_started && !m_safeMode) {
        updateMarkers(false);
    }
    qCDebug(lcService) << "settings loaded";
}

void CanvasService::updateMonitors()
{
    std::vector<MonitorInfo> infos = enumerateMonitors();
    std::vector<MonitorState> next;
    next.reserve(infos.size());
    QHash<QString, int> seen;
    for (MonitorInfo &info : infos) {
        MonitorState state;
        QString key = QString::fromStdWString(info.identity());
        // Two identical monitors share an EDID identity; the device path tells them apart.
        if (seen.contains(key)) {
            key += QLatin1Char('|') + QString::fromStdWString(info.devicePath.empty() ? info.gdiName : info.devicePath);
        }
        seen.insert(key, 1);
        state.key = key;
        state.viewport = toRectF(info.bounds);
        state.work = toRectF(info.work);
        state.info = std::move(info);
        // Keep the overview and markers of monitors that stay.
        for (MonitorState &old : m_monitors) {
            if (old.key == state.key) {
                state.overview = std::move(old.overview);
                state.markers = std::move(old.markers);
                break;
            }
        }
        next.push_back(std::move(state));
    }
    // Overviews of monitors that went away: their canvases leave the overview.
    for (MonitorState &old : m_monitors) {
        if (old.overview) {
            old.overview->hide();
            if (auto it = m_canvases.find(old.key); it != m_canvases.end() && it->second.overview) {
                it->second.overview = false;
                it->second.camera.zoom = 1;
                it->second.committed = it->second.camera;
            }
        }
    }
    m_monitors = std::move(next);
    for (int i = 0; i < int(m_monitors.size()); ++i) {
        if (m_monitors[std::size_t(i)].overview) {
            m_monitors[std::size_t(i)].overview->setMonitor(i);
        }
        canvas(m_monitors[std::size_t(i)].key);
    }
    // Windows whose monitor is gone belong to the monitor they are on now.
    for (auto &[hwnd, window] : m_windows) {
        if (monitorIndexOf(window.canvasKey) >= 0 || !IsWindow(hwnd)) {
            continue;
        }
        const int index = monitorOfWindow(hwnd);
        if (index < 0) {
            continue;
        }
        const MonitorState &monitor = m_monitors[std::size_t(index)];
        window.canvasKey = monitor.key;
        const QRectF frame = frameOf(hwnd);
        window.canvasPos = canvas(monitor.key).committed.screenToCanvas(frame.topLeft(), monitor.viewport);
        window.lastRequest = rounded(frame.topLeft());
    }
    if (m_organizer) {
        m_organizer->monitorsChanged();
    }
    publishSnapshot();
}

int CanvasService::monitorIndexOf(const QString &key) const
{
    for (int i = 0; i < int(m_monitors.size()); ++i) {
        if (m_monitors[std::size_t(i)].key == key) {
            return i;
        }
    }
    return -1;
}

int CanvasService::monitorAt(const QPointF &point) const
{
    QList<QRectF> rects;
    for (const MonitorState &monitor : m_monitors) {
        if (monitor.viewport.contains(point)) {
            return int(&monitor - m_monitors.data());
        }
        rects.append(monitor.viewport);
    }
    return InfinitDesktop::nearestOutputIndex(rects, point);
}

int CanvasService::monitorOfWindow(HWND hwnd) const
{
    HMONITOR handle = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    for (int i = 0; i < int(m_monitors.size()); ++i) {
        if (m_monitors[std::size_t(i)].info.handle == handle) {
            return i;
        }
    }
    return m_monitors.empty() ? -1 : 0;
}

Canvas &CanvasService::canvas(const QString &key)
{
    auto it = m_canvases.find(key);
    if (it == m_canvases.end()) {
        Canvas created;
        created.key = key;
        it = m_canvases.emplace(key, std::move(created)).first;
    }
    return it->second;
}

Canvas *CanvasService::canvasAt(const QPointF &point, int *monitor)
{
    const int index = monitorAt(point);
    if (monitor) {
        *monitor = index;
    }
    return canvasOfMonitor(index);
}

Canvas *CanvasService::canvasOfMonitor(int monitor)
{
    if (monitor < 0 || monitor >= int(m_monitors.size())) {
        return nullptr;
    }
    return &canvas(m_monitors[std::size_t(monitor)].key);
}

void CanvasService::startHooks()
{
    if (m_hook.isRunning()) {
        return;
    }
    const HWND target = m_serviceWindow.get();
    auto sink = [this, target](const InputCommand &command) {
        // Hook thread: queue, coalesce consecutive deltas, wake the main thread once.
        std::lock_guard lock(m_inputMutex);
        if (!m_inputQueue.empty()) {
            InputCommand &last = m_inputQueue.back();
            const bool deltas = (command.kind == InputCommand::Kind::PanDelta || command.kind == InputCommand::Kind::ScrollPan) && last.kind == command.kind;
            if (deltas) {
                last.delta += command.delta;
                last.point = command.point;
                last.time = command.time;
                return;
            }
        }
        if (m_inputQueue.size() < 4096) {
            m_inputQueue.push_back(command);
        }
        if (!m_inputPosted) {
            m_inputPosted = true;
            PostMessageW(target, InputMessage, 0, 0);
        }
    };
    auto releaseLeft = []() {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        input.mi.dwExtraInfo = InputHook::injectedTag;
        SendInput(1, &input, sizeof(INPUT));
    };
    m_router = std::make_unique<InputRouter>(sink, &isDesktopAt, releaseLeft);
    publishSnapshot();
    InputRouter *router = m_router.get();
    const bool ok = m_hook.start(
        [this, router](const MouseEvent &event) {
            return router->mouse(event, m_hook.modifiers());
        },
        [this, router](const KeyEvent &event) {
            return router->key(event, m_hook.modifiers());
        },
        5 * 60 * 1000);
    if (!ok) {
        qCWarning(lcService) << "cannot install the low-level hooks";
    }
}

void CanvasService::stopHooks()
{
    // The hook thread uses the router: stop it first.
    m_hook.stop();
    m_router.reset();
    std::lock_guard lock(m_inputMutex);
    m_inputQueue.clear();
    m_inputPosted = false;
}

void CanvasService::publishSnapshot()
{
    auto snapshot = std::make_shared<InputSnapshot>();
    snapshot->enabled = m_started && !m_safeMode && !m_paused;
    snapshot->shortcutsEnabled = m_started && !m_safeMode;
    snapshot->dragDesktopToPan = m_settings.dragDesktopToPan;
    snapshot->middleButtonPan = m_settings.middleButtonPan;
    snapshot->panModifiers = modifierBits(m_settings.panModifier);
    snapshot->wheelZoomOnDesktop = m_settings.wheelZoomOnDesktop;
    snapshot->zoomModifiers = modifierBits(m_settings.zoomModifier);
    snapshot->shiftWheelPans = m_settings.shiftWheelPans;
    snapshot->touchpadScrollPans = m_settings.touchpadScrollPans;
    snapshot->pinchZoom = m_settings.pinchZoom;
    snapshot->edgePan = m_settings.edgePan;
    snapshot->edgePanMargin = m_settings.edgePanMargin;
    snapshot->flingToOtherScreen = m_settings.flingToOtherScreen;
    snapshot->flingThreshold = m_settings.flingThreshold;
    snapshot->shortcuts = m_shortcuts;
    for (const auto &[key, canvas] : m_canvases) {
        snapshot->gliding |= canvas.motion.isActive();
        snapshot->overviewOpen |= canvas.overview;
    }
    snapshot->overlayOpen = m_organizer && m_organizer->overlayOpen();
    snapshot->canvasMenu = m_started && !m_safeMode;
    const HWND foreground = m_foreground && IsWindow(m_foreground) && !IsIconic(m_foreground) ? m_foreground : nullptr;
    QRectF foregroundFrame;
    bool lockingForeground = false;
    if (foreground && m_settings.lockFullscreen) {
        const TopLevel top = describe(foreground);
        const WindowClass windowClass = classify(top);
        lockingForeground = windowClass != WindowClass::Desktop && windowClass != WindowClass::Taskbar && windowClass != WindowClass::Shell
            && top.processId != m_ownProcess;
        foregroundFrame = toRectF(top.frame);
    }
    for (const MonitorState &monitor : m_monitors) {
        snapshot->monitors.append(monitor.info.bounds);
        bool locked = false;
        if (lockingForeground && !IsZoomed(foreground)) {
            const QRectF &bounds = monitor.viewport;
            locked = std::abs(foregroundFrame.left() - bounds.left()) <= 1 && std::abs(foregroundFrame.top() - bounds.top()) <= 1
                && std::abs(foregroundFrame.right() - bounds.right()) <= 1 && std::abs(foregroundFrame.bottom() - bounds.bottom()) <= 1;
        }
        snapshot->locked.append(locked);
    }
    snapshot->userMove = m_userMove.hwnd && m_userMove.isMove;
    snapshot->userMoveMonitor = m_userMove.monitor;
    m_snapshot = snapshot;
    if (m_router) {
        m_router->setSnapshot(snapshot);
    }
}

// --- Windows -----------------------------------------------------------------------------------

QRectF CanvasService::frameOf(HWND hwnd) const
{
    RECT frame = {};
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame)))) {
        if (!GetWindowRect(hwnd, &frame)) {
            return {};
        }
    }
    return toRectF(frame);
}

QRectF CanvasService::canvasRect(const ManagedWindow &window) const
{
    return QRectF(window.canvasPos, window.size);
}

bool CanvasService::isExcluded(const ManagedWindow &window, const QStringList &list) const
{
    return !list.isEmpty() && (list.contains(window.program) || list.contains(window.windowClass) || list.contains(window.appKey));
}

void CanvasService::adoptExistingWindows()
{
    // A crashed run leaves windows parked outside every monitor: bring them back first.
    const std::vector<HWND> windows = enumerateTopLevel();
    for (HWND hwnd : windows) {
        const TopLevel top = describe(hwnd);
        if (top.processId == m_ownProcess || classify(top) != WindowClass::Canvas || top.minimized) {
            continue;
        }
        const QRectF frame = toRectF(top.frame);
        const bool onSomeMonitor = std::any_of(m_monitors.cbegin(), m_monitors.cend(), [&frame](const MonitorState &monitor) {
            return monitor.viewport.intersects(frame);
        });
        if (onSomeMonitor || m_monitors.empty()) {
            continue;
        }
        const int index = monitorAt(frame.center());
        if (index < 0) {
            continue;
        }
        const QRectF &work = m_monitors[std::size_t(index)].work;
        const QPointF topLeft(std::clamp(work.center().x() - frame.width() / 2, work.left(), std::max(work.left(), work.right() - frame.width())),
                              std::clamp(work.center().y() - frame.height() / 2, work.top(), std::max(work.top(), work.bottom() - frame.height())));
        moveFrameTo(hwnd, rounded(topLeft), true);
        qCInfo(lcService) << "rescued a window outside every monitor" << hwndText(hwnd);
    }
    for (HWND hwnd : windows) {
        tryAdopt(hwnd, false);
    }
    m_stackingDirty = true;
}

ManagedWindow CanvasService::makeRecord(HWND hwnd, const TopLevel &top, int index)
{
    ManagedWindow window;
    window.hwnd = hwnd;
    QString program = QFileInfo(QString::fromStdWString(top.exePath)).fileName().toLower();
    if (program.endsWith(QLatin1String(".exe"))) {
        program.chop(4);
    }
    window.program = program;
    window.windowClass = QString::fromStdWString(top.className).toLower();
    window.launch = top.appUserModelId.empty() ? QString::fromStdWString(top.exePath) : QString::fromStdWString(top.appUserModelId);
    window.appKey = window.launch.toLower();
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    const Canvas &target = canvas(monitor.key);
    const QRectF frame = toRectF(top.frame);
    window.canvasKey = monitor.key;
    window.canvasPos = target.committed.screenToCanvas(frame.topLeft(), monitor.viewport);
    window.size = frame.size();
    window.lastRequest = rounded(frame.topLeft());
    window.minimized = top.minimized;
    window.maximized = top.maximized;
    window.fullscreen = coversMonitor(top, monitor.info.bounds);
    window.unmovable = top.elevatedKnown && top.elevated && !m_selfElevated;
    return window;
}

void CanvasService::forgetDeadWindows()
{
    // A window whose process was killed may vanish without EVENT_OBJECT_DESTROY reaching us
    // (or before it does); its place must still be remembered for the next window of it.
    std::vector<HWND> dead;
    for (const auto &[hwnd, window] : m_windows) {
        if (!IsWindow(hwnd)) {
            dead.push_back(hwnd);
        }
    }
    for (HWND hwnd : dead) {
        onWindowEvent(EVENT_OBJECT_DESTROY, hwnd);
    }
}

void CanvasService::tryAdopt(HWND hwnd, bool isNew)
{
    if (isNew) {
        forgetDeadWindows();
    }
    if (m_windows.count(hwnd) || !IsWindow(hwnd) || m_monitors.empty() || !IsWindowVisible(hwnd)) {
        return;
    }
    // Cheap checks first: menus, tooltips and popups show up all the time and are tool or
    // owned windows; describe() opens the process.
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const bool appWindow = (exStyle & WS_EX_APPWINDOW) != 0;
    if ((style & WS_CHILD) || (!appWindow && ((exStyle & WS_EX_TOOLWINDOW) || GetWindow(hwnd, GW_OWNER)))) {
        return;
    }
    const TopLevel top = describe(hwnd);
    if (top.processId == m_ownProcess) {
        return;
    }
    const WindowClass windowClass = classify(top);
    if (windowClass != WindowClass::Canvas && windowClass != WindowClass::OtherDesktop) {
        return;
    }
    const int index = monitorOfWindow(hwnd);
    if (index < 0) {
        return;
    }
    ManagedWindow window = makeRecord(hwnd, top, index);
    if (isExcluded(window, m_settings.excludedClasses)) {
        return;
    }
    window.hidden = windowClass == WindowClass::OtherDesktop;
    if (m_settings.pinKeepAboveWindows && (top.exStyle & WS_EX_TOPMOST)) {
        window.pinned = true;
    }
    bool revealIt = false;
    // A window started for a group goes to its place there; others where they were last.
    if (isNew && !window.hidden && !m_organizer->placeLaunched(window)) {
        restorePlacement(window, &revealIt);
    }
    const QString key = window.canvasKey;
    m_windows.emplace(hwnd, std::move(window));
    m_stackingDirty = true;
    qCDebug(lcService) << "adopted" << hwndText(hwnd) << m_windows.at(hwnd).program << "new" << isNew;

    ManagedWindow &adopted = m_windows.at(hwnd);
    m_organizer->windowAdopted(adopted);
    Canvas &home = canvas(key);
    if (isNew && home.overview && m_settings.newWindowsExitZoom && !adopted.hidden) {
        reveal(hwnd, InfinitDesktop::RevealMode::Center);
        return;
    }
    if (!home.overview && adopted.onCanvas()) {
        if (isNew) {
            // A window that just appeared is still settling (its DWM frame can lag behind the
            // window rect): place it a moment later and check the result once more.
            settleNewWindow(hwnd, 100, 2);
        } else {
            const int homeIndex = monitorIndexOf(key);
            if (homeIndex >= 0) {
                placeWindow(adopted, home, m_monitors[std::size_t(homeIndex)]);
            }
        }
    }
    if (revealIt) {
        reveal(hwnd, m_settings.revealMode);
    } else if (home.overview) {
        updateOverview(home);
    }
}

void CanvasService::adoptNote(HWND hwnd, const QString &id)
{
    if (m_safeMode || m_shuttingDown || !IsWindow(hwnd) || m_monitors.empty()) {
        return;
    }
    if (auto it = m_windows.find(hwnd); it != m_windows.end()) {
        // Adopted before it was announced (it is not a tool window): a note from now on.
        it->second.noteId = id;
        if (it->second.placementEntry) {
            m_placements.release(it->second.placementEntry);
            it->second.placementEntry = 0;
        }
        return;
    }
    if (!IsWindowVisible(hwnd) || (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) || GetAncestor(hwnd, GA_ROOT) != hwnd) {
        return;
    }
    const TopLevel top = describe(hwnd);
    const int index = monitorOfWindow(hwnd);
    if (top.processId == m_ownProcess || index < 0) {
        return;
    }
    ManagedWindow window = makeRecord(hwnd, top, index);
    window.noteId = id;
    window.fullscreen = false;
    const bool isNewNote = m_organizer->placeNote(window);
    const QString key = window.canvasKey;
    m_windows.emplace(hwnd, std::move(window));
    m_stackingDirty = true;
    ManagedWindow &adopted = m_windows.at(hwnd);
    m_organizer->windowAdopted(adopted);
    Canvas &home = canvas(key);
    if (adopted.onCanvas()) {
        if (home.overview) {
            updateOverview(home);
        } else {
            settleNewWindow(hwnd, 0, 2);
        }
    }
    if (isNewNote) {
        activate(hwnd);
    }
    qCDebug(lcService) << "adopted note" << id << hwndText(hwnd);
}

void CanvasService::settleNewWindow(HWND hwnd, int delayMs, int attempts)
{
    QTimer::singleShot(delayMs, this, [this, hwnd, attempts]() {
        // Looked up again: the window may be gone or changed by now.
        auto it = m_windows.find(hwnd);
        if (it == m_windows.end() || !IsWindow(hwnd) || !it->second.onCanvas() || m_shuttingDown) {
            return;
        }
        ManagedWindow &window = it->second;
        Canvas &home = canvas(window.canvasKey);
        const int index = monitorIndexOf(window.canvasKey);
        if (home.overview || index < 0) {
            return;
        }
        const MonitorState &monitor = m_monitors[std::size_t(index)];
        const QPointF target = home.committed.canvasToScreen(window.canvasPos, monitor.viewport);
        const QRectF frame = frameOf(hwnd);
        qCDebug(lcService) << "settle" << hwndText(hwnd) << "frame" << frame.topLeft() << "target" << target << "attempts" << attempts;
        if (window.parked || (QLineF(frame.topLeft(), target).length() > 2 && frame.intersects(monitor.viewport))) {
            window.lastRequest = POINT{LONG_MIN, LONG_MIN};
        }
        placeWindow(window, home, monitor);
        if (attempts > 1) {
            settleNewWindow(hwnd, 300, attempts - 1);
        }
    });
}

void CanvasService::restorePlacement(ManagedWindow &window, bool *reveal)
{
    if (!m_settings.rememberWindowPlacement || window.appKey.isEmpty() || window.fullscreen || window.maximized
        || isExcluded(window, m_settings.placementExcludedClasses)) {
        return;
    }
    quint64 entry = 0;
    const std::optional<InfinitDesktop::WindowPlacement> placement = m_placements.take(window.appKey, &entry);
    if (!placement) {
        return;
    }
    window.placementEntry = entry;
    QList<InfinitDesktop::OutputIdentity> identities;
    for (const MonitorState &monitor : m_monitors) {
        identities.append(outputIdentity(monitor.info));
    }
    int index = InfinitDesktop::bestOutputMatch(placement->output, identities);
    if (index < 0) {
        index = monitorIndexOf(window.canvasKey);
    }
    if (index < 0) {
        return;
    }
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    Canvas &target = canvas(monitor.key);
    window.canvasKey = monitor.key;
    const QPointF screenPos = monitor.viewport.topLeft() + placement->screenOffset;
    window.canvasPos = m_settings.restoreOnCanvas ? placement->canvasPos : target.committed.screenToCanvas(screenPos, monitor.viewport);
    if (m_settings.restoreWindowSize && placement->size.width() >= 50 && placement->size.height() >= 30) {
        resizeFrame(window, placement->size);
    }
    const bool otherOpen = std::any_of(m_windows.cbegin(), m_windows.cend(), [&window](const auto &entry) {
        return entry.second.appKey == window.appKey;
    });
    if (placement->pinned && !otherOpen) {
        window.pinned = true;
        moveFrameTo(window.hwnd, rounded(screenPos));
        if (placement->keepAbove) {
            SetWindowPos(window.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            window.pinnedAbove = true;
        }
        return;
    }
    *reveal = !InfinitDesktop::isFullyVisible(target.committed, canvasRect(window), monitor.viewport, monitor.work);
}

void CanvasService::resizeFrame(ManagedWindow &window, const QSizeF &size)
{
    RECT windowRect = {};
    const QRectF frame = frameOf(window.hwnd);
    if (!GetWindowRect(window.hwnd, &windowRect) || frame.isEmpty()) {
        return;
    }
    // SetWindowPos takes the window rect, which includes the invisible resize borders.
    const int width = int(std::lround(size.width() + (windowRect.right - windowRect.left) - frame.width()));
    const int height = int(std::lround(size.height() + (windowRect.bottom - windowRect.top) - frame.height()));
    SetWindowPos(window.hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
    window.size = size;
}

void CanvasService::rememberPlacement(const ManagedWindow &window)
{
    if (!m_settings.rememberWindowPlacement || window.appKey.isEmpty() || window.fullscreen || window.hidden || !window.noteId.isEmpty()
        || isExcluded(window, m_settings.placementExcludedClasses)) {
        if (window.placementEntry) {
            m_placements.release(window.placementEntry);
        }
        return;
    }
    const int index = monitorIndexOf(window.canvasKey);
    if (index < 0) {
        return;
    }
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    const Canvas &home = m_canvases.at(window.canvasKey);
    InfinitDesktop::WindowPlacement placement;
    placement.appKey = window.appKey;
    placement.output = outputIdentity(monitor.info);
    placement.canvasPos = window.canvasPos;
    placement.screenOffset = home.committed.canvasToScreen(window.canvasPos, monitor.viewport) - monitor.viewport.topLeft();
    placement.size = window.size;
    placement.maximized = window.maximized;
    placement.pinned = window.pinned;
    placement.keepAbove = window.pinnedAbove;
    placement.timestamp = QDateTime::currentMSecsSinceEpoch();
    m_placements.remember(placement, window.placementEntry);
    saveStateLater();
}

void CanvasService::forget(HWND hwnd)
{
    m_windows.erase(hwnd);
    m_icons.remove(hwnd);
    for (MonitorState &monitor : m_monitors) {
        if (monitor.overview) {
            monitor.overview->removeWindow(hwnd);
        }
    }
    for (auto &[key, canvas] : m_canvases) {
        if (canvas.diveTarget == hwnd) {
            canvas.diveTarget = nullptr;
        }
    }
    if (m_userMove.hwnd == hwnd) {
        m_userMove = UserMove{};
        m_edgeTimer.stop();
        publishSnapshot();
    }
    if (m_foreground == hwnd) {
        m_foreground = nullptr;
    }
    m_stackingDirty = true;
}

void CanvasService::onWindowEvent(DWORD event, HWND hwnd)
{
    if (m_shuttingDown || m_safeMode) {
        return;
    }
    auto it = m_windows.find(hwnd);
    switch (event) {
    case EVENT_OBJECT_CREATE:
        if (m_created.size() > 2000) {
            const ULONGLONG limit = GetTickCount64() - NewWindowMs;
            m_created.removeIf([limit](const QHash<HWND, ULONGLONG>::iterator entry) {
                return entry.value() < limit;
            });
        }
        m_created.insert(hwnd, GetTickCount64());
        return;
    case EVENT_OBJECT_SHOW:
        m_stackingDirty = true;
        if (it == m_windows.end()) {
            if (const std::optional<QString> note = m_organizer->takeExpectedNote(hwnd)) {
                m_created.remove(hwnd);
                adoptNote(hwnd, *note);
                return;
            }
            const ULONGLONG created = m_created.take(hwnd);
            tryAdopt(hwnd, created && GetTickCount64() - created < NewWindowMs);
        } else if (it->second.hidden && IsWindowVisible(hwnd)) {
            DWORD cloaked = 0;
            DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
            if (!cloaked) {
                it->second.hidden = false;
                it->second.lastRequest = POINT{LONG_MIN, LONG_MIN};
                canvas(it->second.canvasKey).dirty = true;
                requestFlush();
            }
        }
        return;
    case EVENT_OBJECT_HIDE:
        m_stackingDirty = true;
        if (it != m_windows.end() && !IsWindowVisible(hwnd)) {
            m_organizer->windowGone(it->second, false);
            it->second.hidden = true;
        }
        return;
    case EVENT_OBJECT_DESTROY:
        m_created.remove(hwnd);
        m_organizer->handleDestroyed(hwnd);
        if (it != m_windows.end()) {
            qCDebug(lcService) << "closed" << hwndText(hwnd) << it->second.program;
            rememberPlacement(it->second);
            m_organizer->windowGone(it->second, true);
            forget(hwnd);
        }
        return;
    case EVENT_SYSTEM_FOREGROUND:
        m_stackingDirty = true;
        onForeground(hwnd);
        return;
    case EVENT_SYSTEM_MOVESIZESTART:
        onMoveSizeStart(hwnd);
        return;
    case EVENT_SYSTEM_MOVESIZEEND:
        onMoveSizeEnd(hwnd);
        return;
    case EVENT_SYSTEM_MINIMIZESTART:
        if (it != m_windows.end()) {
            it->second.minimized = true;
        }
        return;
    case EVENT_SYSTEM_MINIMIZEEND:
        if (it != m_windows.end()) {
            it->second.minimized = false;
            // Windows puts it back where it was minimized; take that as the user's choice.
            it->second.lastRequest = POINT{LONG_MIN, LONG_MIN};
        }
        return;
    case EVENT_OBJECT_LOCATIONCHANGE:
        if (it != m_windows.end()) {
            onLocationChanged(it->second);
        } else if (hwnd == m_foreground) {
            publishSnapshot();
        }
        return;
    case EVENT_OBJECT_CLOAKED:
    case EVENT_OBJECT_UNCLOAKED: {
        m_stackingDirty = true;
        if (it == m_windows.end()) {
            if (event == EVENT_OBJECT_UNCLOAKED) {
                tryAdopt(hwnd, false);
            }
            return;
        }
        DWORD cloaked = 0;
        DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
        const bool hidden = cloaked != 0 || !IsWindowVisible(hwnd);
        if (it->second.hidden && !hidden) {
            // Back from another virtual desktop: to its place on the canvas.
            it->second.lastRequest = POINT{LONG_MIN, LONG_MIN};
            it->second.hidden = false;
            canvas(it->second.canvasKey).dirty = true;
            requestFlush();
        }
        it->second.hidden = hidden;
        return;
    }
    default:
        return;
    }
}

bool CanvasService::isOwnMove(ManagedWindow &window, POINT position, POINT windowPosition)
{
    const ULONGLONG nowTick = GetTickCount64();
    const auto same = [](POINT a, POINT b) {
        return std::abs(a.x - b.x) <= 1 && std::abs(a.y - b.y) <= 1;
    };
    for (int i = 0; i < window.requestCount; ++i) {
        const ManagedWindow::Request &request = window.requests[std::size_t(i)];
        // The window rect is what SetWindowPos got; the frame may differ while DWM catches up.
        if (nowTick - request.tick <= OwnMoveWindowMs && (same(request.position, position) || same(request.windowPosition, windowPosition))) {
            return true;
        }
    }
    return false;
}

void CanvasService::onLocationChanged(ManagedWindow &window)
{
    const HWND hwnd = window.hwnd;
    if (window.moving || !IsWindow(hwnd)) {
        return;
    }
    if (IsIconic(hwnd)) {
        window.minimized = true;
        return;
    }
    const QRectF frame = frameOf(hwnd);
    if (frame.isEmpty()) {
        return;
    }
    const bool resized = std::abs(frame.width() - window.size.width()) > 0.5 || std::abs(frame.height() - window.size.height()) > 0.5;
    window.size = frame.size();
    window.maximized = IsZoomed(hwnd) != FALSE;
    if (resized && !window.areaId.isEmpty()) {
        m_organizer->windowResized(window);
    }
    const int index = monitorAt(frame.center());
    if (index < 0) {
        return;
    }
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    const RECT bounds = monitor.info.bounds;
    const bool covers = !window.maximized && std::abs(frame.left() - bounds.left) <= 1 && std::abs(frame.top() - bounds.top) <= 1
        && std::abs(frame.right() - bounds.right) <= 1 && std::abs(frame.bottom() - bounds.bottom) <= 1;
    const POINT position = rounded(frame.topLeft());
    RECT windowRect = {};
    GetWindowRect(hwnd, &windowRect);
    const POINT windowPosition = {windowRect.left, windowRect.top};
    const bool own = isOwnMove(window, position, windowPosition);
    if (covers != window.fullscreen && !own) {
        window.fullscreen = covers;
        if (!covers) {
            window.canvasKey = monitor.key;
            window.canvasPos = canvas(monitor.key).committed.screenToCanvas(frame.topLeft(), monitor.viewport);
            window.lastRequest = position;
            window.parked = false;
        }
        if (hwnd == m_foreground) {
            publishSnapshot();
        }
        return;
    }
    if (own || window.pinned || window.fullscreen || window.unmovable || window.hidden) {
        return;
    }
    Canvas &home = canvas(window.canvasKey);
    if (home.overview) {
        // The thumbnails follow the canvas, not the physical window.
        return;
    }
    if (window.parked) {
        // Something moved a parked window (Windows rescuing it, a new size): park it again,
        // unless it keeps fighting.
        const ULONGLONG nowTick = GetTickCount64();
        if (nowTick - window.foreignWindowStart > 3000) {
            window.foreignWindowStart = nowTick;
            window.foreignWhileParked = 0;
        }
        if (++window.foreignWhileParked > 3) {
            qCInfo(lcService) << "window refuses to be parked, leaving it" << hwndText(hwnd);
            window.unmovable = true;
            window.parked = false;
            return;
        }
        window.lastRequest = POINT{LONG_MIN, LONG_MIN};
        const int homeIndex = monitorIndexOf(window.canvasKey);
        if (homeIndex >= 0) {
            placeWindow(window, home, m_monitors[std::size_t(homeIndex)]);
        }
        return;
    }
    // Moved by the application or by Windows (Win + arrows): take it over.
    window.canvasKey = monitor.key;
    window.canvasPos = canvas(monitor.key).committed.screenToCanvas(frame.topLeft(), monitor.viewport);
    window.lastRequest = position;
}

void CanvasService::onForeground(HWND hwnd)
{
    m_foreground = hwnd;
    publishSnapshot();
    auto it = m_windows.find(hwnd);
    if (it == m_windows.end() || !m_settings.revealOnActivate || !it->second.onCanvas()) {
        return;
    }
    const ManagedWindow &window = it->second;
    const int index = monitorIndexOf(window.canvasKey);
    if (index < 0) {
        return;
    }
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    Canvas &home = canvas(window.canvasKey);
    if (home.overview || !InfinitDesktop::isFullyVisible(targetCamera(home), canvasRect(window), monitor.viewport, monitor.work)) {
        reveal(hwnd, m_settings.revealMode);
    }
}

void CanvasService::onMoveSizeStart(HWND hwnd)
{
    auto it = m_windows.find(hwnd);
    if (it == m_windows.end() || !it->second.onCanvas()) {
        return;
    }
    POINT cursor = {};
    GetCursorPos(&cursor);
    const QRectF frame = frameOf(hwnd);
    // A drag of the title area is a move; anything at the borders a resize.
    const bool isMove = frame.contains(QPointF(cursor.x, cursor.y)) && cursor.y < frame.top() + 48 && cursor.x > frame.left() + 8
        && cursor.x < frame.right() - 8;
    it->second.moving = true;
    m_userMove = UserMove{hwnd, isMove, monitorAt(frame.center()), QPointF()};
    if (isMove && (m_settings.edgePan || m_settings.flingToOtherScreen)) {
        // Snap would maximize or tile at the edges we scroll at; only for this drag and
        // without touching the user's profile (no SPIF_UPDATEINIFILE).
        BOOL snap = FALSE;
        SystemParametersInfoW(SPI_GETWINARRANGING, 0, &snap, 0);
        if (snap) {
            m_snapBefore = snap;
            setSnap(false);
            m_snapChanged = true;
        }
    }
    publishSnapshot();
}

void CanvasService::onMoveSizeEnd(HWND hwnd)
{
    if (m_userMove.hwnd == hwnd) {
        m_userMove = UserMove{};
        m_edgeTimer.stop();
        if (m_snapChanged) {
            setSnap(m_snapBefore != FALSE);
            m_snapChanged = false;
        }
        publishSnapshot();
    }
    auto it = m_windows.find(hwnd);
    if (it == m_windows.end()) {
        return;
    }
    ManagedWindow &window = it->second;
    window.moving = false;
    const QRectF frame = frameOf(hwnd);
    if (frame.isEmpty()) {
        return;
    }
    window.size = frame.size();
    window.lastRequest = rounded(frame.topLeft());
    window.parked = false;
    if (window.pinned) {
        m_organizer->windowArranged(window);
        return;
    }
    const int index = monitorAt(frame.center());
    if (index < 0) {
        return;
    }
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    window.canvasKey = monitor.key;
    window.canvasPos = canvas(monitor.key).committed.screenToCanvas(frame.topLeft(), monitor.viewport);
    m_organizer->windowArranged(window);
    saveStateLater();
}

// --- Layout ------------------------------------------------------------------------------------

void CanvasService::requestFlush()
{
    if (!m_applyTimer.isActive()) {
        m_applyTimer.start();
    }
}

void CanvasService::flush()
{
    m_applyTimer.stop();
    for (auto &[key, canvas] : m_canvases) {
        if (!canvas.dirty) {
            continue;
        }
        canvas.dirty = false;
        if (canvas.overview) {
            updateOverview(canvas);
        } else {
            applyCanvas(canvas, m_userMove.hwnd);
        }
    }
    if (m_markersDirty) {
        updateMarkersNow();
    }
    if (m_organizer->overlayOpen()) {
        m_organizer->refresh();
    }
}

void CanvasService::applyAll()
{
    for (auto &[key, canvas] : m_canvases) {
        canvas.dirty = true;
    }
    flush();
}

void CanvasService::applyCanvas(Canvas &canvas, HWND except)
{
    if (canvas.overview || m_safeMode || m_shuttingDown) {
        return;
    }
    const int index = monitorIndexOf(canvas.key);
    if (index < 0) {
        return;
    }
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    for (auto &[hwnd, window] : m_windows) {
        if (hwnd != except && window.canvasKey == canvas.key && window.onCanvas()) {
            placeWindow(window, canvas, monitor);
        }
    }
}

void CanvasService::placeWindow(ManagedWindow &window, const Canvas &canvas, const MonitorState &monitor)
{
    const QPointF screen = canvas.committed.canvasToScreen(window.canvasPos, monitor.viewport);
    const QRectF rect(screen, window.size);
    if (rect.intersects(monitor.viewport)) {
        window.parked = false;
        requestMove(window, rounded(screen));
        return;
    }
    // Off its monitor: park it in the void next to it, where it can get no input.
    QList<QRectF> rects;
    for (const MonitorState &m : m_monitors) {
        rects.append(m.viewport);
    }
    const int index = int(&monitor - m_monitors.data());
    const InfinitDesktop::ParkingSpot spot = InfinitDesktop::computeParkingSpot(rects, index, window.size);
    window.parked = true;
    requestMove(window, rounded(spot.topLeft));
}

void CanvasService::requestMove(ManagedWindow &window, POINT position)
{
    if (window.lastRequest.x == position.x && window.lastRequest.y == position.y) {
        return;
    }
    window.lastRequest = position;
    DWORD error = 0;
    POINT windowPosition = {};
    const bool moved = moveFrameTo(window.hwnd, position, false, &error, &windowPosition);
    ManagedWindow::Request &request = window.requests[std::size_t(window.requestHead)];
    request = ManagedWindow::Request{position, windowPosition, GetTickCount64()};
    window.requestHead = (window.requestHead + 1) % int(window.requests.size());
    window.requestCount = std::min<int>(window.requestCount + 1, int(window.requests.size()));
    if (!moved) {
        if (error == ERROR_ACCESS_DENIED) {
            // Elevated: Windows does not let a normal process move it.
            window.unmovable = true;
            qCInfo(lcService) << "cannot move an elevated window" << hwndText(window.hwnd);
        }
    }
}

// --- Camera ------------------------------------------------------------------------------------

Camera CanvasService::targetCamera(const Canvas &canvas) const
{
    return canvas.motion.isActive() ? canvas.motion.target() : canvas.camera;
}

void CanvasService::setCamera(Canvas &canvas, const Camera &camera)
{
    canvas.camera = camera;
    if (!camera.isUnitZoom() && !canvas.overview) {
        enterOverview(canvas);
    }
    if (!canvas.overview) {
        canvas.camera.zoom = 1;
        canvas.committed = canvas.camera;
    }
    canvas.dirty = true;
    updateMarkers(true);
    saveStateLater();
}

void CanvasService::animateTo(Canvas &canvas, const Camera &target)
{
    animateTo(canvas, target, m_settings.animationDuration);
}

void CanvasService::animateTo(Canvas &canvas, const Camera &target, std::chrono::milliseconds duration)
{
    if (duration.count() <= 0) {
        canvas.motion.stop();
        setCamera(canvas, target);
        if (canvas.overview && target.isUnitZoom()) {
            leaveOverview(canvas);
        }
        publishSnapshot();
        return;
    }
    if (!target.isUnitZoom() && !canvas.overview) {
        enterOverview(canvas);
    }
    canvas.motion.startTransition(canvas.camera, target, duration, now());
    ensureTicking();
}

void CanvasService::ensureTicking()
{
    if (!m_tickTimer.isActive()) {
        m_tickTimer.start();
        publishSnapshot();
    }
}

void CanvasService::tick()
{
    const InfinitDesktop::TimePoint time = now();
    bool active = false;
    for (auto &[key, canvas] : m_canvases) {
        if (!canvas.motion.isActive()) {
            continue;
        }
        const bool finished = canvas.motion.isFinished(time);
        Camera camera = finished ? canvas.motion.target() : canvas.motion.sample(time);
        if (finished) {
            canvas.motion.stop();
        }
        setCamera(canvas, camera);
        if (finished && canvas.overview && camera.isUnitZoom()) {
            leaveOverview(canvas);
        }
        active |= !finished;
    }
    // Animation frames are applied right away, not 16 ms later.
    flush();
    if (!active) {
        m_tickTimer.stop();
        publishSnapshot();
        hideMarkersLater();
    }
}

void CanvasService::zoomAt(Canvas &canvas, const QPointF &anchor, qreal steps)
{
    const int index = monitorIndexOf(canvas.key);
    if (index < 0) {
        return;
    }
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    const qreal direction = m_settings.invertWheelZoom ? -1 : 1;
    const Camera base = targetCamera(canvas);
    const qreal zoom = InfinitDesktop::clampZoom(base.zoom * std::pow(m_settings.zoomStep, steps * direction), m_settings.minZoom, m_settings.maxZoom,
                                                 m_settings.snapZoomToOne ? 0.02 : 0.0);
    animateTo(canvas, base.zoomedAt(anchor, zoom, monitor.viewport), ZoomStepDuration);
}

void CanvasService::enterOverview(Canvas &canvas)
{
    if (canvas.overview) {
        return;
    }
    const int index = monitorIndexOf(canvas.key);
    if (index < 0) {
        return;
    }
    MonitorState &monitor = m_monitors[std::size_t(index)];
    canvas.overview = true;
    canvas.diveTarget = nullptr;
    if (!monitor.overview) {
        monitor.overview = std::make_unique<Overview>(this, index);
    }
    monitor.overview->show(monitor.info.work);
    m_stackingDirty = true;
    updateOverview(canvas);
    publishSnapshot();
    qCDebug(lcService) << "overview on" << canvas.key;
}

void CanvasService::leaveOverview(Canvas &canvas)
{
    if (!canvas.overview) {
        return;
    }
    canvas.overview = false;
    canvas.camera.zoom = 1;
    canvas.committed = canvas.camera;
    const int index = monitorIndexOf(canvas.key);
    if (index >= 0 && m_monitors[std::size_t(index)].overview) {
        m_monitors[std::size_t(index)].overview->hide();
    }
    applyCanvas(canvas, m_userMove.hwnd);
    const HWND dive = std::exchange(canvas.diveTarget, nullptr);
    if (dive && IsWindow(dive)) {
        activate(dive);
    }
    updateMarkers(true);
    publishSnapshot();
    qCDebug(lcService) << "overview off" << canvas.key;
}

QList<HWND> CanvasService::stackingOrder()
{
    if (m_stackingDirty) {
        m_stacking.clear();
        for (HWND hwnd : enumerateTopLevel()) {
            if (m_windows.count(hwnd)) {
                m_stacking.append(hwnd);
            }
        }
        m_stackingDirty = false;
    }
    return m_stacking;
}

void CanvasService::updateOverview(Canvas &canvas)
{
    if (!canvas.overview) {
        return;
    }
    const int index = monitorIndexOf(canvas.key);
    if (index < 0 || !m_monitors[std::size_t(index)].overview) {
        return;
    }
    MonitorState &monitor = m_monitors[std::size_t(index)];
    QList<OverviewItem> items;
    const QList<HWND> order = stackingOrder();
    for (auto it = order.crbegin(); it != order.crend(); ++it) {
        auto found = m_windows.find(*it);
        if (found == m_windows.end() || !IsWindow(*it)) {
            continue;
        }
        const ManagedWindow &window = found->second;
        if (window.canvasKey != canvas.key || window.hidden || window.minimized) {
            continue;
        }
        RECT windowRect = {};
        GetWindowRect(*it, &windowRect);
        const QRectF frame = frameOf(*it);
        OverviewItem item;
        item.window = *it;
        item.source = RECT{LONG(frame.left() - windowRect.left), LONG(frame.top() - windowRect.top), LONG(frame.right() - windowRect.left),
                           LONG(frame.bottom() - windowRect.top)};
        item.movable = window.onCanvas() || window.moving;
        // Pinned, fullscreen and elevated windows stay where they are on the screen.
        item.visual = item.movable ? canvas.camera.canvasToScreen(canvasRect(window), monitor.viewport) : frame;
        items.append(item);
    }
    monitor.overview->setItems(items, canvas.camera.zoom);
    Overview::Grid grid;
    grid.visible = true;
    grid.spacing = m_settings.gridSpacing * canvas.camera.zoom;
    grid.origin = canvas.camera.canvasToScreen(QPointF(0, 0), monitor.viewport);
    grid.color = m_settings.gridColor;
    grid.opacity = std::max(0.15, m_settings.gridOpacity);
    monitor.overview->setGrid(grid);
    monitor.overview->setAreas(m_settings.showAreas == ShowMode::Never
                                   ? QList<OverviewArea>()
                                   : m_organizer->overviewAreas(canvas, monitor.viewport, monitor.info.dpi / 96.0));
}

void CanvasService::reveal(HWND hwnd, InfinitDesktop::RevealMode mode)
{
    auto it = m_windows.find(hwnd);
    if (it == m_windows.end() || !it->second.onCanvas()) {
        return;
    }
    qCDebug(lcService) << "reveal" << hwndText(hwnd) << int(mode);
    const int index = monitorIndexOf(it->second.canvasKey);
    if (index < 0) {
        return;
    }
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    Canvas &home = canvas(it->second.canvasKey);
    const Camera target =
        InfinitDesktop::revealCamera(targetCamera(home), canvasRect(it->second), monitor.viewport, monitor.work, mode, RevealMargin, 1.0);
    animateTo(home, target);
}

void CanvasService::showAll(Canvas &canvas, bool toggle)
{
    const int index = monitorIndexOf(canvas.key);
    if (index < 0) {
        return;
    }
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    QList<QRectF> rects;
    for (const auto &[hwnd, window] : m_windows) {
        if (window.canvasKey == canvas.key && window.onCanvas()) {
            rects.append(canvasRect(window));
        }
    }
    if (rects.isEmpty()) {
        return;
    }
    const Camera fit = InfinitDesktop::fitCamera(InfinitDesktop::boundingRect(rects), monitor.viewport, monitor.work, 48, m_settings.minZoom, 1.0);
    if (toggle && canvas.beforeShowAll && targetCamera(canvas).fuzzyEquals(fit, 1.0, 1e-3)) {
        const Camera previous = *canvas.beforeShowAll;
        canvas.beforeShowAll.reset();
        animateTo(canvas, previous);
        return;
    }
    canvas.beforeShowAll = targetCamera(canvas);
    animateTo(canvas, fit);
}

void CanvasService::goHome(Canvas &canvas)
{
    const Camera home{QPointF(0, 0), 1.0};
    if (canvas.beforeHome && targetCamera(canvas).fuzzyEquals(home, 1.0)) {
        const Camera previous = *canvas.beforeHome;
        canvas.beforeHome.reset();
        animateTo(canvas, previous);
        return;
    }
    canvas.beforeHome = targetCamera(canvas);
    animateTo(canvas, home);
}

void CanvasService::activate(HWND hwnd)
{
    if (!IsWindow(hwnd)) {
        return;
    }
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    forceForeground(hwnd);
}

void CanvasService::updateMarkers(bool navigating)
{
    m_markersDirty = true;
    m_markersNavigating |= navigating;
    requestFlush();
}

void CanvasService::updateMarkersNow()
{
    const bool navigating = std::exchange(m_markersNavigating, false);
    m_markersDirty = false;
    if (m_safeMode || m_shuttingDown) {
        return;
    }
    const ShowMode mode = m_settings.offscreenMarkers;
    const bool show = mode == ShowMode::Always || (mode == ShowMode::WhileNavigating && (navigating || m_markerTimer.isActive()));
    for (MonitorState &monitor : m_monitors) {
        if (!show) {
            if (monitor.markers) {
                monitor.markers->hide();
            }
            continue;
        }
        const Canvas &home = canvas(monitor.key);
        QList<QRectF> rects;
        QList<HWND> handles;
        for (const auto &[hwnd, window] : m_windows) {
            if (window.canvasKey == monitor.key && window.onCanvas()) {
                rects.append(home.camera.canvasToScreen(canvasRect(window), monitor.viewport));
                handles.append(hwnd);
            }
        }
        const QList<MarkerPlace> places = layoutMarkers(monitor.work, rects, Markers::Size, 10);
        if (places.isEmpty()) {
            if (monitor.markers) {
                monitor.markers->hide();
            }
            continue;
        }
        if (!monitor.markers) {
            monitor.markers = std::make_unique<Markers>([this](HWND target) {
                // Fly there, at the current zoom.
                auto it = m_windows.find(target);
                if (it == m_windows.end()) {
                    return;
                }
                const int index = monitorIndexOf(it->second.canvasKey);
                if (index < 0) {
                    return;
                }
                const MonitorState &home = m_monitors[std::size_t(index)];
                Canvas &targetCanvas = canvas(it->second.canvasKey);
                const Camera current = targetCamera(targetCanvas);
                animateTo(targetCanvas,
                          InfinitDesktop::revealCamera(current, canvasRect(it->second), home.viewport, home.work, InfinitDesktop::RevealMode::Center,
                                                       RevealMargin, current.zoom));
                if (current.isUnitZoom()) {
                    activate(target);
                }
            });
        }
        QList<Markers::Marker> markers;
        for (const MarkerPlace &place : places) {
            const HWND hwnd = handles[place.window];
            auto icon = m_icons.find(hwnd);
            if (icon == m_icons.end()) {
                icon = m_icons.insert(hwnd, windowIcon(hwnd, 28));
            }
            markers.append(Markers::Marker{hwnd, place.position, place.direction, *icon});
        }
        monitor.markers->show(markers);
    }
    if (navigating && mode == ShowMode::WhileNavigating) {
        m_markerTimer.start();
    }
}

void CanvasService::hideMarkersLater()
{
    if (m_settings.offscreenMarkers == ShowMode::WhileNavigating) {
        m_markerTimer.start();
    }
}

// --- Input -------------------------------------------------------------------------------------

void CanvasService::drainInput()
{
    std::vector<InputCommand> commands;
    {
        std::lock_guard lock(m_inputMutex);
        commands.swap(m_inputQueue);
        m_inputPosted = false;
    }
    for (const InputCommand &command : commands) {
        handleCommand(command);
    }
}

void CanvasService::handleCommand(const InputCommand &command)
{
    if (m_safeMode || m_shuttingDown) {
        return;
    }
    switch (command.kind) {
    case InputCommand::Kind::PanBegin: {
        Canvas *target = canvasAt(command.point);
        if (!target) {
            return;
        }
        target->motion.stop();
        m_panCanvas = target->key;
        m_panVelocity.reset();
        m_panPosition = command.point;
        publishSnapshot();
        return;
    }
    case InputCommand::Kind::PanDelta: {
        auto it = m_canvases.find(m_panCanvas);
        if (it == m_canvases.end()) {
            return;
        }
        m_panPosition += command.delta;
        m_panVelocity.addSample(m_panPosition, command.time);
        setCamera(it->second, it->second.camera.pannedByScreenDelta(command.delta));
        return;
    }
    case InputCommand::Kind::PanEnd: {
        auto it = m_canvases.find(m_panCanvas);
        m_panCanvas.clear();
        if (it == m_canvases.end()) {
            return;
        }
        Canvas &target = it->second;
        const QPointF velocity = m_panVelocity.flingVelocity(command.time);
        if (m_settings.momentum && !velocity.isNull()
            && target.motion.startGlide(target.camera, -velocity / target.camera.zoom, m_settings.friction, now())) {
            ensureTicking();
        } else {
            hideMarkersLater();
        }
        return;
    }
    case InputCommand::Kind::StopMotion:
        for (auto &[key, canvas] : m_canvases) {
            if (canvas.motion.isActive()) {
                const Camera stopped = canvas.motion.sample(now());
                canvas.motion.stop();
                setCamera(canvas, stopped);
                if (canvas.overview && stopped.isUnitZoom()) {
                    leaveOverview(canvas);
                }
            }
        }
        publishSnapshot();
        return;
    case InputCommand::Kind::Zoom:
        if (Canvas *target = canvasAt(command.point)) {
            zoomAt(*target, command.point, command.steps);
        }
        return;
    case InputCommand::Kind::ScrollPan:
        if (Canvas *target = canvasAt(command.point)) {
            target->motion.stop();
            const qreal sign = m_settings.invertScrollPan ? -1 : 1;
            setCamera(*target, target->camera.pannedByScreenDelta(command.delta * m_settings.panSpeed * sign));
            hideMarkersLater();
        }
        return;
    case InputCommand::Kind::Shortcut:
        triggerShortcut(command.action, command.repeat);
        return;
    case InputCommand::Kind::EdgeChanged:
        m_userMove.edge = command.delta;
        if (command.delta.isNull() || !m_userMove.hwnd) {
            m_edgeTimer.stop();
        } else if (!m_edgeTimer.isActive()) {
            m_edgeLast = std::chrono::steady_clock::now();
            m_edgeTimer.start();
        }
        return;
    case InputCommand::Kind::MonitorCrossed:
        if (m_userMove.hwnd) {
            m_userMove.monitor = command.monitor;
            m_userMove.edge = QPointF();
            m_edgeTimer.stop();
            publishSnapshot();
        }
        return;
    case InputCommand::Kind::CancelOverlay:
        m_organizer->closeOverlay();
        return;
    case InputCommand::Kind::CanvasMenu: {
        const QPointF point = command.point;
        // Outside of the input loop: the menu runs its own message loop.
        QTimer::singleShot(0, this, [this, point]() {
            if (!m_shuttingDown && !m_safeMode) {
                m_organizer->showCanvasMenu(point);
            }
        });
        return;
    }
    }
}

void CanvasService::edgeTick()
{
    const auto time = std::chrono::steady_clock::now();
    const qreal seconds = std::min(0.05, std::chrono::duration<qreal>(time - m_edgeLast).count());
    m_edgeLast = time;
    Canvas *target = canvasOfMonitor(m_userMove.monitor);
    if (!target || !m_userMove.hwnd || m_userMove.edge.isNull()) {
        m_edgeTimer.stop();
        return;
    }
    // The canvas scrolls towards the edge; the dragged window stays under the pointer.
    const QPointF delta = m_userMove.edge * m_settings.edgePanSpeed * seconds;
    target->motion.stop();
    setCamera(*target, Camera{target->camera.center + delta, target->camera.zoom});
}

void CanvasService::triggerShortcut(ShortcutAction action, bool repeat)
{
    const ShortcutDefinition *definition = findShortcut(action);
    if (!definition || (repeat && !definition->repeats)) {
        return;
    }
    if (action == ShortcutAction::ToggleEnabled) {
        setPaused(!m_paused);
        return;
    }
    POINT cursor = {};
    GetCursorPos(&cursor);
    const QPointF pointer(cursor.x, cursor.y);
    int index = -1;
    Canvas *target = canvasAt(pointer, &index);
    if (!target || index < 0) {
        return;
    }
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    const auto bookmark = [&](int slot, bool save) {
        if (save) {
            target->bookmarks[std::size_t(slot)] = targetCamera(*target);
            saveStateLater();
        } else if (target->bookmarks[std::size_t(slot)]) {
            animateTo(*target, *target->bookmarks[std::size_t(slot)]);
        }
    };
    const auto pan = [&](qreal dx, qreal dy) {
        const Camera base = targetCamera(*target);
        const QPointF step(monitor.viewport.width() * m_settings.keyboardPanStep / base.zoom, monitor.viewport.height() * m_settings.keyboardPanStep / base.zoom);
        animateTo(*target, Camera{base.center + QPointF(dx * step.x(), dy * step.y()), base.zoom});
    };
    const auto focus = [&](InfinitDesktop::Direction direction) {
        const HWND current = GetForegroundWindow();
        QRectF from(targetCamera(*target).center, QSizeF(1, 1));
        if (auto it = m_windows.find(current); it != m_windows.end() && it->second.canvasKey == target->key) {
            from = canvasRect(it->second);
        }
        QList<QRectF> rects;
        QList<HWND> handles;
        for (const auto &[hwnd, window] : m_windows) {
            if (hwnd != current && window.canvasKey == target->key && window.onCanvas()) {
                rects.append(canvasRect(window));
                handles.append(hwnd);
            }
        }
        const int found = InfinitDesktop::findNeighbor(from, rects, direction);
        if (found >= 0) {
            activate(handles[found]);
            reveal(handles[found], m_settings.revealMode);
        }
    };
    switch (action) {
    case ShortcutAction::ZoomIn:
        zoomAt(*target, pointer, 1);
        break;
    case ShortcutAction::ZoomOut:
        zoomAt(*target, pointer, -1);
        break;
    case ShortcutAction::ZoomReset:
        animateTo(*target, targetCamera(*target).zoomedAt(pointer, 1.0, monitor.viewport));
        break;
    case ShortcutAction::Overview:
        showAll(*target, true);
        break;
    case ShortcutAction::Home:
        goHome(*target);
        break;
    case ShortcutAction::CenterWindow:
        reveal(GetForegroundWindow(), InfinitDesktop::RevealMode::Center);
        break;
    case ShortcutAction::PanLeft:
        pan(-1, 0);
        break;
    case ShortcutAction::PanRight:
        pan(1, 0);
        break;
    case ShortcutAction::PanUp:
        pan(0, -1);
        break;
    case ShortcutAction::PanDown:
        pan(0, 1);
        break;
    case ShortcutAction::FocusLeft:
        focus(InfinitDesktop::Direction::Left);
        break;
    case ShortcutAction::FocusRight:
        focus(InfinitDesktop::Direction::Right);
        break;
    case ShortcutAction::FocusUp:
        focus(InfinitDesktop::Direction::Up);
        break;
    case ShortcutAction::FocusDown:
        focus(InfinitDesktop::Direction::Down);
        break;
    case ShortcutAction::GotoBookmark1:
    case ShortcutAction::GotoBookmark2:
    case ShortcutAction::GotoBookmark3:
    case ShortcutAction::GotoBookmark4:
        bookmark(int(action) - int(ShortcutAction::GotoBookmark1), false);
        break;
    case ShortcutAction::SaveBookmark1:
    case ShortcutAction::SaveBookmark2:
    case ShortcutAction::SaveBookmark3:
    case ShortcutAction::SaveBookmark4:
        bookmark(int(action) - int(ShortcutAction::SaveBookmark1), true);
        break;
    case ShortcutAction::TogglePin:
        togglePin(false);
        break;
    case ShortcutAction::TogglePinAbove:
        togglePin(true);
        break;
    case ShortcutAction::ToggleEnabled:
        break;
    case ShortcutAction::NewNote:
        m_organizer->newNote(pointer);
        break;
    case ShortcutAction::ToggleNotes:
        m_organizer->toggleNotes();
        break;
    case ShortcutAction::ShowMap:
        m_organizer->toggleMap(index);
        break;
    case ShortcutAction::GroupFromSelection:
        m_organizer->startSelection(true, index);
        break;
    case ShortcutAction::AreaFromSelection:
        m_organizer->startSelection(false, index);
        break;
    case ShortcutAction::SaveGroupLayout:
        m_organizer->saveGroupLayout();
        break;
    case ShortcutAction::OpenGroup1:
    case ShortcutAction::OpenGroup2:
    case ShortcutAction::OpenGroup3:
    case ShortcutAction::OpenGroup4:
    case ShortcutAction::OpenGroup5:
    case ShortcutAction::OpenGroup6:
    case ShortcutAction::OpenGroup7:
    case ShortcutAction::OpenGroup8:
    case ShortcutAction::OpenGroup9:
        m_organizer->openGroup(int(action) - int(ShortcutAction::OpenGroup1));
        break;
    }
}

void CanvasService::togglePin(bool above)
{
    const HWND hwnd = GetForegroundWindow();
    auto it = m_windows.find(hwnd);
    if (it == m_windows.end()) {
        return;
    }
    ManagedWindow &window = it->second;
    if (!window.pinned) {
        setPinned(window, true, above);
    } else if (above && !window.pinnedAbove) {
        setPinned(window, true, true);
    } else {
        setPinned(window, false, false);
    }
}

void CanvasService::setPinned(ManagedWindow &window, bool pinned, bool above)
{
    const HWND hwnd = window.hwnd;
    above = above && pinned;
    if (pinned == window.pinned && above == window.pinnedAbove) {
        return;
    }
    if (above != window.pinnedAbove) {
        SetWindowPos(hwnd, above ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        window.pinnedAbove = above;
    }
    if (!pinned && window.pinned) {
        // Released: back on the canvas where it is now.
        window.pinned = false;
        const QRectF frame = frameOf(hwnd);
        const int index = monitorAt(frame.center());
        if (index >= 0) {
            const MonitorState &monitor = m_monitors[std::size_t(index)];
            window.canvasKey = monitor.key;
            window.canvasPos = canvas(monitor.key).committed.screenToCanvas(frame.topLeft(), monitor.viewport);
            window.lastRequest = rounded(frame.topLeft());
        }
    } else if (pinned && !window.pinned) {
        // Pinned while parked or zoomed out: it stays where it is on the screen now.
        window.pinned = true;
        if (window.parked) {
            const int index = monitorIndexOf(window.canvasKey);
            if (index >= 0) {
                const QRectF &work = m_monitors[std::size_t(index)].work;
                moveFrameTo(hwnd, rounded(QPointF(work.center().x() - window.size.width() / 2, work.center().y() - window.size.height() / 2)));
            }
            window.parked = false;
        }
    }
    m_organizer->pinChanged(window);
    if (Canvas *home = m_canvases.count(window.canvasKey) ? &m_canvases.at(window.canvasKey) : nullptr; home && home->overview) {
        updateOverview(*home);
    }
}

bool CanvasService::isLocked(int monitor) const
{
    return m_snapshot && monitor >= 0 && monitor < m_snapshot->locked.size() && m_snapshot->locked[monitor];
}

void CanvasService::forceForeground(HWND hwnd)
{
    if (!IsWindow(hwnd) || GetForegroundWindow() == hwnd) {
        return;
    }
    if (SetForegroundWindow(hwnd) && GetForegroundWindow() == hwnd) {
        return;
    }
    // Windows only lets the process with the last input set the foreground window. An
    // unassigned key (as for the Start menu mask) counts as input.
    INPUT inputs[2] = {};
    for (INPUT &input : inputs) {
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = InputHook::maskKey;
        input.ki.dwExtraInfo = InputHook::injectedTag;
    }
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
    SetForegroundWindow(hwnd);
}

// --- Overview host -----------------------------------------------------------------------------

void CanvasService::overviewPan(int monitor, const QPointF &screenDelta)
{
    if (Canvas *target = canvasOfMonitor(monitor)) {
        target->motion.stop();
        setCamera(*target, target->camera.pannedByScreenDelta(screenDelta));
    }
}

void CanvasService::overviewPanEnd(int monitor, const QPointF &velocity)
{
    Canvas *target = canvasOfMonitor(monitor);
    if (target && m_settings.momentum && !velocity.isNull()
        && target->motion.startGlide(target->camera, -velocity / target->camera.zoom, m_settings.friction, now())) {
        ensureTicking();
    } else {
        hideMarkersLater();
    }
}

void CanvasService::overviewZoom(int monitor, const QPointF &anchor, qreal steps)
{
    if (Canvas *target = canvasOfMonitor(monitor)) {
        zoomAt(*target, anchor, steps);
    }
}

void CanvasService::overviewMoveWindow(HWND hwnd, const QPointF &screenDelta)
{
    auto it = m_windows.find(hwnd);
    if (it == m_windows.end() || !it->second.onCanvas()) {
        return;
    }
    Canvas &home = canvas(it->second.canvasKey);
    it->second.canvasPos += screenDelta / home.camera.zoom;
    home.dirty = true;
    requestFlush();
}

void CanvasService::overviewResizeWindow(HWND hwnd, unsigned edges, const QPointF &screenDelta)
{
    auto it = m_windows.find(hwnd);
    if (it == m_windows.end() || !it->second.onCanvas()) {
        return;
    }
    ManagedWindow &window = it->second;
    Canvas &home = canvas(window.canvasKey);
    const QPointF delta = screenDelta / home.camera.zoom;
    QRectF rect = canvasRect(window);
    if (edges & EdgeLeft) {
        rect.setLeft(std::min(rect.left() + delta.x(), rect.right() - 160));
    }
    if (edges & EdgeRight) {
        rect.setRight(std::max(rect.right() + delta.x(), rect.left() + 160));
    }
    if (edges & EdgeTop) {
        rect.setTop(std::min(rect.top() + delta.y(), rect.bottom() - 80));
    }
    if (edges & EdgeBottom) {
        rect.setBottom(std::max(rect.bottom() + delta.y(), rect.top() + 80));
    }
    window.canvasPos = rect.topLeft();
    // The size changes physically right away (the position at 100 %).
    resizeFrame(window, rect.size());
    home.dirty = true;
    requestFlush();
}

void CanvasService::overviewDragEnd(HWND hwnd)
{
    if (auto it = m_windows.find(hwnd); it != m_windows.end()) {
        m_organizer->windowArranged(it->second);
    }
    saveStateLater();
}

void CanvasService::overviewDive(HWND hwnd)
{
    auto it = m_windows.find(hwnd);
    if (it == m_windows.end()) {
        return;
    }
    Canvas &home = canvas(it->second.canvasKey);
    const int index = monitorIndexOf(home.key);
    if (index < 0) {
        return;
    }
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    Camera target{targetCamera(home).center, 1.0};
    if (it->second.onCanvas()) {
        target = InfinitDesktop::revealCamera(targetCamera(home), canvasRect(it->second), monitor.viewport, monitor.work,
                                              InfinitDesktop::RevealMode::Center, RevealMargin, 1.0);
    }
    home.diveTarget = hwnd;
    animateTo(home, target);
}

void CanvasService::overviewContextMenu(int monitor, const QPointF &point)
{
    Q_UNUSED(monitor)
    // Not from inside the overview's window procedure: the menu runs its own message loop.
    QTimer::singleShot(0, this, [this, point]() {
        if (!m_shuttingDown) {
            m_organizer->showCanvasMenu(point);
        }
    });
}

void CanvasService::overviewCommand(int monitor, Command command)
{
    Canvas *target = canvasOfMonitor(monitor);
    switch (command) {
    case Command::ZoomReset:
        if (target) {
            animateTo(*target, Camera{targetCamera(*target).center, 1.0});
        }
        break;
    case Command::ShowAll:
        if (target) {
            showAll(*target, false);
        }
        break;
    case Command::Settings:
        openSettingsApp();
        break;
    case Command::Quit:
        // Not from inside the overview's window procedure.
        QTimer::singleShot(0, this, &CanvasService::quitRequested);
        break;
    }
}

// --- Control -----------------------------------------------------------------------------------

void CanvasService::setPaused(bool paused)
{
    if (paused == m_paused) {
        return;
    }
    m_paused = paused;
    publishSnapshot();
    if (m_tray) {
        m_tray->setState(Tray::State{m_paused, m_safeMode, m_paused ? text(L"Gestures paused", L"Gesten pausiert") : std::wstring()});
    }
    Q_EMIT statusChanged();
}

void CanvasService::leaveSafeMode()
{
    if (!m_safeMode) {
        return;
    }
    m_safeMode = false;
    m_events.start([this](DWORD event, HWND hwnd) {
        onWindowEvent(event, hwnd);
    });
    adoptExistingWindows();
    startHooks();
    m_touchpad.start(m_serviceWindow.get());
    applyAll();
    m_organizer->start();
    publishSnapshot();
    if (m_tray) {
        m_tray->setState(Tray::State{m_paused, false, {}});
    }
    Q_EMIT statusChanged();
}

void CanvasService::setTray(Tray *tray)
{
    m_tray = tray;
    if (m_tray) {
        m_tray->setState(Tray::State{m_paused, m_safeMode, m_safeMode ? text(L"Safe mode", L"Abgesicherter Modus") : std::wstring()});
    }
}

void CanvasService::bringWindowsBack()
{
    for (auto &[key, home] : m_canvases) {
        home.motion.stop();
        if (home.overview) {
            leaveOverview(home);
        }
    }
    QHash<QString, int> cascade;
    for (auto &[hwnd, window] : m_windows) {
        if (!window.onCanvas()) {
            continue;
        }
        const int index = monitorIndexOf(window.canvasKey);
        if (index < 0) {
            continue;
        }
        const MonitorState &monitor = m_monitors[std::size_t(index)];
        Canvas &home = canvas(window.canvasKey);
        const QRectF visual = home.committed.canvasToScreen(canvasRect(window), monitor.viewport);
        if (monitor.work.intersects(visual)) {
            continue;
        }
        const int step = cascade[window.canvasKey]++ % 12;
        const QRectF &work = monitor.work;
        const QPointF topLeft(std::clamp(work.left() + 40.0 + step * 32, work.left(), std::max(work.left(), work.right() - visual.width())),
                              std::clamp(work.top() + 40.0 + step * 32, work.top(), std::max(work.top(), work.bottom() - visual.height())));
        window.canvasPos = home.committed.screenToCanvas(topLeft, monitor.viewport);
        placeWindow(window, home, monitor);
    }
    saveStateLater();
}

void CanvasService::openSettingsApp()
{
    const QDir directory(QCoreApplication::applicationDirPath());
    for (const QString &name : {QStringLiteral("wid-settings.exe"), QStringLiteral("win-infinit-desktop-settings.exe")}) {
        const QString path = directory.filePath(name);
        if (QFile::exists(path) && QProcess::startDetached(path, {})) {
            return;
        }
    }
    if (m_tray) {
        m_tray->notify(L"WIN Infinit Desktop", text(L"The settings app is not installed yet.", L"Die Einstellungs-App ist noch nicht installiert."));
    }
}

void CanvasService::shutdown()
{
    if (m_shuttingDown || !m_started) {
        return;
    }
    m_shuttingDown = true;
    m_organizer->shutdown();
    stopHooks();
    m_events.stop();
    m_touchpad.stop();
    m_applyTimer.stop();
    m_tickTimer.stop();
    m_edgeTimer.stop();
    if (m_snapChanged) {
        setSnap(m_snapBefore != FALSE);
        m_snapChanged = false;
    }
    for (MonitorState &monitor : m_monitors) {
        if (monitor.overview) {
            monitor.overview->hide();
        }
        if (monitor.markers) {
            monitor.markers->hide();
        }
    }
    // Every window back onto a screen: nothing must stay parked when the canvas is gone.
    QHash<QString, int> cascade;
    for (auto &[hwnd, window] : m_windows) {
        if (!IsWindow(hwnd)) {
            continue;
        }
        if (window.pinnedAbove) {
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
        }
        if (window.unmovable || window.minimized) {
            continue;
        }
        const QRectF frame = frameOf(hwnd);
        const int index = monitorIndexOf(window.canvasKey);
        if (index < 0 || frame.isEmpty()) {
            continue;
        }
        const MonitorState &monitor = m_monitors[std::size_t(index)];
        const bool onScreen = std::any_of(m_monitors.cbegin(), m_monitors.cend(), [&frame](const MonitorState &m) {
            return m.work.intersects(frame);
        });
        if (onScreen) {
            continue;
        }
        const int step = cascade[window.canvasKey]++ % 12;
        const QRectF &work = monitor.work;
        const QPointF topLeft(std::clamp(work.left() + 40.0 + step * 32, work.left(), std::max(work.left(), work.right() - frame.width())),
                              std::clamp(work.top() + 40.0 + step * 32, work.top(), std::max(work.top(), work.bottom() - frame.height())));
        moveFrameTo(hwnd, rounded(topLeft));
    }
    saveState();
    qCInfo(lcService) << "stopped";
}

// --- Persistence -------------------------------------------------------------------------------

void CanvasService::loadState()
{
    QFile canvasFile(QDir(stateDirectory()).filePath(QStringLiteral("canvas.json")));
    if (m_settings.rememberCanvas && canvasFile.open(QIODevice::ReadOnly)) {
        const QJsonObject canvases = QJsonDocument::fromJson(canvasFile.read(4 * 1024 * 1024)).object().value(QStringLiteral("canvases")).toObject();
        for (auto it = canvases.begin(); it != canvases.end(); ++it) {
            const QJsonObject entry = it.value().toObject();
            Canvas &loaded = canvas(it.key());
            if (auto camera = cameraFromJson(entry.value(QStringLiteral("camera")))) {
                // Never restored zoomed out.
                loaded.camera = Camera{camera->center, 1.0};
                loaded.committed = loaded.camera;
            }
            const QJsonArray bookmarks = entry.value(QStringLiteral("bookmarks")).toArray();
            for (int i = 0; i < std::min<int>(4, bookmarks.size()); ++i) {
                loaded.bookmarks[std::size_t(i)] = cameraFromJson(bookmarks[i]);
            }
        }
    }
    QFile placementFile(QDir(stateDirectory()).filePath(QStringLiteral("placements.json")));
    if (placementFile.open(QIODevice::ReadOnly)) {
        m_placements.fromJson(QJsonDocument::fromJson(placementFile.read(16 * 1024 * 1024)).object());
    }
}

void CanvasService::saveStateLater()
{
    m_stateDirty = true;
    if (!m_saveTimer.isActive()) {
        m_saveTimer.start();
    }
}

void CanvasService::saveState()
{
    m_saveTimer.stop();
    m_stateDirty = false;
    QJsonObject canvases;
    for (const auto &[key, entry] : m_canvases) {
        QJsonArray bookmarks;
        for (const auto &bookmark : entry.bookmarks) {
            bookmarks.append(bookmark ? QJsonValue(cameraJson(*bookmark)) : QJsonValue());
        }
        canvases.insert(key, QJsonObject{{QStringLiteral("camera"), cameraJson(entry.committed)}, {QStringLiteral("bookmarks"), bookmarks}});
    }
    const auto write = [](const QString &name, const QJsonObject &object) {
        QSaveFile file(QDir(stateDirectory()).filePath(name));
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
            file.commit();
        }
    };
    write(QStringLiteral("canvas.json"), QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("canvases"), canvases}});
    write(QStringLiteral("placements.json"), m_placements.toJson());
    m_organizer->persist();
}

// --- IPC ---------------------------------------------------------------------------------------

QJsonObject CanvasService::state() const
{
    QJsonArray monitors;
    for (const MonitorState &monitor : m_monitors) {
        const auto it = m_canvases.find(monitor.key);
        QJsonObject entry{
            {QStringLiteral("key"), monitor.key},
            {QStringLiteral("name"), QString::fromStdWString(monitor.info.friendlyName)},
            {QStringLiteral("bounds"), rectJson(monitor.viewport)},
            {QStringLiteral("work"), rectJson(monitor.work)},
        };
        if (it != m_canvases.end()) {
            entry.insert(QStringLiteral("camera"), cameraJson(it->second.camera));
            entry.insert(QStringLiteral("overview"), it->second.overview);
            entry.insert(QStringLiteral("moving"), it->second.motion.isActive());
        }
        monitors.append(entry);
    }
    QJsonArray windows;
    for (const auto &[hwnd, window] : m_windows) {
        QString mode = QStringLiteral("canvas");
        if (window.hidden) {
            mode = QStringLiteral("hidden");
        } else if (window.minimized) {
            mode = QStringLiteral("minimized");
        } else if (window.fullscreen) {
            mode = QStringLiteral("fullscreen");
        } else if (window.pinned) {
            mode = QStringLiteral("pinned");
        } else if (window.moving) {
            mode = QStringLiteral("moving");
        } else if (window.unmovable) {
            mode = QStringLiteral("unmovable");
        }
        windows.append(QJsonObject{
            {QStringLiteral("hwnd"), hwndText(hwnd)},
            {QStringLiteral("program"), window.program},
            {QStringLiteral("class"), window.windowClass},
            {QStringLiteral("app"), window.appKey},
            {QStringLiteral("canvas"), window.canvasKey},
            {QStringLiteral("mode"), mode},
            {QStringLiteral("parked"), window.parked},
            {QStringLiteral("canvasRect"), rectJson(canvasRect(window))},
            {QStringLiteral("frame"), rectJson(frameOf(hwnd))},
            {QStringLiteral("pinnedAbove"), window.pinnedAbove},
            {QStringLiteral("note"), window.noteId},
            {QStringLiteral("group"), window.groupId},
            {QStringLiteral("area"), window.areaId},
        });
    }
    return QJsonObject{
        {QStringLiteral("version"), QStringLiteral(WID_VERSION_STRING)},
        {QStringLiteral("paused"), m_paused},
        {QStringLiteral("safeMode"), m_safeMode},
        {QStringLiteral("monitors"), monitors},
        {QStringLiteral("windows"), windows},
    };
}

QJsonValue CanvasService::handleIpc(const QString &method, const QJsonObject &params, QString *error)
{
    bool handled = false;
    const QJsonValue organized = m_organizer->handleIpc(method, params, error, &handled);
    if (handled) {
        return organized;
    }
    if (method == QLatin1String("version")) {
        return QStringLiteral(WID_VERSION_STRING);
    }
    if (method == QLatin1String("state")) {
        return state();
    }
    if (method == QLatin1String("paused")) {
        return m_paused;
    }
    if (method == QLatin1String("setPaused")) {
        setPaused(params.value(QStringLiteral("paused")).toBool());
        return m_paused;
    }
    if (method == QLatin1String("safeMode")) {
        return m_safeMode;
    }
    if (method == QLatin1String("leaveSafeMode")) {
        leaveSafeMode();
        return m_safeMode;
    }
    if (method == QLatin1String("reloadSettings")) {
        reloadSettings();
        return true;
    }
    if (method == QLatin1String("windowPlacements")) {
        return m_placements.toJson();
    }
    if (method == QLatin1String("forgetWindowPlacements")) {
        m_placements.clear();
        saveStateLater();
        return true;
    }
    if (method == QLatin1String("forgetApplicationPlacements")) {
        const bool removed = m_placements.forget(params.value(QStringLiteral("app")).toString().toLower());
        saveStateLater();
        return removed;
    }
    if (method == QLatin1String("bringWindowsBack")) {
        bringWindowsBack();
        return true;
    }
    if (method == QLatin1String("quit")) {
        QTimer::singleShot(0, this, &CanvasService::quitRequested);
        return true;
    }

    // Navigation: on the monitor under the pointer, or params.monitor, right away.
    if (m_safeMode) {
        *error = QStringLiteral("safe mode");
        return {};
    }
    int index = params.contains(QStringLiteral("monitor")) ? params.value(QStringLiteral("monitor")).toInt(-1) : -1;
    if (index < 0) {
        POINT cursor = {};
        GetCursorPos(&cursor);
        index = monitorAt(QPointF(cursor.x, cursor.y));
    }
    Canvas *target = canvasOfMonitor(index);
    if (!target) {
        *error = QStringLiteral("no such monitor");
        return {};
    }
    const MonitorState &monitor = m_monitors[std::size_t(index)];
    const auto apply = [&](const Camera &camera) {
        target->motion.stop();
        setCamera(*target, camera);
        if (target->overview && camera.isUnitZoom()) {
            leaveOverview(*target);
        }
        // Immediately, for scripts and tests.
        flush();
        publishSnapshot();
        return cameraJson(target->camera);
    };
    const QPointF center = monitor.viewport.center();
    if (method == QLatin1String("pan")) {
        return apply(target->camera.pannedByScreenDelta(QPointF(params.value(QStringLiteral("dx")).toDouble(), params.value(QStringLiteral("dy")).toDouble())));
    }
    if (method == QLatin1String("zoomBy") || method == QLatin1String("setZoom")) {
        const qreal requested = method == QLatin1String("zoomBy") ? target->camera.zoom * params.value(QStringLiteral("factor")).toDouble(1.0)
                                                                  : params.value(QStringLiteral("zoom")).toDouble(1.0);
        if (!std::isfinite(requested) || requested <= 0) {
            *error = QStringLiteral("invalid zoom");
            return {};
        }
        const qreal zoom = InfinitDesktop::clampZoom(requested, m_settings.minZoom, m_settings.maxZoom);
        return apply(target->camera.zoomedAt(center, zoom, monitor.viewport));
    }
    if (method == QLatin1String("resetZoom")) {
        return apply(target->camera.zoomedAt(center, 1.0, monitor.viewport));
    }
    if (method == QLatin1String("goHome")) {
        return apply(Camera{QPointF(0, 0), 1.0});
    }
    if (method == QLatin1String("zoomToFit")) {
        QList<QRectF> rects;
        for (const auto &[hwnd, window] : m_windows) {
            if (window.canvasKey == target->key && window.onCanvas()) {
                rects.append(canvasRect(window));
            }
        }
        if (rects.isEmpty()) {
            return cameraJson(target->camera);
        }
        return apply(InfinitDesktop::fitCamera(InfinitDesktop::boundingRect(rects), monitor.viewport, monitor.work, 48, m_settings.minZoom, 1.0));
    }
    if (method == QLatin1String("setCamera")) {
        const std::optional<Camera> camera = cameraFromJson(params);
        if (!camera) {
            *error = QStringLiteral("invalid camera");
            return {};
        }
        return apply(Camera{camera->center, InfinitDesktop::clampZoom(camera->zoom, m_settings.minZoom, m_settings.maxZoom)});
    }
    *error = QStringLiteral("unknown method %1").arg(method);
    return {};
}

} // namespace Wid
