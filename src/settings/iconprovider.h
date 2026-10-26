/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QQuickImageProvider>

namespace InfiniteDesktop
{

/**
 * Renders the line icons shipped in the resources (":/infinitedesktop/icons")
 * in any color: "image://idicon/<name>/<color without #>".
 *
 * The SVGs use "currentColor", which is replaced before rendering at the
 * requested (device pixel) size, so icons stay crisp at every scale factor.
 */
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

/// Application icons from the shell: "image://themeicon/shell-<hex parsing name>" or "file-<hex path>".
class ThemeIconProvider : public QQuickImageProvider
{
public:
    ThemeIconProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

} // namespace InfiniteDesktop
