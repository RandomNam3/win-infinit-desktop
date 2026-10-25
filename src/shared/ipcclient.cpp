/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "ipcclient.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QLocalSocket>

#include <algorithm>

using namespace std::chrono;

namespace Wid
{

IpcClient::IpcClient(const QString &serverName, QObject *parent)
    : QObject(parent)
    , m_serverName(serverName)
{
    m_deadlineTimer.setSingleShot(true);
    connect(&m_deadlineTimer, &QTimer::timeout, this, &IpcClient::checkDeadlines);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &IpcClient::connectToServer);
}

IpcClient::~IpcClient()
{
    // Nothing may call back into a half destroyed client: cut the socket off first.
    if (m_socket) {
        disconnect(m_socket, nullptr, this, nullptr);
        m_socket->abort();
        delete m_socket;
        m_socket = nullptr;
    }
    m_pending.clear();
    m_queue.clear();
}

bool IpcClient::isConnected() const
{
    return m_socket && m_socket->state() == QLocalSocket::ConnectedState;
}

void IpcClient::setAutoReconnect(bool enabled, milliseconds interval)
{
    m_autoReconnect = enabled;
    m_reconnectTimer.setInterval(int(std::max(interval, milliseconds(100)).count()));
    if (!enabled) {
        m_reconnectTimer.stop();
    } else if (!isConnected()) {
        connectToServer();
    }
}

QLocalSocket *IpcClient::socket()
{
    if (!m_socket) {
        m_socket = new QLocalSocket(this);
        connect(m_socket, &QLocalSocket::connected, this, &IpcClient::onConnected);
        connect(m_socket, &QLocalSocket::disconnected, this, &IpcClient::onDisconnected);
        connect(m_socket, &QLocalSocket::readyRead, this, &IpcClient::readAvailable);
        // Connecting failed (no server) or the pipe broke. Depending on the platform the
        // error comes before or after the state change, so both are watched.
        const auto lost = [this]() {
            if (!m_socket || m_socket->state() != QLocalSocket::UnconnectedState) {
                return;
            }
            failQueued(QStringLiteral("not running"));
            failPending(QStringLiteral("disconnected"));
            if (m_wasConnected) {
                m_wasConnected = false;
                Q_EMIT connectedChanged(false);
            }
            if (m_autoReconnect && !m_reconnectTimer.isActive()) {
                m_reconnectTimer.start();
            }
        };
        connect(m_socket, &QLocalSocket::errorOccurred, this, lost);
        connect(m_socket, &QLocalSocket::stateChanged, this, lost);
    }
    return m_socket;
}

void IpcClient::connectToServer()
{
    QLocalSocket *s = socket();
    if (s->state() != QLocalSocket::UnconnectedState) {
        return;
    }
    m_buffer.clear();
    s->connectToServer(m_serverName);
}

void IpcClient::disconnectFromServer()
{
    m_reconnectTimer.stop();
    if (m_socket) {
        m_socket->disconnectFromServer();
    }
}

void IpcClient::call(const QString &method, const QJsonObject &params, QObject *context, Callback callback)
{
    const bool hasContext = context != nullptr;
    if (isConnected()) {
        send(method, params, QPointer<QObject>(context), hasContext, std::move(callback));
        return;
    }
    if (m_queue.size() >= MaxQueued) {
        if (callback && (!hasContext || context)) {
            callback(QJsonValue(), QStringLiteral("busy"));
        }
        return;
    }
    m_queue.append(Queued{method, params, QPointer<QObject>(context), hasContext, std::move(callback)});
    connectToServer();
}

void IpcClient::send(const QString &method, const QJsonObject &params, const QPointer<QObject> &context, bool hasContext, Callback callback)
{
    if (m_pending.size() >= MaxPending) {
        if (callback && (!hasContext || context)) {
            callback(QJsonValue(), QStringLiteral("busy"));
        }
        return;
    }
    const qint64 id = m_nextId++;
    QJsonObject request{{QStringLiteral("id"), id}, {QStringLiteral("method"), method}};
    if (!params.isEmpty()) {
        request.insert(QStringLiteral("params"), params);
    }
    QByteArray line = QJsonDocument(request).toJson(QJsonDocument::Compact);
    line.append('\n');
    m_pending.insert(id, Pending{context, hasContext, std::move(callback), steady_clock::now() + m_timeout});
    m_socket->write(line);
    m_socket->flush();
    updateTimer();
}

QJsonValue IpcClient::callBlocking(const QString &method, const QJsonObject &params, milliseconds timeout, QString *error)
{
    QElapsedTimer clock;
    clock.start();
    const auto remaining = [&]() {
        return int(std::max<qint64>(0, timeout.count() - clock.elapsed()));
    };
    QLocalSocket *s = socket();
    if (s->state() == QLocalSocket::UnconnectedState) {
        connectToServer();
    }
    if (s->state() != QLocalSocket::ConnectedState && !s->waitForConnected(remaining())) {
        if (error) {
            *error = QStringLiteral("not running");
        }
        return QJsonValue();
    }
    if (m_waitingFor != 0) {
        // Not reentrant: a blocking call from inside a blocking call is a bug.
        if (error) {
            *error = QStringLiteral("busy");
        }
        return QJsonValue();
    }
    const qint64 id = m_nextId;
    send(method, params, QPointer<QObject>(), false, {});
    if (!m_pending.contains(id)) {
        if (error) {
            *error = QStringLiteral("busy");
        }
        return QJsonValue();
    }
    m_waitingFor = id;
    m_waitDone = false;
    m_waitResult = QJsonValue();
    m_waitError.clear();
    while (!m_waitDone && remaining() > 0 && m_socket && m_socket->state() == QLocalSocket::ConnectedState) {
        if (!m_socket->waitForReadyRead(remaining())) {
            break;
        }
        readAvailable();
    }
    m_waitingFor = 0;
    if (!m_waitDone) {
        m_pending.remove(id);
        updateTimer();
        if (error) {
            *error = QStringLiteral("timeout");
        }
        return QJsonValue();
    }
    if (error) {
        *error = m_waitError;
    }
    return m_waitResult;
}

void IpcClient::readAvailable()
{
    if (!m_socket) {
        return;
    }
    m_buffer.append(m_socket->readAll());
    // A callback may delete this client; stop touching it then.
    const QPointer<IpcClient> alive(this);
    qsizetype start = 0;
    while (true) {
        const qsizetype newline = m_buffer.indexOf('\n', start);
        if (newline < 0) {
            break;
        }
        const QByteArray line = m_buffer.mid(start, newline - start);
        start = newline + 1;
        handleLine(line);
        if (!alive || !m_socket) {
            return;
        }
    }
    m_buffer.remove(0, start);
    if (m_buffer.size() > MaxLineLength) {
        // A broken or hostile peer: drop the connection rather than grow without bound.
        m_buffer.clear();
        m_socket->abort();
    }
}

void IpcClient::handleLine(const QByteArray &line)
{
    if (line.trimmed().isEmpty()) {
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (!document.isObject()) {
        return;
    }
    const QJsonObject message = document.object();
    if (message.contains(QStringLiteral("event"))) {
        Q_EMIT event(message.value(QStringLiteral("event")).toString(), message.value(QStringLiteral("params")).toObject());
        return;
    }
    const qint64 id = qint64(message.value(QStringLiteral("id")).toDouble(-1));
    auto it = m_pending.find(id);
    if (it == m_pending.end()) {
        return;
    }
    Pending pending = std::move(it.value());
    m_pending.erase(it);
    updateTimer();
    const QString error = message.value(QStringLiteral("error")).toString();
    const QJsonValue result = message.value(QStringLiteral("result"));
    if (id == m_waitingFor) {
        m_waitDone = true;
        m_waitResult = result;
        m_waitError = error;
        return;
    }
    finish(std::move(pending), result, error);
}

void IpcClient::finish(Pending pending, const QJsonValue &result, const QString &error)
{
    if (!pending.callback || (pending.hasContext && !pending.context)) {
        return;
    }
    pending.callback(result, error);
}

void IpcClient::onConnected()
{
    m_reconnectTimer.stop();
    if (!m_wasConnected) {
        m_wasConnected = true;
        Q_EMIT connectedChanged(true);
    }
    QList<Queued> queue;
    queue.swap(m_queue);
    for (Queued &queued : queue) {
        if (!isConnected()) {
            m_queue.append(std::move(queued));
            continue;
        }
        if (queued.hasContext && !queued.context) {
            continue;
        }
        send(queued.method, queued.params, queued.context, queued.hasContext, std::move(queued.callback));
    }
}

void IpcClient::onDisconnected()
{
    m_buffer.clear();
    failPending(QStringLiteral("disconnected"));
    if (m_wasConnected) {
        m_wasConnected = false;
        Q_EMIT connectedChanged(false);
    }
    if (m_autoReconnect) {
        m_reconnectTimer.start();
    }
}

void IpcClient::failQueued(const QString &error)
{
    QList<Queued> queue;
    queue.swap(m_queue);
    for (Queued &queued : queue) {
        if (queued.callback && (!queued.hasContext || queued.context)) {
            queued.callback(QJsonValue(), error);
        }
    }
}

void IpcClient::failPending(const QString &error)
{
    QHash<qint64, Pending> pending;
    pending.swap(m_pending);
    updateTimer();
    for (auto it = pending.begin(); it != pending.end(); ++it) {
        if (it.key() == m_waitingFor) {
            m_waitDone = true;
            m_waitError = error;
            continue;
        }
        finish(std::move(it.value()), QJsonValue(), error);
    }
}

void IpcClient::checkDeadlines()
{
    const auto now = steady_clock::now();
    QList<Pending> expired;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (it.value().deadline <= now && it.key() != m_waitingFor) {
            expired.append(std::move(it.value()));
            it = m_pending.erase(it);
        } else {
            ++it;
        }
    }
    updateTimer();
    for (Pending &pending : expired) {
        finish(std::move(pending), QJsonValue(), QStringLiteral("timeout"));
    }
}

void IpcClient::updateTimer()
{
    if (m_pending.isEmpty()) {
        m_deadlineTimer.stop();
        return;
    }
    auto earliest = steady_clock::time_point::max();
    for (const Pending &pending : std::as_const(m_pending)) {
        earliest = std::min(earliest, pending.deadline);
    }
    const auto wait = duration_cast<milliseconds>(earliest - steady_clock::now());
    m_deadlineTimer.start(int(std::clamp<qint64>(wait.count() + 1, 1, 60000)));
}

} // namespace Wid
