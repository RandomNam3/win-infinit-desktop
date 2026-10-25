/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "serviceconnection.h"

#include "ipcclient.h"
#include "pipenames.h"

#include <QCoreApplication>
#include <QPointer>
#include <QTimer>

using namespace std::chrono_literals;

namespace InfinitDesktop
{

namespace
{
ServiceConnection *s_instance = nullptr;
bool s_offline = false;

void failLater(QObject *context, ServiceConnection::Callback callback)
{
    if (!callback) {
        return;
    }
    const QPointer<QObject> guard(context);
    const bool hasContext = context != nullptr;
    QTimer::singleShot(0, QCoreApplication::instance(), [guard, hasContext, callback = std::move(callback)]() {
        if (!hasContext || guard) {
            callback(QJsonValue(), QStringLiteral("offline"));
        }
    });
}
} // namespace

ServiceConnection::ServiceConnection(QObject *parent)
    : QObject(parent)
{
    if (s_offline) {
        return;
    }
    m_service = new Wid::IpcClient(Wid::ipcServerName(), this);
    m_service->setTimeout(3s);
    connect(m_service, &Wid::IpcClient::connectedChanged, this, &ServiceConnection::connectedChanged);
    connect(m_service, &Wid::IpcClient::event, this, &ServiceConnection::serviceEvent);
    // Follow the service while the app is open; the app is short-lived, so this costs nothing.
    m_service->setAutoReconnect(true, 3s);

    m_notes = new Wid::IpcClient(Wid::notesServerName(), this);
    m_notes->setTimeout(2s);
}

ServiceConnection::~ServiceConnection()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

ServiceConnection *ServiceConnection::instance()
{
    if (!s_instance) {
        s_instance = new ServiceConnection(QCoreApplication::instance());
    }
    return s_instance;
}

void ServiceConnection::setOffline(bool offline)
{
    s_offline = offline;
}

bool ServiceConnection::isOffline()
{
    return s_offline;
}

bool ServiceConnection::isConnected() const
{
    return m_service && m_service->isConnected();
}

void ServiceConnection::call(const QString &method, const QJsonObject &params, QObject *context, Callback callback)
{
    if (!m_service) {
        failLater(context, std::move(callback));
        return;
    }
    m_service->call(method, params, context, std::move(callback));
}

void ServiceConnection::callNotes(const QString &method, const QJsonObject &params, QObject *context, Callback callback)
{
    if (!m_notes) {
        failLater(context, std::move(callback));
        return;
    }
    m_notes->call(method, params, context, std::move(callback));
}

} // namespace InfinitDesktop
