/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
// The pipe protocol between the apps and the service (ipcclient.h, jsonlineserver.h).

#include "ipcclient.h"
#include "jsonlineserver.h"

#include <QJsonArray>
#include <QLocalSocket>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QUuid>

#include <memory>

using namespace Wid;
using namespace std::chrono_literals;

/// A server on its own thread, like the service in its own process: a client that blocks
/// still gets its answer.
class ServerThread : public QThread
{
public:
    explicit ServerThread(const QString &name)
        : m_name(name)
    {
    }
    ~ServerThread() override
    {
        quit();
        wait();
    }
    void startAndWait()
    {
        start();
        m_ready.acquire();
    }

protected:
    void run() override
    {
        JsonLineServer server(m_name, [](const QString &method, const QJsonObject &params, QString *error) -> QJsonValue {
            if (method == QLatin1String("echo")) {
                return params.value(QStringLiteral("value"));
            }
            *error = QStringLiteral("unknown method");
            return {};
        });
        server.listen();
        m_ready.release();
        exec();
    }

private:
    QString m_name;
    QSemaphore m_ready;
};

class TestIpcClient : public QObject
{
    Q_OBJECT

private:
    QString m_name;
    std::unique_ptr<JsonLineServer> m_server;
    int m_calls = 0;

private Q_SLOTS:
    void init()
    {
        m_name = QStringLiteral("wid-test-") + QUuid::createUuid().toString(QUuid::Id128);
        m_calls = 0;
        m_server = std::make_unique<JsonLineServer>(m_name, [this](const QString &method, const QJsonObject &params, QString *error) -> QJsonValue {
            ++m_calls;
            if (method == QLatin1String("echo")) {
                return params.value(QStringLiteral("value"));
            }
            if (method == QLatin1String("slow")) {
                QTest::qWait(300);
                return true;
            }
            *error = QStringLiteral("unknown method");
            return {};
        });
        QVERIFY(m_server->listen());
    }

    void cleanup()
    {
        m_server.reset();
    }

    void callAndAnswer()
    {
        IpcClient client(m_name);
        QJsonValue result;
        QString error = QStringLiteral("unset");
        bool done = false;
        client.call(QStringLiteral("echo"), {{QStringLiteral("value"), QJsonArray{1, 2, 3}}}, this, [&](const QJsonValue &value, const QString &message) {
            result = value;
            error = message;
            done = true;
        });
        QTRY_VERIFY(done);
        QVERIFY(error.isEmpty());
        QCOMPARE(result.toArray(), (QJsonArray{1, 2, 3}));

        done = false;
        client.call(QStringLiteral("nope"), {}, this, [&](const QJsonValue &, const QString &message) {
            error = message;
            done = true;
        });
        QTRY_VERIFY(done);
        QCOMPARE(error, QStringLiteral("unknown method"));
    }

    void blockingCall()
    {
        m_server.reset();
        ServerThread thread(m_name);
        thread.startAndWait();
        IpcClient client(m_name);
        QString error;
        const QJsonValue value = client.callBlocking(QStringLiteral("echo"), {{QStringLiteral("value"), QStringLiteral("hi")}}, 2s, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(value.toString(), QStringLiteral("hi"));
    }

    void events()
    {
        IpcClient client(m_name);
        QSignalSpy spy(&client, &IpcClient::event);
        // Any call connects; then the server's events arrive.
        bool done = false;
        client.call(QStringLiteral("echo"), {{QStringLiteral("value"), 1}}, this, [&](const QJsonValue &, const QString &) {
            done = true;
        });
        QTRY_VERIFY(done);
        m_server->notify(QStringLiteral("groupsChanged"), {{QStringLiteral("count"), 2}});
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("groupsChanged"));
        QCOMPARE(spy.at(0).at(1).toJsonObject().value(QStringLiteral("count")).toInt(), 2);
    }

    void noServer()
    {
        m_server.reset();
        IpcClient client(m_name);
        QString error;
        bool done = false;
        client.call(QStringLiteral("echo"), {}, this, [&](const QJsonValue &, const QString &message) {
            error = message;
            done = true;
        });
        QTRY_VERIFY(done);
        QVERIFY(!error.isEmpty());
        QString blockingError;
        client.callBlocking(QStringLiteral("echo"), {}, 200ms, &blockingError);
        QVERIFY(!blockingError.isEmpty());
    }

    void timeout()
    {
        IpcClient client(m_name);
        client.setTimeout(100ms);
        QString error;
        bool done = false;
        // The server answers after 300 ms, the client gave up after 100 ms.
        client.call(QStringLiteral("slow"), {}, this, [&](const QJsonValue &, const QString &message) {
            error = message;
            done = true;
        });
        QTRY_VERIFY(done);
        QCOMPARE(error, QStringLiteral("timeout"));
        // The late answer is dropped without a crash.
        QTest::qWait(400);
    }

    void droppedContext()
    {
        IpcClient client(m_name);
        auto context = std::make_unique<QObject>();
        bool called = false;
        client.call(QStringLiteral("echo"), {{QStringLiteral("value"), 1}}, context.get(), [&](const QJsonValue &, const QString &) {
            called = true;
        });
        context.reset();
        QTRY_COMPARE(m_calls, 1);
        QTest::qWait(100);
        QVERIFY(!called);
    }

    void clientDeletedWhileWaiting()
    {
        auto client = std::make_unique<IpcClient>(m_name);
        client->call(QStringLiteral("slow"));
        QTest::qWait(50);
        client.reset();
        QTest::qWait(400);
    }

    void overlongLines()
    {
        // A line over the limit closes the connection instead of growing a buffer.
        QLocalSocket socket;
        socket.connectToServer(m_name);
        QVERIFY(socket.waitForConnected(1000));
        socket.write(QByteArray(JsonLineServer::MaxLineLength + 10, 'x'));
        socket.flush();
        QTRY_VERIFY(socket.state() != QLocalSocket::ConnectedState);
        QCOMPARE(m_calls, 0);
    }

    void secondServerIsRefused()
    {
        JsonLineServer second(m_name, {});
        QVERIFY(!second.listen());
    }

    void handlesGarbage()
    {
        QCOMPARE(m_server->handleLine("not json"), QByteArray(R"({"error":"invalid JSON"})" "\n"));
        QVERIFY(m_server->handleLine(R"({"id":4})").contains("no method"));
        IpcClient client(m_name);
        client.handleLine("garbage");
        client.handleLine(R"({"id": 99, "result": 1})");
        client.handleLine(R"({"event": 5})");
    }
};

QTEST_GUILESS_MAIN(TestIpcClient)
#include "test_ipcclient.moc"
