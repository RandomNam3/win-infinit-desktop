/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "placementrules.h"

#include <cstdlib>

namespace Wid
{

MoveRequests::Request &MoveRequests::at(std::size_t i)
{
    return m_requests[(m_head + Capacity - m_count + i) % Capacity];
}

const MoveRequests::Request &MoveRequests::at(std::size_t i) const
{
    return m_requests[(m_head + Capacity - m_count + i) % Capacity];
}

void MoveRequests::add(POINT position, POINT windowPosition, ULONGLONG tick)
{
    m_requests[m_head] = Request{position, windowPosition, tick, false};
    m_head = (m_head + 1) % Capacity;
    if (m_count < Capacity) {
        ++m_count;
    }
}

bool MoveRequests::answer(POINT position, POINT windowPosition, ULONGLONG now)
{
    const auto same = [](POINT a, POINT b) {
        return std::abs(a.x - b.x) <= 1 && std::abs(a.y - b.y) <= 1;
    };
    // The newest match: a program may pass the same place twice.
    for (std::size_t i = m_count; i-- > 0;) {
        const Request &request = at(i);
        // The window rect is what SetWindowPos got; the frame may differ while DWM catches up.
        if (now - request.tick <= Lifetime && (same(request.position, position) || same(request.windowPosition, windowPosition))) {
            for (std::size_t k = 0; k <= i; ++k) {
                at(k).answered = true;
            }
            return true;
        }
    }
    return false;
}

bool MoveRequests::busy(ULONGLONG now) const
{
    int inFlight = 0;
    for (std::size_t i = 0; i < m_count; ++i) {
        const Request &request = at(i);
        if (!request.answered && now - request.tick < InFlight) {
            ++inFlight;
        }
    }
    return inFlight >= MaxInFlight;
}

bool reachesOtherScreen(const QRectF &rect, const QList<QRectF> &screens, int own)
{
    for (int i = 0; i < screens.size(); ++i) {
        // Touching edges do not count: QRectF::intersects needs a common area.
        if (i != own && rect.intersects(screens[i])) {
            return true;
        }
    }
    return false;
}

int screenAfterForeignMove(const QRectF &frame, const QList<QRectF> &screens, int own)
{
    if (own >= 0 && own < screens.size() && frame.intersects(screens[own])) {
        return own;
    }
    int best = -1;
    qreal bestArea = 0;
    for (int i = 0; i < screens.size(); ++i) {
        const QRectF common = frame.intersected(screens[i]);
        const qreal area = common.width() * common.height();
        if (area > bestArea) {
            best = i;
            bestArea = area;
        }
    }
    return best;
}

} // namespace Wid
