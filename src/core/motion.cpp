/*
    KDE Infinite Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "motion.h"

#include <algorithm>
#include <cmath>

namespace InfiniteDesktop
{

qreal easeOutCubic(qreal t)
{
    t = std::clamp(t, 0.0, 1.0);
    const qreal inverse = 1.0 - t;
    return 1.0 - inverse * inverse * inverse;
}

qreal easeInOutCubic(qreal t)
{
    t = std::clamp(t, 0.0, 1.0);
    if (t < 0.5) {
        return 4.0 * t * t * t;
    }
    const qreal f = -2.0 * t + 2.0;
    return 1.0 - f * f * f / 2.0;
}

void CameraMotion::startTransition(const Camera &from, const Camera &to, std::chrono::milliseconds duration, TimePoint now)
{
    m_from = from;
    m_to = to;
    m_start = now;
    m_duration = std::max(duration, std::chrono::milliseconds(0));
    m_kind = Kind::Transition;
}

bool CameraMotion::startGlide(const Camera &from, const QPointF &velocity, qreal friction, TimePoint now)
{
    const qreal speed = std::hypot(velocity.x(), velocity.y());
    if (!std::isfinite(speed) || speed < MinimumGlideSpeed * 2.0) {
        return false;
    }
    m_friction = std::max(friction, 0.1);
    m_from = from;
    m_velocity = velocity;
    m_start = now;
    // Solve |v| * exp(-k t) = MinimumGlideSpeed for t.
    m_glideDuration = std::log(speed / MinimumGlideSpeed) / m_friction;
    // The resting point is reached for t -> infinity: from + v / k.
    m_to = Camera{from.center + velocity / m_friction, from.zoom};
    m_kind = Kind::Glide;
    return true;
}

void CameraMotion::stop()
{
    m_kind = Kind::None;
}

qreal CameraMotion::progress(TimePoint now) const
{
    if (m_duration.count() <= 0) {
        return 1.0;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - m_start);
    return std::clamp(qreal(elapsed.count()) / qreal(std::chrono::duration_cast<std::chrono::microseconds>(m_duration).count()), 0.0, 1.0);
}

Camera CameraMotion::sample(TimePoint now) const
{
    switch (m_kind) {
    case Kind::None:
        return m_to;
    case Kind::Transition:
        return interpolate(m_from, m_to, easeOutCubic(progress(now)));
    case Kind::Glide: {
        const qreal seconds = std::min(std::chrono::duration<qreal>(now - m_start).count(), m_glideDuration);
        if (seconds <= 0.0) {
            return m_from;
        }
        // Integral of v * exp(-k t) dt from 0 to t.
        const qreal factor = (1.0 - std::exp(-m_friction * seconds)) / m_friction;
        return Camera{m_from.center + m_velocity * factor, m_from.zoom};
    }
    }
    return m_to;
}

bool CameraMotion::isFinished(TimePoint now) const
{
    switch (m_kind) {
    case Kind::None:
        return true;
    case Kind::Transition:
        return progress(now) >= 1.0;
    case Kind::Glide:
        return std::chrono::duration<qreal>(now - m_start).count() >= m_glideDuration;
    }
    return true;
}

Camera CameraMotion::target() const
{
    if (m_kind == Kind::Glide) {
        const qreal factor = (1.0 - std::exp(-m_friction * m_glideDuration)) / m_friction;
        return Camera{m_from.center + m_velocity * factor, m_from.zoom};
    }
    return m_to;
}

} // namespace InfiniteDesktop
