/*
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "navigation.h"

#include <QTest>

using namespace InfinitDesktop;

class TestNavigation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void neighbors()
    {
        const QRectF current(0, 0, 100, 100);
        const QList<QRectF> candidates{
            QRectF(300, 0, 100, 100), // right
            QRectF(-300, 20, 100, 100), // left
            QRectF(0, -400, 100, 100), // up
            QRectF(10, 250, 100, 100), // down
            QRectF(150, 900, 100, 100), // far down right
            current,
        };
        QCOMPARE(findNeighbor(current, candidates, Direction::Right), 0);
        QCOMPARE(findNeighbor(current, candidates, Direction::Left), 1);
        QCOMPARE(findNeighbor(current, candidates, Direction::Up), 2);
        QCOMPARE(findNeighbor(current, candidates, Direction::Down), 3);
        QCOMPARE(findNeighbor(current, {current}, Direction::Down), -1);
    }

    void neighborPrefersCone()
    {
        const QRectF current(0, 0, 100, 100);
        const QList<QRectF> candidates{
            QRectF(200, 600, 100, 100), // right but far below: outside the cone
            QRectF(700, 0, 100, 100), // further right but straight ahead
        };
        QCOMPARE(findNeighbor(current, candidates, Direction::Right), 1);
    }

    void fit()
    {
        const QRectF viewport(0, 0, 1920, 1080);
        const QRectF area(0, 0, 1920, 1040); // panel at the bottom
        const QRectF content(-2000, -1000, 6000, 3000);
        const Camera camera = fitCamera(content, viewport, area, 40, 0.05, 1.0);
        QVERIFY(camera.zoom < 1.0);
        QVERIFY(isFullyVisible(camera, content, viewport, area));
        // Small content is not magnified beyond the maximum.
        const Camera small = fitCamera(QRectF(0, 0, 100, 100), viewport, area, 40, 0.05, 1.0);
        QCOMPARE(small.zoom, 1.0);
        QVERIFY(isFullyVisible(small, QRectF(0, 0, 100, 100), viewport, area));
        // Clamped at the minimum.
        const Camera huge = fitCamera(QRectF(0, 0, 1e7, 1e7), viewport, area, 40, 0.05, 1.0);
        QCOMPARE(huge.zoom, 0.05);
    }

    void revealMinimal()
    {
        const QRectF viewport(1920, 0, 1920, 1080);
        const QRectF area = viewport;
        const Camera camera{QPointF(0, 0), 1.0};
        // Window just off the right edge.
        const QRectF window(800, -100, 400, 300);
        QVERIFY(!isFullyVisible(camera, window, viewport, area));
        const Camera revealed = revealCamera(camera, window, viewport, area, RevealMode::Minimal, 20);
        QVERIFY(isFullyVisible(revealed, window, viewport, area));
        // Only the horizontal position changes.
        QCOMPARE(revealed.center.y(), camera.center.y());
        // An already visible window does not move the camera.
        const QRectF visible(-100, -100, 200, 200);
        QCOMPARE(revealCamera(camera, visible, viewport, area, RevealMode::Minimal, 20).center, camera.center);
    }

    void revealLargeWindowShowsTopLeft()
    {
        const QRectF viewport(0, 0, 1000, 800);
        const Camera camera{QPointF(0, 0), 1.0};
        const QRectF window(5000, 5000, 3000, 2000);
        const Camera revealed = revealCamera(camera, window, viewport, viewport, RevealMode::Minimal, 0);
        QCOMPARE(revealed.canvasToScreen(window.topLeft(), viewport), viewport.topLeft());
    }

    void revealCenterFromZoomedOut()
    {
        const QRectF viewport(0, 0, 1000, 800);
        const QRectF area(0, 0, 1000, 760);
        const Camera camera{QPointF(0, 0), 0.2};
        const QRectF window(3000, 3000, 400, 300);
        const Camera revealed = revealCamera(camera, window, viewport, area, RevealMode::Center, 0, 1.0);
        QCOMPARE(revealed.zoom, 1.0);
        QCOMPARE(revealed.canvasToScreen(window.center(), viewport), area.center());
    }

    void freeTileSpot()
    {
        const QSizeF size(200, 120);
        const QList<QRectF> occupied{QRectF(-100, -60, 200, 120)};
        const QPointF spot = findFreeTileSpot(QPointF(0, 0), size, occupied, 20);
        QVERIFY(!QRectF(spot, size).intersects(occupied.first()));
        QCOMPARE(findFreeTileSpot(QPointF(1000, 1000), size, occupied, 20), QPointF(900, 940));
    }

    void bounding()
    {
        QCOMPARE(boundingRect({}), QRectF());
        QCOMPARE(boundingRect({QRectF(0, 0, 10, 10), QRectF(-10, 5, 5, 20)}), QRectF(-10, 0, 20, 25));
    }
};

QTEST_GUILESS_MAIN(TestNavigation)
#include "test_navigation.moc"
