/*
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "velocitytracker.h"

#include <QTest>

using namespace InfiniteDesktop;
using namespace std::chrono_literals;

class TestVelocityTracker : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void constantVelocity()
    {
        VelocityTracker tracker;
        // 1000 px/s to the right, 500 px/s up, sampled at ~125 Hz.
        for (int i = 0; i <= 20; ++i) {
            const auto t = std::chrono::microseconds(i * 8000);
            tracker.addSample(QPointF(i * 8.0, -i * 4.0), t);
        }
        const QPointF v = tracker.velocity(std::chrono::microseconds(20 * 8000));
        QVERIFY2(std::abs(v.x() - 1000) < 1, qPrintable(QString::number(v.x())));
        QVERIFY2(std::abs(v.y() + 500) < 1, qPrintable(QString::number(v.y())));
    }

    void restingPointerHasNoVelocity()
    {
        VelocityTracker tracker;
        tracker.addSample(QPointF(0, 0), 0us);
        tracker.addSample(QPointF(50, 0), 10ms);
        // Pointer was held still for 200 ms before releasing.
        QCOMPARE(tracker.velocity(210ms), QPointF());
        QVERIFY(tracker.velocity(15ms).x() > 0);
    }

    void needsTwoSamples()
    {
        VelocityTracker tracker;
        QVERIFY(tracker.isEmpty());
        QCOMPARE(tracker.velocity(0us), QPointF());
        tracker.addSample(QPointF(1, 1), 0us);
        QCOMPARE(tracker.velocity(0us), QPointF());
    }

    void onlyRecentSamplesCount()
    {
        VelocityTracker tracker;
        // Slow movement long ago ...
        for (int i = 0; i < 10; ++i) {
            tracker.addSample(QPointF(i, 0), std::chrono::milliseconds(i * 10));
        }
        // ... then a fast flick.
        for (int i = 0; i < 6; ++i) {
            tracker.addSample(QPointF(10 + i * 30.0, 0), std::chrono::milliseconds(500 + i * 10));
        }
        const QPointF v = tracker.velocity(550ms);
        QVERIFY2(v.x() > 2500, qPrintable(QString::number(v.x())));
    }

    void timeGoingBackwardsResets()
    {
        VelocityTracker tracker;
        tracker.addSample(QPointF(0, 0), 100ms);
        tracker.addSample(QPointF(10, 0), 110ms);
        tracker.addSample(QPointF(20, 0), 5ms);
        QCOMPARE(tracker.velocity(5ms), QPointF());
    }

    void straightFlickFlings()
    {
        VelocityTracker tracker;
        for (int i = 0; i <= 40; ++i) {
            tracker.addSample(QPointF(i * 10.0, i * 2.0), std::chrono::milliseconds(i * 8));
        }
        const QPointF v = tracker.flingVelocity(320ms);
        QVERIFY2(v.x() > 1000, qPrintable(QString::number(v.x())));
    }

    void circlingDoesNotFling()
    {
        // One or two circles with the mouse, released while still moving.
        for (const double turnsPerSecond : {1.0, 2.0, 3.0}) {
            VelocityTracker tracker;
            const int samples = 125;
            for (int i = 0; i <= samples; ++i) {
                const double t = i * 0.008;
                const double angle = 2.0 * M_PI * turnsPerSecond * t;
                tracker.addSample(QPointF(500 + 120 * std::cos(angle), 400 + 120 * std::sin(angle)), std::chrono::milliseconds(i * 8));
            }
            QVERIFY(tracker.speed(std::chrono::milliseconds(samples * 8)) > 250);
            QCOMPARE(tracker.flingVelocity(std::chrono::milliseconds(samples * 8)), QPointF());
        }
    }

    void slowReleaseDoesNotFling()
    {
        VelocityTracker tracker;
        for (int i = 0; i <= 40; ++i) {
            tracker.addSample(QPointF(i * 1.0, 0), std::chrono::milliseconds(i * 8));
        }
        QVERIFY(tracker.speed(320ms) > 0);
        QCOMPARE(tracker.flingVelocity(320ms), QPointF());
    }

    void ringBufferWraps()
    {
        VelocityTracker tracker;
        for (int i = 0; i < 200; ++i) {
            tracker.addSample(QPointF(i * 2.0, 0), std::chrono::milliseconds(i));
        }
        const QPointF v = tracker.velocity(199ms);
        QVERIFY(std::abs(v.x() - 2000) < 1);
    }
};

QTEST_GUILESS_MAIN(TestVelocityTracker)
#include "test_velocitytracker.moc"
