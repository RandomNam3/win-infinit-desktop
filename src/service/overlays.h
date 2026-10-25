/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "win32util.h"

#include <QImage>
#include <QRect>

#include <functional>

namespace Wid
{

/**
 * A topmost window of the service that is never activated (the keyboard stays with the
 * application): the veil and the rectangle of a selection, the map panel, messages.
 *
 * - Veil: one color, one opacity for the whole window (DWM blends it, nothing is drawn
 *   per frame); gets the mouse.
 * - Tint: like the veil with a border, lets the mouse through (the selection rectangle).
 * - Panel: opaque, shows an image; gets the mouse.
 * - Label: an image with per-pixel alpha, lets the mouse through (messages).
 *
 * Owners never destroy one from inside its handler: they hide it. Hidden windows keep no
 * image.
 */
class OverlayWindow
{
public:
    enum class Style {
        Veil,
        Tint,
        Panel,
        Label,
    };
    /// Every message goes here first; true = handled (the window procedure returns 0).
    using Handler = std::function<bool(UINT message, WPARAM wParam, LPARAM lParam)>;

    explicit OverlayWindow(Style style, Handler handler = {});
    ~OverlayWindow();
    OverlayWindow(const OverlayWindow &) = delete;
    OverlayWindow &operator=(const OverlayWindow &) = delete;

    /// Shows the window over `rect` (screen pixels), above all other windows.
    void show(const QRect &rect);
    void hide();
    /// Puts a shown window back on top (after another topmost window, the overview).
    void raise();
    bool isVisible() const
    {
        return m_visible;
    }
    HWND window() const
    {
        return m_window.get();
    }
    QRect rect() const
    {
        return m_rect;
    }

    /// Veil and tint: fill and border color, opacity of the whole window.
    void setColor(COLORREF fill, COLORREF border, BYTE alpha);
    /// Panel: the content (size of the window). Label: the content, shown at the window's
    /// top left (the window takes the image's size).
    void setImage(const QImage &image);

    /// Screen point of a mouse message's client coordinates.
    QPoint screenPoint(LPARAM lParam) const;

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT message, WPARAM wParam, LPARAM lParam);
    bool create();
    void paint();

    Style m_style;
    Handler m_handler;
    UniqueWindow m_window;
    QRect m_rect;
    bool m_visible = false;
    COLORREF m_fill = RGB(0, 0, 0);
    COLORREF m_border = RGB(0, 0, 0);
    BYTE m_alpha = 255;
    QImage m_image;
};

} // namespace Wid
