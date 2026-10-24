/*
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "areas.h"
#include "minimap.h"
#include "noteplacements.h"

#include <QJsonDocument>
#include <QTest>

using namespace InfinitDesktop;

class TestAreas : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void dynamicRect()
    {
        Area area;
        area.drawnRect = QRectF(0, 0, 1000, 800);
        const AreaMetrics metrics{50, QSizeF(400, 300)};
        // Without windows: as drawn.
        QCOMPARE(dynamicAreaRect(area, {}, metrics), area.drawnRect);
        // Grows to cover a window dragged in beyond its border, shrinks when it gets smaller.
        QCOMPARE(dynamicAreaRect(area, {QRectF(100, 100, 1500, 500)}, metrics), QRectF(50, 50, 1600, 600));
        QCOMPARE(dynamicAreaRect(area, {QRectF(100, 100, 100, 100)}, metrics), QRectF(-50, 0, 400, 300));
        // A fixed area keeps its rect.
        area.dynamic = false;
        QCOMPARE(dynamicAreaRect(area, {QRectF(100, 100, 1500, 500)}, metrics), area.drawnRect);
    }

    void membership()
    {
        QList<Area> areas(2);
        areas[0].rect = QRectF(0, 0, 2000, 2000);
        areas[1].rect = QRectF(100, 100, 500, 500);
        // Nested: the smaller one wins.
        QCOMPARE(areaForWindow(areas, QRectF(200, 200, 100, 100)), 1);
        QCOMPARE(areaForWindow(areas, QRectF(1000, 1000, 100, 100)), 0);
        QCOMPARE(areaForWindow(areas, QRectF(5000, 0, 100, 100)), -1);
        QCOMPARE(areaAt(areas, QPointF(150, 150)), 1);
    }

    void storeRoundTrip()
    {
        AreaStore store;
        Area area;
        area.name = store.nextName(QStringLiteral("Bereich %1"));
        area.drawnRect = QRectF(QPointF(500, 400), QPointF(0, 0)); // drawn backwards
        Area *added = store.add(area);
        QVERIFY(added);
        QCOMPARE(added->drawnRect, QRectF(0, 0, 500, 400));
        QCOMPARE(added->rect, added->drawnRect);
        QCOMPARE(added->name, QStringLiteral("Bereich 1"));
        Area empty;
        QVERIFY(!store.add(empty));

        AreaStore loaded;
        loaded.fromJson(QJsonDocument::fromJson(QJsonDocument(store.toJson()).toJson()).object());
        QCOMPARE(loaded.areas().size(), 1);
        QCOMPARE(loaded.areas()[0].drawnRect, QRectF(0, 0, 500, 400));
        QVERIFY(loaded.areas()[0].dynamic);
        QVERIFY(loaded.remove(loaded.areas()[0].id));
    }

    void notePlacements()
    {
        NotePlacements notes;
        NotePlacement placement;
        placement.id = QStringLiteral("n1");
        placement.canvasPos = QPointF(10, 20);
        placement.pinned = true;
        QVERIFY(notes.store(placement));
        placement.id = QStringLiteral("n2");
        QVERIFY(notes.store(placement));
        NotePlacement broken;
        broken.id = QStringLiteral("n3");
        broken.canvasPos = QPointF(qInf(), 0);
        // JSON would turn infinity into null and that into 0: rejected right away.
        QVERIFY(!notes.store(broken));

        NotePlacements loaded;
        loaded.fromJson(QJsonDocument::fromJson(QJsonDocument(notes.toJson()).toJson()).object());
        QCOMPARE(loaded.count(), 2);
        QVERIFY(loaded.find(QStringLiteral("n1"))->pinned);
        QCOMPARE(loaded.retain({QStringLiteral("n2")}), 1);
        QVERIFY(!loaded.find(QStringLiteral("n1")));
    }

    void mapViewFitsEverything()
    {
        const QRectF visible(0, 0, 1920, 1080);
        const QList<QRectF> windows = {QRectF(-4000, 0, 800, 600), QRectF(3000, 2000, 800, 600)};
        const MapView view = mapView(windows, visible, QRectF(0, 0, 1000, 600), 20, 1.0, QSizeF(4000, 3000));
        QVERIFY(view.isValid());
        QCOMPARE(view.screen, QRectF(20, 20, 960, 560));
        for (const QRectF &window : windows) {
            QVERIFY(view.screen.contains(view.toScreen(window)));
        }
        QVERIFY(view.screen.contains(view.toScreen(visible)));
        // Round trip.
        const QPointF point(123, 456);
        const QPointF back = view.toCanvas(view.toScreen(point));
        QVERIFY(qAbs(back.x() - point.x()) < 1e-6 && qAbs(back.y() - point.y()) < 1e-6);
        // Dragging the viewport by 10 map pixels moves the camera by 10 / scale.
        QCOMPARE(mapDragCenter(view, QPointF(0, 0), QPointF(10, 0)), QPointF(10 / view.scale, 0));
    }

    void mapViewOfEmptyCanvas()
    {
        const MapView view = mapView({}, QRectF(0, 0, 100, 100), QRectF(0, 0, 800, 800), 0, 0.5, QSizeF(4000, 4000));
        QVERIFY(view.isValid());
        QVERIFY(view.scale <= 0.5);
        QVERIFY(view.canvas.width() >= 4000);
        QVERIFY(!mapView({}, QRectF(), QRectF(0, 0, 800, 800), 0, 1, QSizeF()).isValid());
    }
};

QTEST_GUILESS_MAIN(TestAreas)

#include "test_areas.moc"
