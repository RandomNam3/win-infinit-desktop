/*
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "camera.h"

#include <QTest>

using namespace InfiniteDesktop;

class TestCamera : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTrip_data()
    {
        QTest::addColumn<QPointF>("center");
        QTest::addColumn<qreal>("zoom");
        QTest::addColumn<QRectF>("viewport");
        QTest::addColumn<QPointF>("canvasPoint");
        QTest::newRow("identity") << QPointF(0, 0) << 1.0 << QRectF(0, 0, 1920, 1080) << QPointF(100, 200);
        QTest::newRow("second screen") << QPointF(-500, 300) << 1.0 << QRectF(1920, 0, 2560, 1440) << QPointF(-3000, 99);
        QTest::newRow("zoomed out") << QPointF(1000, -1000) << 0.25 << QRectF(0, 0, 1920, 1080) << QPointF(5000, 5000);
        QTest::newRow("magnified") << QPointF(12.5, 7.25) << 2.5 << QRectF(-1280, 0, 1280, 1024) << QPointF(-17.75, 3.0);
    }

    void roundTrip()
    {
        QFETCH(QPointF, center);
        QFETCH(qreal, zoom);
        QFETCH(QRectF, viewport);
        QFETCH(QPointF, canvasPoint);
        const Camera camera{center, zoom};
        const QPointF screen = camera.canvasToScreen(canvasPoint, viewport);
        const QPointF back = camera.screenToCanvas(screen, viewport);
        QVERIFY(std::abs(back.x() - canvasPoint.x()) < 1e-9);
        QVERIFY(std::abs(back.y() - canvasPoint.y()) < 1e-9);
    }

    void centerIsViewportCenter()
    {
        const Camera camera{QPointF(42, -7), 0.5};
        const QRectF viewport(1920, 0, 1920, 1080);
        QCOMPARE(camera.canvasToScreen(camera.center, viewport), viewport.center());
    }

    void unitZoomIsTranslation()
    {
        const Camera camera{QPointF(100, 100), 1.0};
        const QRectF viewport(0, 0, 1000, 800);
        const QRectF window(100, 100, 300, 200);
        // At zoom 1 the canvas maps 1:1, only shifted.
        QCOMPARE(camera.canvasToScreen(window, viewport), QRectF(500, 400, 300, 200));
    }

    void zoomKeepsAnchor()
    {
        const QRectF viewport(0, 0, 1920, 1080);
        const Camera camera{QPointF(300, 200), 1.0};
        const QPointF anchor(1500, 100);
        const QPointF canvasUnderAnchor = camera.screenToCanvas(anchor, viewport);
        for (qreal zoom : {0.1, 0.5, 0.999, 2.0}) {
            const Camera zoomed = camera.zoomedAt(anchor, zoom, viewport);
            QCOMPARE(zoomed.zoom, zoom);
            const QPointF after = zoomed.canvasToScreen(canvasUnderAnchor, viewport);
            QVERIFY2(std::abs(after.x() - anchor.x()) < 1e-6 && std::abs(after.y() - anchor.y()) < 1e-6, "anchor moved");
        }
    }

    void pan()
    {
        const Camera camera{QPointF(0, 0), 0.5};
        const Camera panned = camera.pannedByScreenDelta(QPointF(100, -50));
        // Content follows the delta: the camera moves the opposite way, scaled by 1/zoom.
        QCOMPARE(panned.center, QPointF(-200, 100));
        QCOMPARE(panned.zoom, 0.5);
    }

    void interpolateEndpoints()
    {
        const Camera a{QPointF(0, 0), 1.0};
        const Camera b{QPointF(1000, -500), 0.2};
        QVERIFY(interpolate(a, b, 0.0).fuzzyEquals(a));
        QVERIFY(interpolate(a, b, 1.0).fuzzyEquals(b));
        const Camera middle = interpolate(a, b, 0.5);
        // Logarithmic zoom interpolation: geometric mean.
        QVERIFY(std::abs(middle.zoom - std::sqrt(0.2)) < 1e-9);
        QVERIFY(middle.center.x() > 0 && middle.center.x() < 1000);
    }

    void interpolateIsMonotonic()
    {
        const Camera a{QPointF(0, 0), 0.1};
        const Camera b{QPointF(500, 500), 1.0};
        qreal previousX = -1;
        qreal previousZoom = 0;
        for (int i = 0; i <= 20; ++i) {
            const Camera c = interpolate(a, b, i / 20.0);
            QVERIFY(c.center.x() >= previousX);
            QVERIFY(c.zoom >= previousZoom);
            previousX = c.center.x();
            previousZoom = c.zoom;
        }
    }

    void clampAndSnap()
    {
        QCOMPARE(clampZoom(0.01, 0.1, 1.0), 0.1);
        QCOMPARE(clampZoom(5.0, 0.1, 1.0), 1.0);
        QCOMPARE(clampZoom(0.985, 0.1, 1.0), 1.0);
        QCOMPARE(clampZoom(0.97, 0.1, 1.0, 0.0), 0.97);
        QCOMPARE(clampZoom(std::numeric_limits<qreal>::quiet_NaN(), 0.1, 1.0), 1.0);
        QCOMPARE(clampZoom(-1, 0.1, 1.0), 1.0);
        // Swapped limits are tolerated.
        QCOMPARE(clampZoom(0.5, 1.0, 0.1), 0.5);
    }

    void pixelGrid()
    {
        QCOMPARE(snapToPixelGrid(QPointF(10.3, 10.7), 1.0), QPointF(10, 11));
        // With a scale of 1.25 the logical grid has a step of 0.8.
        QCOMPARE(snapToPixelGrid(QPointF(10.3, 0.5), 1.25), QPointF(10.4, 0.8));
        QCOMPARE(snapToPixelGrid(QPointF(1.4, 1.6), 0.0), QPointF(1, 2));
    }
};

QTEST_GUILESS_MAIN(TestCamera)
#include "test_camera.moc"
