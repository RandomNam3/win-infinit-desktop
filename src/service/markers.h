/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "win32util.h"

#include <QImage>
#include <QList>
#include <QPointF>

#include <functional>
#include <memory>
#include <vector>

namespace Wid
{

/**
 * The markers for windows outside the view: one small layered, topmost window per marker
 * that never takes the focus (a circle with the program's icon and an arrow towards the
 * window). Small windows keep the cost of moving markers with the canvas low: per frame
 * only a 64 × 64 image is drawn and positioned. Transparent pixels let the mouse through;
 * a click calls the handler with the marked window.
 */
class Markers
{
public:
    static constexpr int Size = 44; ///< circle diameter
    static constexpr int Extent = 64; ///< window size, with room for the arrow

    struct Marker
    {
        HWND window = nullptr;
        QPointF position; ///< centre, screen coordinates
        QPointF direction;
        QImage icon;
    };
    using ClickHandler = std::function<void(HWND)>;

    explicit Markers(ClickHandler handler);
    ~Markers();
    Markers(const Markers &) = delete;
    Markers &operator=(const Markers &) = delete;

    void show(const QList<Marker> &markers);
    void hide();
    /// Puts the shown markers back on top (after another topmost window, the overview).
    void raise();
    bool isVisible() const
    {
        return m_visible > 0;
    }

private:
    struct Bubble
    {
        Markers *owner = nullptr;
        UniqueWindow window;
        HWND target = nullptr;
        QPointF direction;
        bool shown = false;
    };
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    bool render(Bubble &bubble, const Marker &marker);

    ClickHandler m_handler;
    std::vector<std::unique_ptr<Bubble>> m_bubbles;
    int m_visible = 0;
};

} // namespace Wid
