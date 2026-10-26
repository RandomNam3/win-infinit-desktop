/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QColor>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

namespace InfiniteDesktop
{

/**
 * Light/dark mode and accent color of the system, used by Theme.qml.
 *
 * Follows QStyleHints::colorScheme() (falls back to the lightness of the
 * window color) and takes the accent from the palette highlight.
 */
class ThemeController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SystemTheme)
    QML_SINGLETON
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(bool animations READ animations NOTIFY changed)
    Q_PROPERTY(bool screenshotMode READ screenshotMode CONSTANT)
    /// "light" or "dark" to override the system, empty to follow it.
    Q_PROPERTY(QString forcedScheme READ forcedScheme WRITE setForcedScheme NOTIFY changed)

public:
    explicit ThemeController(QObject *parent);

    static ThemeController *instance();
    static ThemeController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    bool dark() const;
    QColor accent() const;
    bool animations() const;
    bool screenshotMode() const;
    void setScreenshotMode(bool screenshotMode);
    QString forcedScheme() const;
    void setForcedScheme(const QString &scheme);
    void setForcedAccent(const QColor &accent);

Q_SIGNALS:
    void changed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void update();

    bool m_dark = false;
    QColor m_accent;
    QColor m_forcedAccent;
    QString m_forcedScheme;
    bool m_screenshotMode = false;
};

} // namespace InfiniteDesktop
