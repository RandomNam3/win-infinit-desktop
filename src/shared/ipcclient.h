/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <chrono>
#include <functional>

class QLocalSocket;

namespace Wid
{

/**
 * Client side of the newline delimited JSON protocol of the service and the notes app
 * (see IpcServer in the service):
 *
 * Request:  {"id": 1, "method": "…", "params": {…}}
 * Answer:   {"id": 1, "result": …}  or  {"id": 1, "error": "message"}
 * Event:    {"event": "…", "params": {…}}
 *
 * Calls are asynchronous: the answer goes to a callback, which is dropped when its context
 * object is gone. While the pipe is not connected, a few calls wait for the connection;
 * they fail when it cannot be made. Nothing runs while idle: the only timer is armed while
 * answers are outstanding or (optionally) while a lost connection is retried.
 */
class IpcClient : public QObject
{
    Q_OBJECT

public:
    /// Longest line accepted from the other side (a groups document can be large).
    static constexpr qsizetype MaxLineLength = 4 * 1024 * 1024;
    /// Calls that may wait for the connection.
    static constexpr int MaxQueued = 32;
    /// Answers that may be outstanding at a time.
    static constexpr int MaxPending = 64;

    using Callback = std::function<void(const QJsonValue &result, const QString &error)>;

    explicit IpcClient(const QString &serverName, QObject *parent = nullptr);
    ~IpcClient() override;

    QString serverName() const
    {
        return m_serverName;
    }
    bool isConnected() const;

    /// How long an answer may take before the call fails (default 3 s).
    void setTimeout(std::chrono::milliseconds timeout)
    {
        m_timeout = timeout;
    }
    /// While enabled, a lost or missing connection is retried every `interval`.
    void setAutoReconnect(bool enabled, std::chrono::milliseconds interval = std::chrono::seconds(3));

    /// Connects if not connected (asynchronous).
    void connectToServer();
    void disconnectFromServer();

    /**
     * Sends a request. `callback` (optional) runs on this thread with the result or an error,
     * unless `context` was destroyed in the meantime.
     */
    void call(const QString &method, const QJsonObject &params = {}, QObject *context = nullptr, Callback callback = {});

    /**
     * Sends a request and waits for its answer, at most `timeout`. Events and other answers
     * that arrive meanwhile are handled as usual. Only for the few calls that must finish
     * before the caller goes on (announcing a note window before it is shown).
     */
    QJsonValue callBlocking(const QString &method, const QJsonObject &params, std::chrono::milliseconds timeout, QString *error = nullptr);

    /// Handles one line from the server; public for the unit tests.
    void handleLine(const QByteArray &line);

Q_SIGNALS:
    void connectedChanged(bool connected);
    void event(const QString &name, const QJsonObject &params);

private:
    struct Pending
    {
        QPointer<QObject> context;
        bool hasContext = false;
        Callback callback;
        std::chrono::steady_clock::time_point deadline;
    };
    struct Queued
    {
        QString method;
        QJsonObject params;
        QPointer<QObject> context;
        bool hasContext = false;
        Callback callback;
    };

    QLocalSocket *socket();
    void send(const QString &method, const QJsonObject &params, const QPointer<QObject> &context, bool hasContext, Callback callback);
    void readAvailable();
    void onConnected();
    void onDisconnected();
    void failQueued(const QString &error);
    void failPending(const QString &error);
    void finish(Pending pending, const QJsonValue &result, const QString &error);
    void checkDeadlines();
    void updateTimer();

    QString m_serverName;
    QLocalSocket *m_socket = nullptr;
    QByteArray m_buffer;
    qint64 m_nextId = 1;
    QHash<qint64, Pending> m_pending;
    QList<Queued> m_queue;
    std::chrono::milliseconds m_timeout{3000};
    QTimer m_deadlineTimer;
    QTimer m_reconnectTimer;
    bool m_autoReconnect = false;
    bool m_wasConnected = false;
    /// Answers taken while callBlocking() waits.
    qint64 m_waitingFor = 0;
    bool m_waitDone = false;
    QJsonValue m_waitResult;
    QString m_waitError;
};

} // namespace Wid
