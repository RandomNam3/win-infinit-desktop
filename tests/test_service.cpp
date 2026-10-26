/*
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "crashguard.h"
#include "ipcserver.h"
#include "markerlayout.h"
#include "markers.h"
#include "placementrules.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QPainter>
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

    void markersForWindowsParkedAtTheEdge()
    {
        const QRectF area(0, 0, 1000, 800);
        // Reaches into the area, but parked at the edge to a neighbouring screen.
        const QList<QRectF> windows = {QRectF(800, 300, 400, 200), QRectF(100, 100, 200, 200)};
        QCOMPARE(layoutMarkers(area, windows, 44, 10).size(), 0);
        const QList<MarkerPlace> markers = layoutMarkers(area, windows, 44, 10, {true, false});
        QCOMPARE(markers.size(), 1);
        QCOMPARE(markers.front().window, 0);
        QCOMPARE(markers.front().position.x(), 1000.0 - 32);
    }

    void ownMovesOfABusyProgram()
    {
        MoveRequests requests;
        ULONGLONG tick = 10000;
        // Three moves on their way: the program is busy, a fourth would only queue up.
        for (int i = 0; i < 3; ++i, tick += 16) {
            QVERIFY(!requests.busy(tick));
            requests.add(POINT{1000 - i * 10, 300}, POINT{993 - i * 10, 300}, tick);
        }
        QVERIFY(requests.busy(tick));
        // It reports the first one, late: ours, and it is free again.
        tick += 200;
        QVERIFY(requests.answer(POINT{1000, 300}, POINT{0, 0}, tick));
        QVERIFY(!requests.busy(tick));
        // Reported by its window rect, ±1 px: answers the older ones too.
        requests.add(POINT{900, 300}, POINT{893, 300}, tick);
        QVERIFY(requests.answer(POINT{0, 0}, POINT{894, 301}, tick + 20));
        QVERIFY(!requests.busy(tick + 20));
        // A position reported twice (the frame catches up) is still ours.
        QVERIFY(requests.answer(POINT{990, 300}, POINT{0, 0}, tick + 40));
        // The program's or Windows' own move.
        QVERIFY(!requests.answer(POINT{1234, 567}, POINT{1227, 567}, tick + 40));
        // Too old to count.
        QVERIFY(!requests.answer(POINT{1000, 300}, POINT{0, 0}, tick + 6000));
    }

    void movesNobodyAnswersDoNotBlock()
    {
        // A move to where the window already is is never reported: after half a second it is
        // no longer in flight.
        MoveRequests requests;
        for (int i = 0; i < 3; ++i) {
            requests.add(POINT{i, 0}, POINT{i, 0}, 1000);
        }
        QVERIFY(requests.busy(1100));
        QVERIFY(!requests.busy(1600));
    }

    void manyMovesKeepTheNewest()
    {
        MoveRequests requests;
        for (int i = 0; i < int(MoveRequests::Capacity) * 3; ++i) {
            requests.add(POINT{i, 0}, POINT{i, 0}, 1000 + ULONGLONG(i));
        }
        const int newest = int(MoveRequests::Capacity) * 3 - 1;
        QVERIFY(requests.answer(POINT{newest, 0}, POINT{-9, -9}, 2000));
        QVERIFY(!requests.answer(POINT{0, 0}, POINT{-9, -9}, 2000)); // overwritten
    }

    void edgeToANeighbouringScreen()
    {
        const QList<QRectF> screens = {QRectF(0, 0, 1920, 1080), QRectF(1920, 0, 1920, 1080), QRectF(-1280, 1080, 1280, 1024)};
        QVERIFY(reachesOtherScreen(QRectF(1800, 100, 400, 300), screens, 0));
        QVERIFY(!reachesOtherScreen(QRectF(1500, 100, 400, 300), screens, 0));
        QVERIFY(!reachesOtherScreen(QRectF(1520, 100, 400, 300), screens, 0)); // touching
        QVERIFY(!reachesOtherScreen(QRectF(-300, 100, 400, 300), screens, 0)); // nothing there
        QVERIFY(reachesOtherScreen(QRectF(-300, 1000, 400, 300), screens, 0));

        // Moved by the program or by Windows: its screen while it still reaches into it.
        QCOMPARE(screenAfterForeignMove(QRectF(1800, 100, 400, 300), screens, 0), 0);
        QCOMPARE(screenAfterForeignMove(QRectF(2000, 100, 400, 300), screens, 0), 1);
        QCOMPARE(screenAfterForeignMove(QRectF(1800, 100, 400, 300), screens, -1), 1);
        QCOMPARE(screenAfterForeignMove(QRectF(9000, 9000, 400, 300), screens, 0), -1);
    }

    void markerIconsStayVisible()
    {
        const auto iconOf = [](const QColor &color) {
            QImage icon(28, 28, QImage::Format_ARGB32_Premultiplied);
            icon.fill(Qt::transparent);
            QPainter painter(&icon);
            painter.fillRect(QRect(6, 6, 16, 16), color);
            return icon;
        };
        // Right next to the icon, inside the disc a dark icon gets.
        const QPoint beside(Markers::Extent / 2 + 15, Markers::Extent / 2);
        const auto lightness = [&](const QImage &image) {
            return image.pixelColor(beside).lightnessF();
        };
        QVERIFY(lightness(Markers::image(iconOf(Qt::black), QPointF(1, 0))) > 0.8);
        QVERIFY(lightness(Markers::image(iconOf(QColor(0xff, 0xe2, 0x7a)), QPointF(1, 0))) < 0.2);
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
