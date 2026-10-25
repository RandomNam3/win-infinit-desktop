/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

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
 * Server side of the newline delimited JSON protocol (see IpcClient), used by the notes app.
 * Only the current user can open the pipe. Lines longer than MaxLineLength close the
 * connection; at most MaxClients connections at a time.
 */
class JsonLineServer : public QObject
{
    Q_OBJECT

public:
    static constexpr qsizetype MaxLineLength = 64 * 1024;
    static constexpr int MaxClients = 16;

    /// Returns the result; sets `error` to fail the request.
    using Handler = std::function<QJsonValue(const QString &method, const QJsonObject &params, QString *error)>;

    JsonLineServer(const QString &name, Handler handler, QObject *parent = nullptr);
    ~JsonLineServer() override;

    /// False if the name is taken by a running server.
    bool listen();
    bool isListening() const;
    void notify(const QString &event, const QJsonObject &params);

    /// Handles one request line; public for the unit tests.
    QByteArray handleLine(const QByteArray &line) const;

private:
    void acceptConnections();
    void readClient(QLocalSocket *socket);

    QString m_name;
    Handler m_handler;
    QLocalServer *m_server = nullptr;
    QList<QPointer<QLocalSocket>> m_clients;
};

} // namespace Wid
