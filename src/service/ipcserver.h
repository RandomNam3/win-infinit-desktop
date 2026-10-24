/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "pipenames.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QPointer>

#include <functional>

class QLocalServer;
class QLocalSocket;

namespace Wid
{

/**
 * The service's control interface, the Windows counterpart of the D-Bus interface
 * org.kde.InfinitDesktop: newline delimited JSON over a named pipe that only the current
 * user can open.
 *
 * Request:  {"id": 1, "method": "pan", "params": {"dx": 100, "dy": 0}}
 * Answer:   {"id": 1, "result": …}  or  {"id": 1, "error": "message"}
 * Event:    {"event": "statusChanged", "params": {…}}  (to every connected client)
 *
 * Lines longer than MaxLineLength close the connection; at most MaxClients at a time.
 */
class IpcServer : public QObject
{
    Q_OBJECT

public:
    static constexpr int MaxLineLength = 64 * 1024;
    static constexpr int MaxClients = 16;

    /// Returns the result; sets `error` to fail the request.
    using Handler = std::function<QJsonValue(const QString &method, const QJsonObject &params, QString *error)>;

    explicit IpcServer(Handler handler, QObject *parent = nullptr);
    ~IpcServer() override;

    bool listen();
    void notify(const QString &event, const QJsonObject &params);

    /// Handles one request line; public for the unit tests.
    QByteArray handleLine(const QByteArray &line) const;

private:
    void acceptConnections();
    void readClient(QLocalSocket *socket);

    Handler m_handler;
    QLocalServer *m_server = nullptr;
    QList<QPointer<QLocalSocket>> m_clients;
};

} // namespace Wid
