/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "ipcserver.h"

#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>

namespace Wid
{

IpcServer::IpcServer(Handler handler, QObject *parent)
    : QObject(parent)
    , m_handler(std::move(handler))
{
}

IpcServer::~IpcServer() = default;

bool IpcServer::listen()
{
    if (m_server) {
        return m_server->isListening();
    }
    m_server = new QLocalServer(this);
    // Only the user who runs the service may connect.
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    m_server->setMaxPendingConnections(MaxClients);
    connect(m_server, &QLocalServer::newConnection, this, &IpcServer::acceptConnections);
    const QString name = ipcServerName();
    if (!m_server->listen(name)) {
        // A dead pipe of a crashed run can block the name.
        QLocalServer::removeServer(name);
        return m_server->listen(name);
    }
    return true;
}

void IpcServer::acceptConnections()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        m_clients.removeAll(nullptr);
        if (m_clients.size() >= MaxClients) {
            socket->disconnectFromServer();
            socket->deleteLater();
            continue;
        }
        m_clients.append(socket);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            readClient(socket);
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void IpcServer::readClient(QLocalSocket *socket)
{
    while (socket->canReadLine()) {
        const QByteArray line = socket->readLine(MaxLineLength + 1);
        if (line.size() > MaxLineLength) {
            socket->abort();
            return;
        }
        const QByteArray answer = handleLine(line.trimmed());
        if (!answer.isEmpty()) {
            socket->write(answer);
        }
    }
    if (socket->bytesAvailable() > MaxLineLength) {
        socket->abort();
    }
}

QByteArray IpcServer::handleLine(const QByteArray &line) const
{
    if (line.isEmpty()) {
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    QJsonObject answer;
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        answer.insert(QStringLiteral("error"), QStringLiteral("invalid JSON"));
        return QJsonDocument(answer).toJson(QJsonDocument::Compact) + '\n';
    }
    const QJsonObject request = document.object();
    answer.insert(QStringLiteral("id"), request.value(QStringLiteral("id")));
    const QString method = request.value(QStringLiteral("method")).toString();
    if (method.isEmpty() || !m_handler) {
        answer.insert(QStringLiteral("error"), QStringLiteral("no method"));
    } else {
        QString error;
        const QJsonValue result = m_handler(method, request.value(QStringLiteral("params")).toObject(), &error);
        if (!error.isEmpty()) {
            answer.insert(QStringLiteral("error"), error);
        } else {
            answer.insert(QStringLiteral("result"), result);
        }
    }
    return QJsonDocument(answer).toJson(QJsonDocument::Compact) + '\n';
}

void IpcServer::notify(const QString &event, const QJsonObject &params)
{
    const QByteArray line =
        QJsonDocument(QJsonObject{{QStringLiteral("event"), event}, {QStringLiteral("params"), params}}).toJson(QJsonDocument::Compact) + '\n';
    m_clients.removeAll(nullptr);
    for (const QPointer<QLocalSocket> &client : std::as_const(m_clients)) {
        if (client && client->state() == QLocalSocket::ConnectedState) {
            client->write(line);
        }
    }
}

} // namespace Wid
