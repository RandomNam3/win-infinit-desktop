/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "inputrouter.h"

#include <cstdlib>
#include <utility>

namespace Wid
{

namespace
{

constexpr DWORD TouchpadWindowMs = 50;

bool contains(const RECT &rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}

bool hasAll(unsigned modifiers, unsigned wanted)
{
    return wanted != 0 && (modifiers & wanted) == wanted;
}

InputCommand make(InputCommand::Kind kind, const QPointF &point = QPointF())
{
    InputCommand command;
    command.kind = kind;
    command.point = point;
    return command;
}

} // namespace

InputRouter::InputRouter(Sink sink, DesktopTest desktopTest, ReleaseLeft releaseLeft)
    : m_sink(std::move(sink))
    , m_desktopTest(std::move(desktopTest))
    , m_releaseLeft(std::move(releaseLeft))
{
    m_snapshot.store(std::make_shared<const InputSnapshot>());
}

void InputRouter::setSnapshot(std::shared_ptr<const InputSnapshot> snapshot)
{
    if (snapshot) {
        m_snapshot.store(std::move(snapshot));
    }
}

std::shared_ptr<const InputSnapshot> InputRouter::snapshot() const
{
    return m_snapshot.load();
}

void InputRouter::setTouchpadTick(DWORD tick)
{
    m_touchpadTick.store(tick);
}

std::chrono::microseconds InputRouter::timeOf(const MouseEvent &event)
{
    return std::chrono::milliseconds(event.time);
}

void InputRouter::send(InputCommand command)
{
    if (m_sink) {
        m_sink(command);
    }
}

int InputRouter::monitorAt(const InputSnapshot &snapshot, POINT point) const
{
    for (int i = 0; i < snapshot.monitors.size(); ++i) {
        if (contains(snapshot.monitors[i], point)) {
            return i;
        }
    }
    return -1;
}

bool InputRouter::lockedAt(const InputSnapshot &snapshot, POINT point) const
{
    const int index = monitorAt(snapshot, point);
    return index >= 0 && index < snapshot.locked.size() && snapshot.locked[index];
}

HookAction InputRouter::mouse(const MouseEvent &event, unsigned modifiers)
{
    const std::shared_ptr<const InputSnapshot> snapshot = m_snapshot.load();
    const POINT point = event.position;

    switch (event.message) {
    case WM_MOUSEMOVE: {
        if (snapshot->userMove) {
            return userMoveMotion(event, *snapshot);
        }
        m_handValid = false;
        if (m_state == State::Pending) {
            if (std::abs(point.x - m_pressPoint.x) < DragThreshold && std::abs(point.y - m_pressPoint.y) < DragThreshold) {
                return HookAction::Pass;
            }
            // The desktop has the press: end it before it draws a selection rectangle.
            if (m_releaseLeft) {
                m_releaseLeft();
            }
            m_state = State::Panning;
            m_panButtonUp = WM_LBUTTONUP;
            m_swallowLeftUp = true;
            send(make(InputCommand::Kind::PanBegin, QPointF(m_pressPoint.x, m_pressPoint.y)));
            InputCommand delta = make(InputCommand::Kind::PanDelta, QPointF(point.x, point.y));
            delta.delta = QPointF(point.x - m_pressPoint.x, point.y - m_pressPoint.y);
            delta.time = timeOf(event);
            send(delta);
            m_lastPoint = point;
            // This one move would still reach the desktop with the button held.
            return HookAction::Swallow;
        }
        if (m_state == State::Panning) {
            InputCommand delta = make(InputCommand::Kind::PanDelta, QPointF(point.x, point.y));
            delta.delta = QPointF(point.x - m_lastPoint.x, point.y - m_lastPoint.y);
            delta.time = timeOf(event);
            m_lastPoint = point;
            if (!delta.delta.isNull()) {
                send(delta);
            }
        }
        return HookAction::Pass;
    }
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        const bool left = event.message == WM_LBUTTONDOWN;
        if (snapshot->gliding && m_state == State::Idle) {
            send(make(InputCommand::Kind::StopMotion));
            (left ? m_swallowLeftUp : m_swallowMiddleUp) = true;
            return HookAction::Swallow;
        }
        if (!snapshot->enabled || m_state != State::Idle || lockedAt(*snapshot, point)) {
            return HookAction::Pass;
        }
        if (left && hasAll(modifiers, snapshot->panModifiers)) {
            m_state = State::Panning;
            m_panButtonUp = WM_LBUTTONUP;
            m_swallowLeftUp = true;
            m_lastPoint = point;
            send(make(InputCommand::Kind::PanBegin, QPointF(point.x, point.y)));
            return HookAction::Swallow;
        }
        const bool wantDesktop = left ? snapshot->dragDesktopToPan : snapshot->middleButtonPan;
        if (!wantDesktop || !m_desktopTest || !m_desktopTest(point)) {
            return HookAction::Pass;
        }
        if (left) {
            m_state = State::Pending;
            m_pressPoint = point;
            return HookAction::Pass;
        }
        // The desktop does nothing with the middle button: pan right away.
        m_state = State::Panning;
        m_panButtonUp = WM_MBUTTONUP;
        m_swallowMiddleUp = true;
        m_lastPoint = point;
        send(make(InputCommand::Kind::PanBegin, QPointF(point.x, point.y)));
        return HookAction::Swallow;
    }
    case WM_RBUTTONDOWN:
        if (snapshot->gliding && m_state == State::Idle) {
            send(make(InputCommand::Kind::StopMotion));
            m_swallowRightUp = true;
            return HookAction::Swallow;
        }
        return HookAction::Pass;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP: {
        if (snapshot->userMove && event.message == WM_LBUTTONUP && !m_edge.isNull()) {
            m_edge = QPointF();
            send(make(InputCommand::Kind::EdgeChanged));
        }
        if (m_state == State::Pending && event.message == WM_LBUTTONUP) {
            m_state = State::Idle;
            return HookAction::Pass;
        }
        if (m_state == State::Panning && event.message == m_panButtonUp) {
            m_state = State::Idle;
            InputCommand end = make(InputCommand::Kind::PanEnd);
            end.time = timeOf(event);
            send(end);
        }
        bool &swallow = event.message == WM_LBUTTONUP ? m_swallowLeftUp : event.message == WM_MBUTTONUP ? m_swallowMiddleUp : m_swallowRightUp;
        if (std::exchange(swallow, false)) {
            return HookAction::Swallow;
        }
        return HookAction::Pass;
    }
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        return wheel(event, modifiers, *snapshot);
    default:
        return HookAction::Pass;
    }
}

HookAction InputRouter::wheel(const MouseEvent &event, unsigned modifiers, const InputSnapshot &snapshot)
{
    if (!snapshot.enabled || event.wheelDelta == 0 || lockedAt(snapshot, event.position)) {
        return HookAction::Pass;
    }
    const bool horizontal = event.message == WM_MOUSEHWHEEL;
    const QPointF point(event.position.x, event.position.y);
    const qreal notches = event.wheelDelta / 120.0;
    if (!horizontal && hasAll(modifiers, snapshot.zoomModifiers)) {
        InputCommand zoom = make(InputCommand::Kind::Zoom, point);
        zoom.steps = notches;
        send(zoom);
        return HookAction::Swallow;
    }
    if (!m_desktopTest || !m_desktopTest(event.position)) {
        return HookAction::Pass;
    }
    const bool injected = (event.flags & LLMHF_INJECTED) != 0;
    const bool control = (modifiers & MOD_CONTROL) != 0;
    const bool shift = (modifiers & MOD_SHIFT) != 0;
    const DWORD tick = m_touchpadTick.load();
    const bool fingers = tick != 0 && event.time - tick <= TouchpadWindowMs + 10;

    if (!horizontal && injected && control) {
        // Pinch on a precision touchpad arrives as injected Ctrl + wheel.
        if (!snapshot.pinchZoom) {
            return HookAction::Pass;
        }
        InputCommand zoom = make(InputCommand::Kind::Zoom, point);
        zoom.steps = notches;
        send(zoom);
        return HookAction::Swallow;
    }
    if (fingers && !control) {
        if (!snapshot.touchpadScrollPans) {
            return HookAction::Pass;
        }
        InputCommand pan = make(InputCommand::Kind::ScrollPan, point);
        // Like scrolling a document: wheel up shows what is above, right shows what is right.
        pan.delta = horizontal ? QPointF(-event.wheelDelta, 0) : QPointF(shift ? event.wheelDelta : 0, shift ? 0 : event.wheelDelta);
        send(pan);
        return HookAction::Swallow;
    }
    if (snapshot.shiftWheelPans && (shift || control || horizontal)) {
        InputCommand pan = make(InputCommand::Kind::ScrollPan, point);
        // One notch moves by a fixed amount; the main thread scales it.
        if (horizontal) {
            pan.delta = QPointF(-notches * 120, 0);
        } else if (shift) {
            pan.delta = QPointF(notches * 120, 0);
        } else {
            pan.delta = QPointF(0, notches * 120);
        }
        send(pan);
        return HookAction::Swallow;
    }
    if (!horizontal && !shift && !control && snapshot.wheelZoomOnDesktop) {
        InputCommand zoom = make(InputCommand::Kind::Zoom, point);
        zoom.steps = notches;
        send(zoom);
        return HookAction::Swallow;
    }
    return HookAction::Pass;
}

HookAction InputRouter::userMoveMotion(const MouseEvent &event, const InputSnapshot &snapshot)
{
    const POINT point = event.position;
    if (!m_handValid || m_userMoveMonitor != snapshot.userMoveMonitor) {
        // A new drag: the hand starts where the pointer is.
        m_hand.reset();
        m_handPosition = QPointF(point.x, point.y);
        m_lastPassed = point;
        m_handValid = true;
        m_userMoveMonitor = snapshot.userMoveMonitor;
        m_edge = QPointF();
    } else {
        // Swallowed moves leave the pointer where it was; the hand keeps going.
        m_handPosition += QPointF(point.x - m_lastPassed.x, point.y - m_lastPassed.y);
    }
    m_hand.addSample(m_handPosition, timeOf(event));

    const int home = m_userMoveMonitor;
    const int target = monitorAt(snapshot, point);
    bool swallow = false;
    if (home >= 0 && target >= 0 && target != home) {
        const qreal speed = m_hand.speed(timeOf(event));
        if (snapshot.flingToOtherScreen && speed >= snapshot.flingThreshold) {
            m_userMoveMonitor = target;
            InputCommand crossed = make(InputCommand::Kind::MonitorCrossed);
            crossed.monitor = target;
            send(crossed);
        } else {
            swallow = true;
        }
    }
    if (!swallow) {
        m_lastPassed = point;
    }

    // Edge scrolling: the direction towards the edges the pointer touches.
    QPointF edge;
    if (snapshot.edgePan && m_userMoveMonitor >= 0 && m_userMoveMonitor < snapshot.monitors.size()) {
        const RECT &rect = snapshot.monitors[m_userMoveMonitor];
        const POINT at = swallow ? m_lastPassed : point;
        const int margin = snapshot.edgePanMargin;
        if (at.x < rect.left + margin) {
            edge.setX(-1);
        } else if (at.x >= rect.right - margin) {
            edge.setX(1);
        }
        if (at.y < rect.top + margin) {
            edge.setY(-1);
        } else if (at.y >= rect.bottom - margin) {
            edge.setY(1);
        }
    }
    if (edge != m_edge) {
        m_edge = edge;
        InputCommand changed = make(InputCommand::Kind::EdgeChanged);
        changed.delta = edge;
        send(changed);
    }
    return swallow ? HookAction::Swallow : HookAction::Pass;
}

HookAction InputRouter::key(const KeyEvent &event, unsigned modifiers)
{
    const bool down = event.message == WM_KEYDOWN || event.message == WM_SYSKEYDOWN;
    const unsigned vk = event.vk & 0xff;
    if (!down) {
        if (m_swallowedKeys.test(vk)) {
            m_swallowedKeys.reset(vk);
            return HookAction::Swallow;
        }
        return HookAction::Pass;
    }
    if (isModifierKey(vk)) {
        return HookAction::Pass;
    }
    const std::shared_ptr<const InputSnapshot> snapshot = m_snapshot.load();
    if (!snapshot->shortcutsEnabled) {
        return HookAction::Pass;
    }
    const std::optional<ShortcutAction> action = snapshot->shortcuts.actionFor(KeyCombo{modifiers, vk});
    if (!action) {
        return HookAction::Pass;
    }
    InputCommand command = make(InputCommand::Kind::Shortcut);
    command.action = *action;
    command.repeat = m_swallowedKeys.test(vk);
    m_swallowedKeys.set(vk);
    send(command);
    return HookAction::Swallow;
}

} // namespace Wid
