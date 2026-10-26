/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "layered.h"
#include "win32util.h"

#include <QImage>

#include <shellapi.h>

#include <cstring>

namespace Wid
{

bool updateLayeredWindow(HWND hwnd, const QImage &source, POINT topLeft)
{
    if (!hwnd || source.isNull()) {
        return false;
    }
    // Premultiplied ARGB32 is BGRA in memory on little endian: what a 32 bit DIB expects.
    const QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = image.width();
    info.bmiHeader.biHeight = -image.height(); // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    UniqueMemoryDc dc(CreateCompatibleDC(screen));
    void *bits = nullptr;
    UniqueBitmap bitmap(CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0));
    bool ok = false;
    if (dc && bitmap && bits) {
        for (int y = 0; y < image.height(); ++y) {
            std::memcpy(static_cast<uchar *>(bits) + std::size_t(y) * image.width() * 4, image.constScanLine(y), std::size_t(image.width()) * 4);
        }
        HGDIOBJ old = SelectObject(dc.get(), bitmap.get());
        SIZE size = {image.width(), image.height()};
        POINT origin = {0, 0};
        BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        ok = UpdateLayeredWindow(hwnd, screen, &topLeft, &size, dc.get(), &origin, 0, &blend, ULW_ALPHA) != FALSE;
        SelectObject(dc.get(), old);
    }
    ReleaseDC(nullptr, screen);
    return ok;
}

QImage windowIcon(HWND hwnd, int size)
{
    HICON icon = nullptr;
    DWORD_PTR result = 0;
    // A hung window must not block the service: 50 ms at most.
    if (SendMessageTimeoutW(hwnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 50, &result)) {
        icon = reinterpret_cast<HICON>(result);
    }
    if (!icon) {
        icon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICON));
    }
    QImage image;
    if (icon) {
        image = QImage::fromHICON(icon);
    } else {
        // The program's icon; this one is a copy and must be destroyed.
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        UniqueHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
        wchar_t path[MAX_PATH * 2] = {};
        DWORD length = static_cast<DWORD>(std::size(path));
        if (process && QueryFullProcessImageNameW(process.get(), 0, path, &length)) {
            SHFILEINFOW fileInfo = {};
            if (SHGetFileInfoW(path, 0, &fileInfo, sizeof(fileInfo), SHGFI_ICON | SHGFI_LARGEICON) && fileInfo.hIcon) {
                UniqueIcon owned(fileInfo.hIcon);
                image = QImage::fromHICON(owned.get());
            }
        }
    }
    if (image.isNull()) {
        return image;
    }
    return image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

} // namespace Wid
