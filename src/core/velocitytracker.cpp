/*
    KDE Infinit Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "velocitytracker.h"

#include <cmath>
#include <numbers>

namespace InfinitDesktop
{

void VelocityTracker::reset()
{
    m_head = 0;
    m_count = 0;
}

void VelocityTracker::addSample(const QPointF &position, Time time)
{
    if (m_count > 0) {
        const Sample &last = m_samples[(m_head + Capacity - 1) % Capacity];
        if (time < last.time) {
            // Time went backwards (different input device clocks); start over.
            reset();
        }
    }
    m_samples[m_head] = Sample{position, time};
    m_head = (m_head + 1) % Capacity;
    if (m_count < Capacity) {
        ++m_count;
    }
}

QPointF VelocityTracker::velocity(Time now, Time window, Time restTimeout) const
{
    if (m_count < 2) {
        return QPointF();
    }
    const Sample &newest = m_samples[(m_head + Capacity - 1) % Capacity];
    if (now - newest.time > restTimeout) {
        return QPointF();
    }

    // Least squares fit of position over time for the samples inside the window.
    // Much more robust against jittery input than a two-point difference.
    double sumT = 0, sumX = 0, sumY = 0, sumTT = 0, sumTX = 0, sumTY = 0;
    int n = 0;
    for (std::size_t i = 0; i < m_count; ++i) {
        const Sample &sample = m_samples[(m_head + Capacity - 1 - i) % Capacity];
        if (newest.time - sample.time > window) {
            break;
        }
        const double t = std::chrono::duration<double>(sample.time - newest.time).count();
        sumT += t;
        sumX += sample.position.x();
        sumY += sample.position.y();
        sumTT += t * t;
        sumTX += t * sample.position.x();
        sumTY += t * sample.position.y();
        ++n;
    }
    if (n < 2) {
        return QPointF();
    }
    const double denominator = n * sumTT - sumT * sumT;
    if (std::abs(denominator) < 1e-12) {
        return QPointF();
    }
    const double vx = (n * sumTX - sumT * sumX) / denominator;
    const double vy = (n * sumTY - sumT * sumY) / denominator;
    if (!std::isfinite(vx) || !std::isfinite(vy)) {
        return QPointF();
    }
    return QPointF(vx, vy);
}

qreal VelocityTracker::speed(Time now) const
{
    const QPointF v = velocity(now);
    return std::hypot(v.x(), v.y());
}

QPointF VelocityTracker::flingVelocity(Time now, qreal minimumSpeed) const
{
    using namespace std::chrono_literals;
    const QPointF v = velocity(now);
    const qreal speed = std::hypot(v.x(), v.y());
    if (speed < minimumSpeed) {
        return QPointF();
    }
    const Sample &newest = m_samples[(m_head + Capacity - 1) % Capacity];
    const auto sampleAgo = [&](std::size_t i) -> const Sample & {
        return m_samples[(m_head + Capacity - 1 - i) % Capacity];
    };
    // Position at the newest sample not newer than "age" before the last one.
    const auto positionAt = [&](Time age, bool *found) {
        for (std::size_t i = 0; i < m_count; ++i) {
            if (newest.time - sampleAgo(i).time >= age) {
                *found = true;
                return sampleAgo(i).position;
            }
        }
        *found = false;
        return sampleAgo(m_count - 1).position;
    };
    const auto length = [](const QPointF &p) {
        return std::hypot(p.x(), p.y());
    };

    // Straight: over the last 150 ms the pointer covered most of its path as distance.
    qreal path = 0;
    QPointF oldest = newest.position;
    for (std::size_t i = 1; i < m_count && newest.time - sampleAgo(i).time <= 150ms; ++i) {
        path += length(sampleAgo(i - 1).position - sampleAgo(i).position);
        oldest = sampleAgo(i).position;
    }
    const QPointF displacement = newest.position - oldest;
    if (path < 1.0 || length(displacement) < 0.8 * path) {
        return QPointF();
    }
    // Same direction as the measured velocity.
    if (QPointF::dotProduct(displacement, v) < std::cos(35.0 * std::numbers::pi / 180.0) * length(displacement) * speed) {
        return QPointF();
    }
    // Not turning: the last 100 ms point the same way as the 150 ms before them
    // (a circle at one turn per second turns by about 45 degrees in between).
    bool foundMiddle = false;
    bool foundStart = false;
    const QPointF middle = positionAt(100ms, &foundMiddle);
    const QPointF start = positionAt(250ms, &foundStart);
    if (foundMiddle && foundStart) {
        const QPointF late = newest.position - middle;
        const QPointF early = middle - start;
        if (length(early) > 2.0 && length(late) > 2.0
            && QPointF::dotProduct(early, late) < std::cos(30.0 * std::numbers::pi / 180.0) * length(early) * length(late)) {
            return QPointF();
        }
    }
    return v;
}

} // namespace InfinitDesktop
