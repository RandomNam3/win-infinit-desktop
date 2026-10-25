/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTimer>

class QLocalSocket;

namespace Wid
{

/**
 * Client of the notes app's pipe (wid-notes.exe, notesServerName()): requests in the
 * framing of IpcServer, fire and forget. The service never waits for the notes app.
 *
 * Requests made while the app is not connected wait in a short queue (MaxQueued, the oldest
 * is dropped first); if the app is not running it is started once, and after GiveUpMs
 * without a connection the queue is dropped. Answers are read and ignored.
 */
class NotesClient : public QObject
{
    Q_OBJECT

public:
    static constexpr int MaxQueued = 8;
    static constexpr int GiveUpMs = 10000;
    static constexpr qint64 MaxPendingBytes = 64 * 1024;

    explicit NotesClient(QObject *parent = nullptr);
    ~NotesClient() override;

    /// Sends `method`; `start`: start the notes app if it is not running.
    void call(const QString &method, const QJsonObject &params = {}, bool start = true);
    /// Full path of wid-notes.exe next to the service.
    static QString programPath();

Q_SIGNALS:
    /// The notes app is not installed next to the service.
    void unavailable();

private:
    void connectNow();
    void onConnected();
    void onError();
    void giveUp();
    void write(const QByteArray &line);

    QLocalSocket *m_socket = nullptr;
    QList<QByteArray> m_queue;
    bool m_startAllowed = false;
    bool m_started = false; ///< started by this round of connection attempts
    QTimer m_retry;
    QTimer m_giveUp;
    qint64 m_nextId = 1;
};

} // namespace Wid
