/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "shellitem_win.h"

#include <QDir>

#include <algorithm>
#include <cstring>

namespace InfiniteDesktop
{

namespace
{

/// A 32 bit DIB section as the shell returns it (premultiplied alpha, or none at all).
QImage imageFromBitmap(HBITMAP bitmap)
{
    DIBSECTION section = {};
    if (GetObjectW(bitmap, sizeof(section), &section) != sizeof(section) || section.dsBm.bmBitsPixel != 32 || !section.dsBm.bmBits) {
        return QImage();
    }
    const int width = section.dsBm.bmWidth;
    const int height = section.dsBm.bmHeight;
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        return QImage();
    }
    const bool topDown = section.dsBmih.biHeight < 0;
    const auto *bits = static_cast<const uchar *>(section.dsBm.bmBits);
    const int stride = section.dsBm.bmWidthBytes;
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        return QImage();
    }
    bool anyAlpha = false;
    for (int y = 0; y < height; ++y) {
        const uchar *source = bits + qsizetype(topDown ? y : height - 1 - y) * stride;
        std::memcpy(image.scanLine(y), source, size_t(width) * 4);
        const auto *pixels = reinterpret_cast<const quint32 *>(source);
        for (int x = 0; !anyAlpha && x < width; ++x) {
            anyAlpha = (pixels[x] >> 24) != 0;
        }
    }
    if (!anyAlpha) {
        // Old style icons carry no alpha at all: they are opaque.
        image = image.convertToFormat(QImage::Format_RGB32).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    return image;
}

} // namespace

QString displayName(IShellItem *item, SIGDN form)
{
    PWSTR name = nullptr;
    if (!item || FAILED(item->GetDisplayName(form, &name)) || !name) {
        return QString();
    }
    const QString result = QString::fromWCharArray(name);
    CoTaskMemFree(name);
    return result;
}

QImage shellItemIcon(const QString &parsingName, bool appsFolder, QSize size)
{
    if (parsingName.isEmpty() || size.isEmpty()) {
        return QImage();
    }
    const QString path = appsFolder ? QStringLiteral("shell:AppsFolder\\") + parsingName : QDir::toNativeSeparators(parsingName);
    ComPtr<IShellItemImageFactory> factory;
    if (FAILED(SHCreateItemFromParsingName(reinterpret_cast<PCWSTR>(path.utf16()), nullptr, IID_PPV_ARGS(factory.put())))) {
        return QImage();
    }
    HBITMAP bitmap = nullptr;
    const SIZE wanted = {std::min(size.width(), 256), std::min(size.height(), 256)};
    if (FAILED(factory->GetImage(wanted, SIIGBF_ICONONLY | SIIGBF_RESIZETOFIT, &bitmap)) || !bitmap) {
        return QImage();
    }
    QImage image = imageFromBitmap(bitmap);
    DeleteObject(bitmap);
    return image;
}

} // namespace InfiniteDesktop
