/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace Wid
{

/**
 * Counts starts that did not end cleanly, like KDE Infinit Desktop's CrashGuard.
 *
 * Every start increments a counter on disk; 30 s of stable running or a clean exit reset
 * it. After three crashes in a row the service starts in safe mode: no hooks, no window
 * management, until the user leaves it (tray menu or IPC).
 */
class CrashGuard : public QObject
{
    Q_OBJECT

public:
    static constexpr int Threshold = 3;
    static constexpr int StableAfterMs = 30000;

    /// `path`: the counter file; empty = %LOCALAPPDATA%\WIN Infinit Desktop\crashguard.
    explicit CrashGuard(const QString &path = QString(), QObject *parent = nullptr);

    /// Reads the counter and arms the guard. Returns true if safe mode is required.
    bool start(bool enabled);
    bool isSafeMode() const
    {
        return m_safeMode;
    }
    void leaveSafeMode();
    void markClean();
    int counter() const;

private:
    void writeCounter(int value) const;

    QString m_path;
    QTimer m_stableTimer;
    bool m_safeMode = false;
    bool m_armed = false;
};

} // namespace Wid
