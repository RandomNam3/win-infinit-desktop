/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "inputhook.h"
#include "shortcuts.h"
#include "velocitytracker.h"

#include <QList>
#include <QPointF>

#include <atomic>
#include <bitset>
#include <functional>
#include <memory>

namespace Wid
{

/**
 * What the hook thread needs to decide about an event, published by the main thread as an
 * immutable snapshot (InputRouter::setSnapshot). Reading it is one atomic load.
 */
struct InputSnapshot
{
    bool enabled = false; ///< false: paused or safe mode, gestures pass through
    bool shortcutsEnabled = false; ///< false in safe mode
    bool dragDesktopToPan = true;
    bool middleButtonPan = true;
    unsigned panModifiers = 0; ///< MOD_* bits, 0 = disabled
    bool wheelZoomOnDesktop = true;
    unsigned zoomModifiers = 0;
    bool shiftWheelPans = true;
    bool touchpadScrollPans = true;
    bool pinchZoom = true;
    bool gliding = false; ///< a camera motion runs: a click only stops it
    QList<RECT> monitors;
    QList<bool> locked; ///< per monitor: a fullscreen window has the focus
    // While the user drags a canvas window (system move loop).
    bool userMove = false;
    int userMoveMonitor = -1;
    bool edgePan = true;
    int edgePanMargin = 24;
    bool flingToOtherScreen = true;
    qreal flingThreshold = 1600;
    ShortcutMap shortcuts;
};

/// One thing for the main thread to do. Pan deltas are in screen pixels.
struct InputCommand
{
    enum class Kind {
        PanBegin, ///< point: where the pan starts
        PanDelta, ///< delta; point: pointer position; time
        PanEnd, ///< time
        StopMotion,
        Zoom, ///< point: anchor; steps: wheel notches, positive = in
        ScrollPan, ///< delta (wheel or touchpad), no momentum
        Shortcut, ///< action; repeat
        EdgeChanged, ///< delta: direction (-1/0/1 per axis) while dragging a window at an edge
        MonitorCrossed, ///< monitor: the dragged window is thrown onto this monitor
    };
    Kind kind = Kind::StopMotion;
    QPointF point;
    QPointF delta;
    qreal steps = 0;
    ShortcutAction action = ShortcutAction::ZoomIn;
    bool repeat = false;
    int monitor = -1;
    std::chrono::microseconds time{0};
};

/**
 * Turns the hook's mouse and keyboard events into pass/swallow decisions and commands.
 * Runs on the hook thread; everything it touches there is owned by it, except the
 * snapshot (atomic) and the touchpad tick (atomic).
 *
 * The rules are those of KDE Infinit Desktop's CanvasInputFilter where Windows allows:
 * - LMB on the empty desktop passes; from a few pixels of movement on it becomes a pan, the
 *   desktop gets a (tagged) button release at once so it does not draw a selection, and the
 *   pointer keeps moving normally.
 * - The pan modifier + LMB pans anywhere (press and release are swallowed).
 * - MMB on the empty desktop pans.
 * - A click while the camera glides only stops it.
 * - Zoom modifier + wheel zooms anywhere. On the empty desktop: two fingers on a precision
 *   touchpad pan, pinch (injected Ctrl + wheel) zooms, Shift + wheel pans horizontally,
 *   Ctrl + wheel vertically, the plain wheel zooms.
 * - Bound shortcuts are swallowed with their key release.
 * - While the user drags a window: entering a neighbour monitor is held back unless the
 *   hand moves faster than the fling threshold; the screen edges report edge scrolling.
 */
class InputRouter
{
public:
    using Sink = std::function<void(const InputCommand &)>;
    /// Is the empty desktop (wallpaper, icon view) under this point? Called on presses and
    /// wheel events only, never on plain moves.
    using DesktopTest = std::function<bool(POINT)>;
    /// Sends the left button release to whatever has the capture (tagged as injected).
    using ReleaseLeft = std::function<void()>;

    InputRouter(Sink sink, DesktopTest desktopTest, ReleaseLeft releaseLeft);

    void setSnapshot(std::shared_ptr<const InputSnapshot> snapshot);
    std::shared_ptr<const InputSnapshot> snapshot() const;
    /// GetTickCount() of the last touchpad report with two or more fingers.
    void setTouchpadTick(DWORD tick);

    HookAction mouse(const MouseEvent &event, unsigned modifiers);
    HookAction key(const KeyEvent &event, unsigned modifiers);

    /// Movement needed on the desktop before a press becomes a pan.
    static constexpr int DragThreshold = 3;

private:
    enum class State {
        Idle,
        Pending, ///< LMB pressed on the desktop, not moved far enough yet
        Panning,
    };
    int monitorAt(const InputSnapshot &snapshot, POINT point) const;
    bool lockedAt(const InputSnapshot &snapshot, POINT point) const;
    HookAction wheel(const MouseEvent &event, unsigned modifiers, const InputSnapshot &snapshot);
    HookAction userMoveMotion(const MouseEvent &event, const InputSnapshot &snapshot);
    void send(InputCommand command);
    static std::chrono::microseconds timeOf(const MouseEvent &event);

    Sink m_sink;
    DesktopTest m_desktopTest;
    ReleaseLeft m_releaseLeft;
    std::atomic<std::shared_ptr<const InputSnapshot>> m_snapshot;
    std::atomic<DWORD> m_touchpadTick{0};

    // Hook thread only.
    State m_state = State::Idle;
    WPARAM m_panButtonUp = 0; ///< WM_LBUTTONUP / WM_MBUTTONUP that ends the pan
    POINT m_pressPoint = {};
    POINT m_lastPoint = {};
    bool m_swallowLeftUp = false;
    bool m_swallowMiddleUp = false;
    bool m_swallowRightUp = false;
    std::bitset<256> m_swallowedKeys;
    // Window drag by the user
    InfinitDesktop::VelocityTracker m_hand;
    QPointF m_handPosition;
    POINT m_lastPassed = {};
    bool m_handValid = false;
    QPointF m_edge;
    int m_userMoveMonitor = -1;
};

} // namespace Wid
