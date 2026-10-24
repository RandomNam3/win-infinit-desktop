/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "markers.h"
#include "layered.h"

#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace Wid
{

namespace
{
const wchar_t *const s_windowClass = L"WinInfinitDesktopMarker";
constexpr int MaxBubbles = 64;
}

Markers::Markers(ClickHandler handler)
    : m_handler(std::move(handler))
{
}

Markers::~Markers()
{
    for (const auto &bubble : m_bubbles) {
        detachWindowObject(bubble->window.get());
    }
}

bool Markers::render(Bubble &bubble, const Marker &marker)
{
    const QPointF center(Extent / 2.0, Extent / 2.0);
    const qreal radius = Size / 2.0;
    QImage image(Extent, Extent, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF normal(-marker.direction.y(), marker.direction.x());
    QPainterPath arrow;
    arrow.moveTo(center + marker.direction * (radius + 9));
    arrow.lineTo(center + marker.direction * (radius - 1) + normal * 7);
    arrow.lineTo(center + marker.direction * (radius - 1) - normal * 7);
    arrow.closeSubpath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 22, 26, 225));
    painter.drawPath(arrow);
    painter.drawEllipse(center, radius, radius);
    painter.setPen(QPen(QColor(255, 255, 255, 100), 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius - 1, radius - 1);
    if (!marker.icon.isNull()) {
        const QSizeF size = marker.icon.size();
        painter.drawImage(QRectF(center - QPointF(size.width() / 2, size.height() / 2), size), marker.icon);
    }
    painter.end();
    const POINT topLeft = {LONG(std::lround(marker.position.x() - Extent / 2.0)), LONG(std::lround(marker.position.y() - Extent / 2.0))};
    return updateLayeredWindow(bubble.window.get(), image, topLeft);
}

void Markers::show(const QList<Marker> &markers)
{
    const int count = std::min<int>(markers.size(), MaxBubbles);
    for (int i = 0; i < count; ++i) {
        if (i >= int(m_bubbles.size())) {
            auto bubble = std::make_unique<Bubble>();
            bubble->owner = this;
            registerWindowClass(s_windowClass, &Markers::windowProc);
            bubble->window.reset(CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, s_windowClass, L"", WS_POPUP, 0, 0,
                                                 Extent, Extent, nullptr, nullptr, GetModuleHandleW(nullptr), bubble.get()));
            if (!bubble->window) {
                break;
            }
            SetWindowDisplayAffinity(bubble->window.get(), WDA_EXCLUDEFROMCAPTURE);
            m_bubbles.push_back(std::move(bubble));
        }
        Bubble &bubble = *m_bubbles[std::size_t(i)];
        bubble.target = markers[i].window;
        bubble.direction = markers[i].direction;
        if (render(bubble, markers[i]) && !bubble.shown) {
            SetWindowPos(bubble.window.get(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            bubble.shown = true;
        }
    }
    for (std::size_t i = std::size_t(std::max(count, 0)); i < m_bubbles.size(); ++i) {
        if (m_bubbles[i]->shown) {
            ShowWindow(m_bubbles[i]->window.get(), SW_HIDE);
            m_bubbles[i]->shown = false;
            m_bubbles[i]->target = nullptr;
        }
    }
    m_visible = count;
}

void Markers::hide()
{
    for (const auto &bubble : m_bubbles) {
        if (bubble->shown) {
            ShowWindow(bubble->window.get(), SW_HIDE);
            bubble->shown = false;
        }
        bubble->target = nullptr;
    }
    m_visible = 0;
}

LRESULT CALLBACK Markers::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Bubble *bubble = windowObject<Bubble>(hwnd, message, lParam);
    if (!bubble || !bubble->window) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    switch (message) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_LBUTTONDOWN:
        if (bubble->target && bubble->owner->m_handler) {
            // Copy first: the handler may hide or reuse this bubble.
            const HWND target = bubble->target;
            const ClickHandler handler = bubble->owner->m_handler;
            handler(target);
        }
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

} // namespace Wid
