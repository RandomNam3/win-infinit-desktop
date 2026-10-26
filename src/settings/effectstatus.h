/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

namespace InfiniteDesktop
{

/**
 * State of the canvas service (win-infinite-desktop.exe), for the start page and the
 * sidebar. The QML name stays "Effect", as in KDE Infinite Desktop.
 *
 * @c state is one of "active", "paused", "safemode", "disabled" (installed, not running),
 * "notinstalled" (the service is not next to this app) or "unknown" (checking).
 * Turning it on starts the service and lets it start at login (HKCU\…\Run); turning it
 * off asks it to quit and removes the login entry.
 */
class EffectStatus : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Effect)
    QML_SINGLETON
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(bool installed READ installed NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
    Q_PROPERTY(bool safeMode READ safeMode NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool startsAtLogin READ startsAtLogin NOTIFY changed)
    /// Version of the running service (empty while it is not running).
    Q_PROPERTY(QString effectVersion READ effectVersion NOTIFY changed)
    /// "Windows 11 25H2 (build 26200)"
    Q_PROPERTY(QString osVersion READ osVersion CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)

public:
    explicit EffectStatus(QObject *parent);
    ~EffectStatus() override;

    static EffectStatus *instance();
    static EffectStatus *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    QString state() const
    {
        return m_state;
    }
    bool installed() const;
    bool enabled() const
    {
        return m_loaded;
    }
    bool loaded() const
    {
        return m_loaded;
    }
    bool safeMode() const
    {
        return m_safeMode;
    }
    bool paused() const
    {
        return m_paused;
    }
    bool busy() const
    {
        return m_busy;
    }
    bool startsAtLogin() const;
    QString effectVersion() const
    {
        return m_serviceVersion;
    }
    QString osVersion() const;
    QString appVersion() const;

    /// Starts the service (and the start at login) or stops it (and the start at login).
    Q_INVOKABLE void setEnabled(bool enabled);
    Q_INVOKABLE void leaveSafeMode();
    Q_INVOKABLE void setPaused(bool paused);

    /// Screenshot mode: pretend a fixed state, no pipes at all.
    void setDemoState(const QString &state);

public Q_SLOTS:
    void refresh();

Q_SIGNALS:
    void changed();
    void actionFailed(const QString &message);

private:
    void computeState();
    void setStartAtLogin(bool start);

    QString m_state = QStringLiteral("unknown");
    bool m_loaded = false;
    bool m_safeMode = false;
    bool m_paused = false;
    bool m_busy = false;
    bool m_demo = false;
    bool m_checked = false;
    /// What setEnabled() asked for: -1 nothing, 0 stopped, 1 running.
    int m_target = -1;
    QString m_serviceVersion;
};

} // namespace InfiniteDesktop
