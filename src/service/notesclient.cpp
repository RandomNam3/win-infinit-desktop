/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "notesclient.h"
#include "logging.h"
#include "pipenames.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>

namespace Wid
{

NotesClient::NotesClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
{
    m_retry.setSingleShot(true);
    m_retry.setInterval(250);
    connect(&m_retry, &QTimer::timeout, this, &NotesClient::connectNow);
    m_giveUp.setSingleShot(true);
    m_giveUp.setInterval(GiveUpMs);
    connect(&m_giveUp, &QTimer::timeout, this, &NotesClient::giveUp);
    connect(m_socket, &QLocalSocket::connected, this, &NotesClient::onConnected);
    // Queued: errors can be reported from inside connectToServer().
    connect(m_socket, &QLocalSocket::errorOccurred, this, &NotesClient::onError, Qt::QueuedConnection);
    connect(m_socket, &QLocalSocket::readyRead, this, [this]() {
        // Answers are not needed.
        m_socket->readAll();
    });
}

NotesClient::~NotesClient()
{
    m_retry.stop();
    m_giveUp.stop();
    m_socket->disconnect(this);
    m_socket->abort();
}

QString NotesClient::programPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("wid-notes.exe"));
}

void NotesClient::call(const QString &method, const QJsonObject &params, bool start)
{
    QJsonObject request{{QStringLiteral("id"), m_nextId++}, {QStringLiteral("method"), method}};
    if (!params.isEmpty()) {
        request.insert(QStringLiteral("params"), params);
    }
    const QByteArray line = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
    if (m_socket->state() == QLocalSocket::ConnectedState) {
        write(line);
        return;
    }
    if (m_queue.size() >= MaxQueued) {
        m_queue.removeFirst();
    }
    m_queue.append(line);
    m_startAllowed |= start;
    if (!m_giveUp.isActive()) {
        m_started = false;
        m_giveUp.start();
    }
    if (m_socket->state() == QLocalSocket::UnconnectedState && !m_retry.isActive()) {
        connectNow();
    }
}

void NotesClient::connectNow()
{
    if (m_socket->state() != QLocalSocket::UnconnectedState) {
        return;
    }
    m_socket->connectToServer(notesServerName());
}

void NotesClient::onConnected()
{
    m_retry.stop();
    m_giveUp.stop();
    m_startAllowed = false;
    const QList<QByteArray> queued = std::exchange(m_queue, {});
    for (const QByteArray &line : queued) {
        write(line);
    }
}

void NotesClient::onError()
{
    if (m_socket->state() == QLocalSocket::ConnectedState || m_queue.isEmpty()) {
        // An error of a connection that is gone (the app quit): the next call reconnects.
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            m_socket->abort();
        }
        return;
    }
    m_socket->abort();
    if (!m_startAllowed) {
        // Only for a running app (e.g. "announce your notes again" at start).
        giveUp();
        return;
    }
    if (!m_started) {
        m_started = true;
        const QString program = programPath();
        if (!QFile::exists(program) || !QProcess::startDetached(program, {})) {
            qCWarning(lcService) << "cannot start the notes app" << program;
            giveUp();
            Q_EMIT unavailable();
            return;
        }
        qCInfo(lcService) << "started the notes app";
    }
    if (m_giveUp.isActive()) {
        m_retry.start();
    }
}

void NotesClient::giveUp()
{
    if (!m_queue.isEmpty()) {
        qCDebug(lcService) << "notes app not reached, dropped" << m_queue.size() << "request(s)";
    }
    m_queue.clear();
    m_retry.stop();
    m_giveUp.stop();
    m_startAllowed = false;
    if (m_socket->state() != QLocalSocket::ConnectedState) {
        m_socket->abort();
    }
}

void NotesClient::write(const QByteArray &line)
{
    // A notes app that stopped reading must not make the service buffer without end.
    if (m_socket->bytesToWrite() > MaxPendingBytes) {
        qCWarning(lcService) << "the notes app does not read its requests; dropping the connection";
        m_socket->abort();
        return;
    }
    m_socket->write(line);
}

} // namespace Wid
