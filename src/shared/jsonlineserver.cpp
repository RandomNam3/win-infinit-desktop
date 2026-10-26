/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "jsonlineserver.h"

#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>

namespace Wid
{

JsonLineServer::JsonLineServer(const QString &name, Handler handler, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_handler(std::move(handler))
{
}

JsonLineServer::~JsonLineServer()
{
    for (const QPointer<QLocalSocket> &client : std::as_const(m_clients)) {
        if (client) {
            disconnect(client, nullptr, this, nullptr);
        }
    }
}

bool JsonLineServer::listen()
{
    if (m_server) {
        return m_server->isListening();
    }
    // A server that answers owns the name. (Windows lets a second process create another
    // instance of the same pipe, so listen() alone does not tell.)
    {
        QLocalSocket probe;
        probe.connectToServer(m_name);
        if (probe.waitForConnected(300)) {
            probe.disconnectFromServer();
            return false;
        }
    }
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    m_server->setMaxPendingConnections(MaxClients);
    connect(m_server, &QLocalServer::newConnection, this, &JsonLineServer::acceptConnections);
    if (m_server->listen(m_name)) {
        return true;
    }
    // Elsewhere a crashed run leaves its socket file behind.
    QLocalServer::removeServer(m_name);
    return m_server->listen(m_name);
}

bool JsonLineServer::isListening() const
{
    return m_server && m_server->isListening();
}

void JsonLineServer::acceptConnections()
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

void JsonLineServer::readClient(QLocalSocket *socket)
{
    const QPointer<QLocalSocket> guard(socket);
    while (guard && socket->canReadLine()) {
        const QByteArray line = socket->readLine(MaxLineLength + 1);
        if (line.size() > MaxLineLength) {
            socket->abort();
            return;
        }
        const QByteArray answer = handleLine(line.trimmed());
        if (guard && !answer.isEmpty() && socket->state() == QLocalSocket::ConnectedState) {
            socket->write(answer);
        }
    }
    if (guard && socket->bytesAvailable() > MaxLineLength) {
        socket->abort();
    }
}

QByteArray JsonLineServer::handleLine(const QByteArray &line) const
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

void JsonLineServer::notify(const QString &event, const QJsonObject &params)
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
