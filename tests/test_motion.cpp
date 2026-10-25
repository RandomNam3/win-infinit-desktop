/*
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "motion.h"

#include <QTest>

using namespace InfinitDesktop;
using namespace std::chrono_literals;

class TestMotion : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void transition()
    {
        const TimePoint start = Clock::now();
        CameraMotion motion;
        const Camera from{QPointF(0, 0), 1.0};
        const Camera to{QPointF(400, 300), 0.5};
        motion.startTransition(from, to, 200ms, start);
        QVERIFY(motion.isActive());
        QCOMPARE(motion.kind(), CameraMotion::Kind::Transition);
        QVERIFY(motion.sample(start).fuzzyEquals(from));
        QVERIFY(!motion.isFinished(start + 100ms));
        const Camera half = motion.sample(start + 100ms);
        QVERIFY(half.center.x() > 0 && half.center.x() < 400);
        QVERIFY(motion.isFinished(start + 200ms));
        QVERIFY(motion.sample(start + 250ms).fuzzyEquals(to));
        QVERIFY(motion.target().fuzzyEquals(to));
    }

    void zeroDurationTransition()
    {
        const TimePoint start = Clock::now();
        CameraMotion motion;
        const Camera to{QPointF(1, 2), 0.3};
        motion.startTransition(Camera{}, to, 0ms, start);
        QVERIFY(motion.isFinished(start));
        QVERIFY(motion.sample(start).fuzzyEquals(to));
    }

    void glide()
    {
        const TimePoint start = Clock::now();
        CameraMotion motion;
        const Camera from{QPointF(0, 0), 1.0};
        QVERIFY(motion.startGlide(from, QPointF(2000, 0), 5.0, start));
        QCOMPARE(motion.kind(), CameraMotion::Kind::Glide);
        // The glide only moves forward and slows down.
        qreal previousX = 0;
        qreal previousStep = std::numeric_limits<qreal>::max();
        for (int ms = 16; ms <= 1000; ms += 16) {
            const qreal x = motion.sample(start + std::chrono::milliseconds(ms)).center.x();
            QVERIFY(x >= previousX);
            const qreal step = x - previousX;
            QVERIFY(step <= previousStep + 1e-9);
            previousStep = step;
            previousX = x;
        }
        // Resting point: v / k = 400, the glide stops a little before reaching it.
        const qreal rest = motion.target().center.x();
        QVERIFY(rest > 390 && rest <= 400);
        QVERIFY(motion.isFinished(start + 5s));
        QCOMPARE(motion.sample(start + 5s).center.x(), rest);
    }

    void tinyGlideIsIgnored()
    {
        CameraMotion motion;
        QVERIFY(!motion.startGlide(Camera{}, QPointF(5, 5), 5.0, Clock::now()));
        QVERIFY(!motion.isActive());
        QVERIFY(!motion.startGlide(Camera{}, QPointF(std::numeric_limits<qreal>::infinity(), 0), 5.0, Clock::now()));
    }

    void easing()
    {
        QCOMPARE(easeOutCubic(0), 0.0);
        QCOMPARE(easeOutCubic(1), 1.0);
        QCOMPARE(easeOutCubic(2), 1.0);
        QCOMPARE(easeInOutCubic(0.5), 0.5);
        QVERIFY(easeOutCubic(0.5) > 0.5);
    }
};

QTEST_GUILESS_MAIN(TestMotion)
#include "test_motion.moc"
