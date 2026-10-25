/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QList>
#include <QRectF>

#include <windows.h>

#include <array>
#include <cstddef>

namespace Wid
{

/**
 * The service's recent moves of one window, to tell them from moves of the program or of
 * Windows, and to keep a program that falls behind from piling them up.
 *
 * Moves are asynchronous (SWP_ASYNCWINDOWPOS): the program carries them out in order, when it
 * gets to it, and each one is reported then. A reported position that was requested counts
 * as answered, together with all older requests. A request is in flight until it is answered
 * or half a second old; with three in flight the program is busy, and the service holds the
 * next position back until it answers (only the newest position matters).
 */
class MoveRequests
{
public:
    static constexpr std::size_t Capacity = 32;
    static constexpr ULONGLONG Lifetime = 5000; ///< ms a request is recognised in a report
    static constexpr ULONGLONG InFlight = 500; ///< ms an unanswered request keeps the program busy
    static constexpr int MaxInFlight = 3;

    /// `position`: the frame's top left; `windowPosition`: the window rect's, as SetWindowPos got it.
    void add(POINT position, POINT windowPosition, ULONGLONG tick);
    /// A reported position (±1 px, frame or window rect): true if it was requested in the last
    /// 5 s. It and all older requests count as answered.
    bool answer(POINT position, POINT windowPosition, ULONGLONG now);
    /// Whether a further request would only queue up behind unanswered ones.
    bool busy(ULONGLONG now) const;

private:
    struct Request
    {
        POINT position;
        POINT windowPosition;
        ULONGLONG tick;
        bool answered;
    };
    /// i = 0 is the oldest.
    Request &at(std::size_t i);
    const Request &at(std::size_t i) const;

    std::array<Request, Capacity> m_requests{};
    std::size_t m_head = 0; ///< where the next one goes
    std::size_t m_count = 0;
};

/**
 * Whether a window rect reaches into another screen than `own`. Windows cannot cut a window
 * off at a screen edge, so such a window shows on the neighbour.
 */
bool reachesOtherScreen(const QRectF &rect, const QList<QRectF> &screens, int own);

/**
 * The screen a window belongs to after the program or Windows moved it: its own while it still
 * reaches into it (a program that adapts to the scaling of a neighbour moves itself a little),
 * else the one with the largest part of it; -1 if it is on none.
 */
int screenAfterForeignMove(const QRectF &frame, const QList<QRectF> &screens, int own);

} // namespace Wid
