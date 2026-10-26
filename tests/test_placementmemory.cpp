/*
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "placementmemory.h"

#include <QJsonDocument>
#include <QTest>

using namespace InfiniteDesktop;

class TestPlacementMemory : public QObject
{
    Q_OBJECT

    static WindowPlacement placement(const QString &app, qreal x, qint64 time = 1)
    {
        WindowPlacement p;
        p.appKey = app;
        p.output = OutputIdentity{QStringLiteral("DP-1"), QStringLiteral("DEL"), QStringLiteral("U2720Q"), QStringLiteral("ABC123")};
        p.canvasPos = QPointF(x, 10);
        p.screenOffset = QPointF(x, 20);
        p.size = QSizeF(800, 600);
        p.timestamp = time;
        return p;
    }

private Q_SLOTS:
    void applicationKey()
    {
        QCOMPARE(PlacementMemory::applicationKey(QStringLiteral("org.kde.konsole.desktop"), QStringLiteral("konsole")), QStringLiteral("org.kde.konsole"));
        QCOMPARE(PlacementMemory::applicationKey(QString(), QStringLiteral("Firefox")), QStringLiteral("firefox"));
        QCOMPARE(PlacementMemory::applicationKey(QStringLiteral("  "), QString()), QString());
    }

    void outputMatching()
    {
        const OutputIdentity stored{QStringLiteral("DP-1"), QStringLiteral("DEL"), QStringLiteral("U2720Q"), QStringLiteral("ABC123")};
        const OutputIdentity sameEverything = stored;
        const OutputIdentity otherConnector{QStringLiteral("HDMI-A-1"), QStringLiteral("DEL"), QStringLiteral("U2720Q"), QStringLiteral("ABC123")};
        const OutputIdentity otherMonitorSamePort{QStringLiteral("DP-1"), QStringLiteral("SAM"), QStringLiteral("C27"), QStringLiteral("X")};
        const OutputIdentity twinMonitor{QStringLiteral("DP-2"), QStringLiteral("DEL"), QStringLiteral("U2720Q"), QStringLiteral("XYZ999")};
        QCOMPARE(outputMatchScore(stored, sameEverything), 4);
        QCOMPARE(outputMatchScore(stored, otherConnector), 3);
        QCOMPARE(outputMatchScore(stored, otherMonitorSamePort), 0);
        QCOMPARE(outputMatchScore(stored, twinMonitor), 0);
        QCOMPARE(bestOutputMatch(stored, {otherMonitorSamePort, otherConnector, twinMonitor}), 1);
        QCOMPARE(bestOutputMatch(stored, {otherMonitorSamePort, twinMonitor}), -1);
        // Virtual outputs / outputs without EDID: the connector decides.
        const OutputIdentity noEdid{QStringLiteral("Virtual-1"), {}, {}, {}};
        QCOMPARE(outputMatchScore(noEdid, OutputIdentity{QStringLiteral("Virtual-1"), {}, {}, {}}), 1);
        QCOMPARE(outputMatchScore(noEdid, OutputIdentity{QStringLiteral("Virtual-0"), {}, {}, {}}), 0);
    }

    void stackOfWindows()
    {
        PlacementMemory memory;
        memory.remember(placement(QStringLiteral("konsole"), 100), 0);
        memory.remember(placement(QStringLiteral("konsole"), 200), 0);
        quint64 first = 0;
        quint64 second = 0;
        quint64 third = 0;
        // The most recent placement first, then the older one, then nothing.
        QCOMPARE(memory.take(QStringLiteral("konsole"), &first)->canvasPos.x(), 200.0);
        QCOMPARE(memory.take(QStringLiteral("konsole"), &second)->canvasPos.x(), 100.0);
        QVERIFY(!memory.take(QStringLiteral("konsole"), &third));
        // The first window closes at a new place: its entry is replaced and free again.
        memory.remember(placement(QStringLiteral("konsole"), 300), first);
        QCOMPARE(memory.placementCount(QStringLiteral("konsole")), 2);
        QCOMPARE(memory.take(QStringLiteral("konsole"), &third)->canvasPos.x(), 300.0);
        // Released entries can be taken again.
        memory.release(third);
        QCOMPARE(memory.take(QStringLiteral("konsole"), &third)->canvasPos.x(), 300.0);
        QVERIFY(!memory.take(QStringLiteral("dolphin"), &third));
    }

    void limits()
    {
        PlacementMemory memory;
        for (int i = 0; i < PlacementMemory::MaxPerApplication + 5; ++i) {
            memory.remember(placement(QStringLiteral("kate"), i), 0);
        }
        QCOMPARE(memory.placementCount(QStringLiteral("kate")), PlacementMemory::MaxPerApplication);
        for (int i = 0; i < PlacementMemory::MaxApplications + 20; ++i) {
            memory.remember(placement(QStringLiteral("app%1").arg(i), 0, i + 10), 0);
        }
        QCOMPARE(memory.applicationCount(), PlacementMemory::MaxApplications);
        // The oldest applications were dropped first.
        QCOMPARE(memory.placementCount(QStringLiteral("kate")), 0);
        QVERIFY(memory.placementCount(QStringLiteral("app%1").arg(PlacementMemory::MaxApplications + 19)) > 0);
    }

    void jsonRoundTrip()
    {
        PlacementMemory memory;
        WindowPlacement p = placement(QStringLiteral("org.kde.dolphin"), 42);
        p.maximized = true;
        p.pinned = true;
        p.keepAbove = true;
        p.desktopId = QStringLiteral("uuid-1");
        memory.remember(p, 0);
        const QByteArray json = QJsonDocument(memory.toJson()).toJson();

        PlacementMemory restored;
        restored.fromJson(QJsonDocument::fromJson(json).object());
        quint64 id = 0;
        const auto back = restored.take(QStringLiteral("org.kde.dolphin"), &id);
        QVERIFY(back);
        QCOMPARE(back->canvasPos, p.canvasPos);
        QCOMPARE(back->screenOffset, p.screenOffset);
        QCOMPARE(back->size, p.size);
        QCOMPARE(back->output.serial, QStringLiteral("ABC123"));
        QVERIFY(back->maximized && back->pinned && back->keepAbove);
        QCOMPARE(back->desktopId, QStringLiteral("uuid-1"));
    }

    void forgetOneApplication()
    {
        PlacementMemory memory;
        memory.remember(placement(QStringLiteral("konsole"), 100), 0);
        memory.remember(placement(QStringLiteral("konsole"), 200), 0);
        memory.remember(placement(QStringLiteral("dolphin"), 300), 0);
        QVERIFY(memory.forget(QStringLiteral("konsole")));
        QVERIFY(!memory.forget(QStringLiteral("konsole")));
        QCOMPARE(memory.applicationCount(), 1);
        quint64 id = 0;
        QVERIFY(!memory.take(QStringLiteral("konsole"), &id));
        QVERIFY(memory.take(QStringLiteral("dolphin"), &id));
        // A window that had taken a forgotten entry is remembered again when it closes.
        memory.remember(placement(QStringLiteral("konsole"), 400), 12345);
        QCOMPARE(memory.placementCount(QStringLiteral("konsole")), 1);
    }

    void brokenFilesAreIgnored()
    {
        PlacementMemory memory;
        memory.fromJson(QJsonDocument::fromJson(R"({"applications":{"evil":[{"app":"evil","canvas":{"x":1e300,"y":0},"width":10,"height":10},
                                                    {"app":"other","canvas":{"x":1,"y":2}},
                                                    {"app":"evil","canvas":{"x":5,"y":6},"width":-3,"height":10}],
                                                  "fine":[{"app":"fine","canvas":{"x":5,"y":6},"screen":{"x":1,"y":1},"width":300,"height":200}]}})")
                            .object());
        quint64 id = 0;
        QVERIFY(!memory.take(QStringLiteral("evil"), &id));
        QVERIFY(memory.take(QStringLiteral("fine"), &id));
    }
};

QTEST_GUILESS_MAIN(TestPlacementMemory)
#include "test_placementmemory.moc"
