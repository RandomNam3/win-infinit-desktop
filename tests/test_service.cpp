/*
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "crashguard.h"
#include "ipcserver.h"
#include "markerlayout.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QTemporaryDir>
#include <QTest>

using namespace Wid;

class TestService : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void markers()
    {
        const QRectF area(0, 0, 1000, 800);
        const QList<QRectF> windows = {
            QRectF(200, 200, 100, 100), // visible: no marker
            QRectF(3000, 350, 200, 100), // right
            QRectF(3000, 360, 200, 100), // right, about the same direction
            QRectF(400, -2000, 100, 100), // above
        };
        const QList<MarkerPlace> markers = layoutMarkers(area, windows, 44, 10);
        QCOMPARE(markers.size(), 3);
        for (const MarkerPlace &marker : markers) {
            QVERIFY(marker.window != 0);
            QVERIFY(area.adjusted(32, 32, -32, -32).adjusted(-0.5, -0.5, 0.5, 0.5).contains(marker.position));
        }
        // The two markers on the right edge do not overlap.
        QList<QPointF> right;
        for (const MarkerPlace &marker : markers) {
            if (marker.window == 1 || marker.window == 2) {
                QCOMPARE(marker.position.x(), 1000.0 - 32);
                right.append(marker.position);
            }
        }
        QCOMPARE(right.size(), 2);
        QVERIFY(QLineF(right[0], right[1]).length() >= 44);
    }

    void ipcLines()
    {
        IpcServer server([](const QString &method, const QJsonObject &params, QString *error) -> QJsonValue {
            if (method == QLatin1String("add")) {
                return params.value(QStringLiteral("a")).toDouble() + params.value(QStringLiteral("b")).toDouble();
            }
            *error = QStringLiteral("unknown");
            return {};
        });
        QJsonObject answer = QJsonDocument::fromJson(server.handleLine(R"({"id":7,"method":"add","params":{"a":1,"b":2}})")).object();
        QCOMPARE(answer.value(QStringLiteral("id")).toInt(), 7);
        QCOMPARE(answer.value(QStringLiteral("result")).toDouble(), 3.0);
        answer = QJsonDocument::fromJson(server.handleLine(R"({"id":8,"method":"nope"})")).object();
        QCOMPARE(answer.value(QStringLiteral("error")).toString(), QStringLiteral("unknown"));
        answer = QJsonDocument::fromJson(server.handleLine("not json")).object();
        QVERIFY(answer.contains(QStringLiteral("error")));
        QVERIFY(server.handleLine(QByteArray()).isEmpty());
    }

    void crashGuard()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("crashguard"));
        for (int run = 0; run < CrashGuard::Threshold; ++run) {
            CrashGuard guard(path);
            QVERIFY(!guard.start(true));
            // No markClean(): a crash.
        }
        CrashGuard guard(path);
        QVERIFY(guard.start(true));
        QVERIFY(guard.isSafeMode());
        guard.leaveSafeMode();
        QVERIFY(!guard.isSafeMode());
        guard.markClean();
        QCOMPARE(guard.counter(), 0);
        CrashGuard disabled(path);
        QVERIFY(!disabled.start(false));
    }
};

QTEST_GUILESS_MAIN(TestService)

#include "test_service.moc"
