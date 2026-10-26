/*
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "parking.h"

#include <QTest>

using namespace InfiniteDesktop;

class TestParking : public QObject
{
    Q_OBJECT

private:
    static void verifySpot(const QList<QRectF> &outputs, int owner, const QSizeF &size, bool expectOwned = true)
    {
        const ParkingSpot spot = computeParkingSpot(outputs, owner, size);
        const QRectF rect(spot.topLeft, size);
        for (const QRectF &output : outputs) {
            QVERIFY2(!output.intersects(rect), "parked window intersects an output");
        }
        if (expectOwned) {
            QVERIFY(spot.ownedByOwner);
            QCOMPARE(nearestOutputIndex(outputs, rect.center()), owner);
        }
    }

private Q_SLOTS:
    void nearestOutputMatchesKWin()
    {
        const QList<QRectF> outputs{QRectF(0, 0, 1920, 1080), QRectF(1920, 0, 1920, 1080)};
        QCOMPARE(nearestOutputIndex(outputs, QPointF(10, 10)), 0);
        QCOMPARE(nearestOutputIndex(outputs, QPointF(1919.5, 10)), 0);
        QCOMPARE(nearestOutputIndex(outputs, QPointF(1920, 10)), 1);
        QCOMPARE(nearestOutputIndex(outputs, QPointF(5000, 500)), 1);
        QCOMPARE(nearestOutputIndex(outputs, QPointF(-5000, 5000)), 0);
        QCOMPARE(nearestOutputIndex({}, QPointF()), -1);
    }

    void singleOutput()
    {
        verifySpot({QRectF(0, 0, 1920, 1080)}, 0, QSizeF(800, 600));
        verifySpot({QRectF(0, 0, 1920, 1080)}, 0, QSizeF(5000, 4000));
    }

    void sideBySide()
    {
        const QList<QRectF> outputs{QRectF(0, 0, 1920, 1080), QRectF(1920, 0, 2560, 1440)};
        verifySpot(outputs, 0, QSizeF(800, 600));
        verifySpot(outputs, 1, QSizeF(800, 600));
        verifySpot(outputs, 0, QSizeF(3000, 2000));
    }

    void stacked()
    {
        const QList<QRectF> outputs{QRectF(0, 0, 1920, 1080), QRectF(0, 1080, 1920, 1080)};
        verifySpot(outputs, 0, QSizeF(1000, 700));
        verifySpot(outputs, 1, QSizeF(1000, 700));
    }

    void surrounded()
    {
        // The middle output has neighbours on all four sides; the spot must
        // still not intersect anything. Ownership cannot always be guaranteed.
        const QList<QRectF> outputs{
            QRectF(1000, 1000, 1000, 1000), // middle
            QRectF(0, 1000, 1000, 1000),
            QRectF(2000, 1000, 1000, 1000),
            QRectF(1000, 0, 1000, 1000),
            QRectF(1000, 2000, 1000, 1000),
        };
        verifySpot(outputs, 0, QSizeF(400, 300), false);
        verifySpot(outputs, 1, QSizeF(400, 300));
    }

    void misaligned()
    {
        const QList<QRectF> outputs{QRectF(0, 300, 1920, 1080), QRectF(1920, 0, 1080, 1920)};
        verifySpot(outputs, 0, QSizeF(1200, 900));
        verifySpot(outputs, 1, QSizeF(1200, 900));
    }

    void invalidInput()
    {
        const ParkingSpot spot = computeParkingSpot({}, 0, QSizeF(10, 10));
        QVERIFY(!spot.ownedByOwner);
        const ParkingSpot spot2 = computeParkingSpot({QRectF(0, 0, 10, 10)}, 3, QSizeF(10, 10));
        QVERIFY(!spot2.ownedByOwner);
    }

    void otherOutputIntersection()
    {
        const QList<QRectF> outputs{QRectF(0, 0, 100, 100), QRectF(100, 0, 100, 100)};
        QVERIFY(intersectsOtherOutput(outputs, 0, QRectF(90, 10, 20, 20)));
        QVERIFY(!intersectsOtherOutput(outputs, 0, QRectF(10, 10, 20, 20)));
        QVERIFY(!intersectsOtherOutput(outputs, 0, QRectF(10, 200, 20, 20)));
    }
};

QTEST_GUILESS_MAIN(TestParking)
#include "test_parking.moc"
