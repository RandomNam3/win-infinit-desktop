/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "iconprovider.h"

#include <QBuffer>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QPainter>

#ifdef Q_OS_WIN
#include "shellitem_win.h"
#endif

namespace InfiniteDesktop
{

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int slash = id.lastIndexOf(QLatin1Char('/'));
    const QString name = slash > 0 ? id.left(slash) : id;
    QColor color = slash > 0 ? QColor::fromString(QLatin1Char('#') + id.mid(slash + 1)) : QColor(Qt::black);
    if (!color.isValid()) {
        color = Qt::black;
    }

    QSize target = requestedSize;
    if (target.width() <= 0 && target.height() <= 0) {
        target = QSize(24, 24);
    } else if (target.width() <= 0) {
        target.setWidth(target.height());
    } else if (target.height() <= 0) {
        target.setHeight(target.width());
    }

    QFile file(QStringLiteral(":/infinitedesktop/icons/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("IconProvider: unknown icon %s", qPrintable(name));
        QImage empty(target, QImage::Format_ARGB32_Premultiplied);
        empty.fill(Qt::transparent);
        if (size) {
            *size = target;
        }
        return empty;
    }
    QByteArray data = file.readAll();
    data.replace("currentColor", color.name(QColor::HexRgb).toLatin1());

    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "svg");
    reader.setScaledSize(target);
    QImage image = reader.read();
    if (image.isNull()) {
        qWarning("IconProvider: could not render %s: %s", qPrintable(name), qPrintable(reader.errorString()));
        image = QImage(target, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
    } else if (color.alpha() < 255) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        painter.fillRect(image.rect(), QColor(0, 0, 0, color.alpha()));
    }
    if (size) {
        *size = image.size();
    }
    return image;
}

ThemeIconProvider::ThemeIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage ThemeIconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QSize target = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(32, 32);
    QImage image;
#ifdef Q_OS_WIN
    // "shell-<hex>": a Start menu entry, "file-<hex>": a program (see ApplicationIndex).
    // Runs on QML's image loader thread, so COM is set up here.
    const bool appsFolder = id.startsWith(QLatin1String("shell-"));
    if (appsFolder || id.startsWith(QLatin1String("file-"))) {
        const QString name = QString::fromUtf8(QByteArray::fromHex(id.mid(id.indexOf(QLatin1Char('-')) + 1).toLatin1()));
        const ComApartment apartment;
        if (apartment.ok()) {
            image = shellItemIcon(name, appsFolder, target);
            if (!image.isNull() && image.size() != target) {
                image = image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
    }
#else
    if (QFileInfo(id).isAbsolute()) {
        image = QIcon(id).pixmap(target).toImage();
    }
#endif
    if (image.isNull()) {
        image = QImage(target, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
    }
    if (size) {
        *size = image.size();
    }
    return image;
}

} // namespace InfiniteDesktop
