/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>

#include <functional>

namespace Wid
{
class IpcClient;
}

namespace InfiniteDesktop
{

/**
 * The settings app's connections to the canvas service and to the notes app (named pipes,
 * see IpcClient). While the app runs, a lost service connection is retried every few
 * seconds, so the status follows the service starting and stopping. Everything here is
 * asynchronous.
 */
class ServiceConnection : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(const QJsonValue &result, const QString &error)>;

    static ServiceConnection *instance();
    /// No pipes at all (screenshots, unit tests): calls fail at once with "offline".
    static void setOffline(bool offline);
    static bool isOffline();

    bool isConnected() const;

    /// A request to the service; `callback` is dropped once `context` is gone.
    void call(const QString &method, const QJsonObject &params = {}, QObject *context = nullptr, Callback callback = {});
    /// A request to the notes app (not started when it is not running).
    void callNotes(const QString &method, const QJsonObject &params = {}, QObject *context = nullptr, Callback callback = {});

Q_SIGNALS:
    void connectedChanged(bool connected);
    /// An event of the service ("statusChanged", "groupsChanged", …).
    void serviceEvent(const QString &name, const QJsonObject &params);

private:
    explicit ServiceConnection(QObject *parent);
    ~ServiceConnection() override;

    Wid::IpcClient *m_service = nullptr;
    Wid::IpcClient *m_notes = nullptr;
};

} // namespace InfiniteDesktop
