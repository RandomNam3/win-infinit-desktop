/*
    KDE Infinit Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "camera.h"

#include <chrono>

namespace InfinitDesktop
{

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

/**
 * Time based camera motion.
 *
 * All motions are evaluated as a pure function of absolute time, so it does
 * not matter how often or from how many outputs/views the motion is sampled.
 * There are two kinds of motion:
 *  - a transition: eased interpolation from one camera to another,
 *  - a glide: the camera keeps moving with an initial velocity that decays
 *    exponentially (friction), like kinetic scrolling.
 */
class CameraMotion
{
public:
    enum class Kind {
        None,
        Transition,
        Glide,
    };

    Kind kind() const
    {
        return m_kind;
    }
    bool isActive() const
    {
        return m_kind != Kind::None;
    }

    /// Starts an eased transition. A zero duration ends it immediately at @p to.
    void startTransition(const Camera &from, const Camera &to, std::chrono::milliseconds duration, TimePoint now);

    /**
     * Starts a glide from @p from with @p velocity (canvas units per second).
     * @p friction is the exponential decay rate in 1/s.
     * Returns false (and does nothing) when the velocity is too small to matter.
     */
    bool startGlide(const Camera &from, const QPointF &velocity, qreal friction, TimePoint now);

    void stop();

    /// Camera at time @p now. Only valid while isActive().
    Camera sample(TimePoint now) const;

    /// True when the motion has reached its end at time @p now.
    bool isFinished(TimePoint now) const;

    /// The final camera of the motion (target of a transition, resting point of a glide).
    Camera target() const;

    /// Velocities below this value (canvas px/s) end a glide.
    static constexpr qreal MinimumGlideSpeed = 15.0;

private:
    qreal progress(TimePoint now) const;

    Kind m_kind = Kind::None;
    Camera m_from;
    Camera m_to;
    TimePoint m_start;
    std::chrono::milliseconds m_duration{0};
    QPointF m_velocity;
    qreal m_friction = 5.0;
    qreal m_glideDuration = 0.0; // seconds
};

/// Ease-out cubic, the curve used for camera transitions.
qreal easeOutCubic(qreal t);
/// Ease-in-out cubic.
qreal easeInOutCubic(qreal t);

} // namespace InfinitDesktop
