/*
    KDE Infinite Desktop - infinite canvas for KWin
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QPointF>

#include <array>
#include <chrono>
#include <cstddef>

namespace InfiniteDesktop
{

/**
 * Estimates the velocity of a pointer (or of an accumulated gesture position)
 * from its most recent samples.
 *
 * Used for kinetic gliding after a pan and for detecting a "fling" of a dragged
 * window against the screen edge.
 */
class VelocityTracker
{
public:
    using Time = std::chrono::microseconds;

    void reset();
    void addSample(const QPointF &position, Time time);

    /**
     * Velocity in units per second at time @p now, computed over the samples of
     * the last @p window. Returns a null vector if the pointer has been resting
     * for longer than @p restTimeout before @p now.
     */
    QPointF velocity(Time now, Time window = std::chrono::milliseconds(90), Time restTimeout = std::chrono::milliseconds(60)) const;

    /// Speed (length of velocity()).
    qreal speed(Time now) const;

    /**
     * Velocity for a kinetic glide after releasing at @p now: like velocity(), but null
     * unless the pointer ended with a flick, i.e. fast (at least @p minimumSpeed) and in a
     * fairly straight line. Circling or wiggling before releasing must not send the canvas
     * flying in whatever direction the pointer happened to move last.
     */
    QPointF flingVelocity(Time now, qreal minimumSpeed = 250.0) const;

    bool isEmpty() const
    {
        return m_count == 0;
    }

private:
    struct Sample
    {
        QPointF position;
        Time time;
    };
    /// Enough for a 1000 Hz mouse over the 250 ms the fling check looks at.
    static constexpr std::size_t Capacity = 256;
    std::array<Sample, Capacity> m_samples{};
    std::size_t m_head = 0; // index of the next write
    std::size_t m_count = 0;
};

} // namespace InfiniteDesktop
