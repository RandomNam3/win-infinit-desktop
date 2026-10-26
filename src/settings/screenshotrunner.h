/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSize>

class QQuickWindow;

namespace InfiniteDesktop
{

/**
 * Implements --screenshot <dir>: walks through all pages (plus a few
 * interesting states) in light and dark mode, grabs the window into PNG
 * files and quits. The QML side provides screenshotPrepare(command).
 */
class ScreenshotRunner : public QObject
{
    Q_OBJECT

public:
    ScreenshotRunner(QQuickWindow *window, const QString &directory, const QSize &size, QObject *parent = nullptr);

    void start();

private:
    struct Step {
        QString fileName;
        QString command;
        QSize size;
        QString demoState;
    };

    void addStep(const QString &fileName, const QString &command, const QSize &size = QSize(), const QString &demoState = QString());
    void runStep();
    void capture();

    QPointer<QQuickWindow> m_window;
    QString m_directory;
    QSize m_size;
    QList<Step> m_steps;
    int m_index = -1;
    int m_failures = 0;
};

} // namespace InfiniteDesktop
