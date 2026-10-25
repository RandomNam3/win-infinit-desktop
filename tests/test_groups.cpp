/*
    SPDX-FileCopyrightText: 2026 KDE Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "groups.h"

#include <QJsonDocument>
#include <QTest>

using namespace InfinitDesktop;

class TestGroups : public QObject
{
    Q_OBJECT

    static Group mailGroup()
    {
        Group group;
        group.id = QStringLiteral("mail");
        group.name = QStringLiteral("Mail");
        group.origin = QPointF(1000, 500);
        group.members = {
            GroupMember{QStringLiteral("thunderbird"), QStringLiteral("org.mozilla.thunderbird"), QRectF(0, 0, 800, 600)},
            GroupMember{QStringLiteral("firefox"), QStringLiteral("firefox"), QRectF(820, 0, 700, 600)},
            GroupMember{QStringLiteral("kalendar"), QStringLiteral("org.kde.merkuro.calendar"), QRectF(0, 620, 800, 300)},
        };
        return group;
    }

    static GroupCandidate window(quint64 handle, const QString &app, qreal x, const QString &groupId = QString())
    {
        return GroupCandidate{handle, app, QRectF(x, 0, 400, 300), groupId, true};
    }

private Q_SLOTS:
    void bounds()
    {
        const Group group = mailGroup();
        QCOMPARE(group.layoutBounds(), QRectF(0, 0, 1520, 920));
        QCOMPARE(group.canvasBounds(), QRectF(1000, 500, 1520, 920));
    }

    void planMovesAndLaunches()
    {
        const Group group = mailGroup();
        const GroupPlan plan = planGroup(group, {window(1, QStringLiteral("thunderbird"), -3000), window(2, QStringLiteral("konsole"), 0)});
        QCOMPARE(plan.moves.size(), 1);
        QCOMPARE(plan.moves[0].handle, quint64(1));
        QCOMPARE(plan.moves[0].target, QRectF(1000, 500, 800, 600));
        QCOMPARE(plan.launches, (QList<int>{1, 2}));
        QCOMPARE(plan.bounds, group.canvasBounds());
    }

    void sharedApplicationMovesAlong()
    {
        // The only browser window belongs to another group: it still moves along.
        const GroupPlan plan = planGroup(mailGroup(), {window(7, QStringLiteral("firefox"), 0, QStringLiteral("work"))});
        QCOMPARE(plan.moves.size(), 1);
        QCOMPARE(plan.moves[0].handle, quint64(7));
        QCOMPARE(plan.moves[0].member, 1);
    }

    void prefersTaggedThenNearestUntagged()
    {
        const Group group = mailGroup();
        // Two browser windows: the one tagged with this group wins.
        GroupPlan plan = planGroup(group, {window(1, QStringLiteral("firefox"), 1100), window(2, QStringLiteral("firefox"), -5000, QStringLiteral("mail"))});
        QCOMPARE(plan.moves.size(), 1);
        QCOMPARE(plan.moves[0].handle, quint64(2));

        // None tagged: the nearest untagged one.
        plan = planGroup(group, {window(1, QStringLiteral("firefox"), -5000), window(2, QStringLiteral("firefox"), 1500)});
        QCOMPARE(plan.moves[0].handle, quint64(2));

        // Several windows, all of other groups: start a new one instead of stealing.
        plan = planGroup(group, {window(1, QStringLiteral("firefox"), 0, QStringLiteral("a")), window(2, QStringLiteral("firefox"), 0, QStringLiteral("b"))});
        QVERIFY(plan.moves.isEmpty());
        QVERIFY(plan.launches.contains(1));
    }

    void oneWindowPerMember()
    {
        Group group;
        group.id = QStringLiteral("terminals");
        group.members = {GroupMember{QStringLiteral("konsole"), {}, QRectF(0, 0, 500, 400)}, GroupMember{QStringLiteral("konsole"), {}, QRectF(520, 0, 500, 400)}};
        const GroupPlan plan = planGroup(group, {window(1, QStringLiteral("konsole"), 0)});
        QCOMPARE(plan.moves.size(), 1);
        QCOMPARE(plan.launches, QList<int>{1});
    }

    void unmovableWindowIsUsedButNotMoved()
    {
        GroupCandidate fullscreen = window(1, QStringLiteral("firefox"), 0);
        fullscreen.movable = false;
        const GroupPlan plan = planGroup(mailGroup(), {fullscreen});
        QVERIFY(plan.moves.isEmpty());
        QVERIFY(!plan.launches.contains(1));
    }

    void captureAndFollow()
    {
        Group group = mailGroup();
        // Windows were rearranged and moved; the missing calendar keeps its place.
        captureLayout(group, {QRectF(200, 100, 800, 600), QRectF(1100, 150, 700, 500), QRectF()});
        QCOMPARE(group.origin, QPointF(200, 100));
        QCOMPARE(group.members[0].geometry, QRectF(0, 0, 800, 600));
        QCOMPARE(group.members[1].geometry, QRectF(900, 50, 700, 500));
        QCOMPARE(group.members[2].geometry.translated(group.origin), QRectF(1000, 1120, 800, 300));

        // The whole group was moved by (-500, +300): only the origin follows.
        Group moved = mailGroup();
        followWindows(moved, {QRectF(500, 800, 800, 600), QRectF(1320, 800, 700, 600), QRectF()});
        QCOMPARE(moved.origin, QPointF(500, 800));
        QCOMPARE(moved.members[1].geometry, QRectF(820, 0, 700, 600));

        // One of two windows moved by 200 to the right: the origin moves half of that.
        Group one = mailGroup();
        followWindows(one, {QRectF(1200, 500, 800, 600), QRectF(1820, 500, 700, 600), QRectF()});
        QCOMPARE(one.origin, QPointF(1100, 500));
    }

    void autoArrangeRows()
    {
        const QList<QRectF> rects = autoArrange({QSizeF(100, 50), QSizeF(200, 80), QSizeF(50, 50)}, 2, 10);
        QCOMPARE(rects, (QList<QRectF>{QRectF(0, 0, 100, 50), QRectF(110, 0, 200, 80), QRectF(0, 90, 50, 50)}));
    }

    void insideSelection()
    {
        const QList<QRectF> windows = {QRectF(0, 0, 100, 100), QRectF(500, 500, 100, 100), QRectF(90, 90, 100, 100)};
        // Drawn from bottom right to top left: normalised.
        QCOMPARE(windowsInside(QRectF(QPointF(200, 200), QPointF(0, 0)), windows), (QList<int>{0, 2}));
    }

    void storeRoundTrip()
    {
        GroupStore store;
        Group group = mailGroup();
        group.layoutMode = GroupLayoutMode::Automatic;
        QVERIFY(store.add(group));
        Group second;
        second.id = QStringLiteral("mail"); // taken: gets a new id
        second.name = store.nextName(QStringLiteral("Gruppe %1"));
        QVERIFY(store.add(second));
        QCOMPARE(store.groups().size(), 2);
        QVERIFY(store.groups()[1].id != QLatin1String("mail"));
        QCOMPARE(store.groups()[1].name, QStringLiteral("Gruppe 1"));

        const QByteArray json = QJsonDocument(store.toJson()).toJson();
        GroupStore loaded;
        loaded.fromJson(QJsonDocument::fromJson(json).object());
        QCOMPARE(loaded.groups().size(), 2);
        const Group *mail = loaded.find(QStringLiteral("mail"));
        QVERIFY(mail);
        QCOMPARE(mail->members.size(), 3);
        QCOMPARE(mail->members[1].geometry, QRectF(820, 0, 700, 600));
        QCOMPARE(mail->layoutMode, GroupLayoutMode::Automatic);
        QCOMPARE(mail->origin, QPointF(1000, 500));

        QVERIFY(loaded.move(QStringLiteral("mail"), 1));
        QCOMPARE(loaded.indexOf(QStringLiteral("mail")), 1);
        QVERIFY(loaded.removeApplication(QStringLiteral("firefox")));
        QCOMPARE(loaded.find(QStringLiteral("mail"))->members.size(), 2);
        QVERIFY(loaded.remove(QStringLiteral("mail")));
        QVERIFY(!loaded.remove(QStringLiteral("mail")));
    }

    void rejectsBrokenFiles()
    {
        GroupStore store;
        const QByteArray json = R"({"groups":[
            {"id":"a","name":"ok","x":1,"y":2,"members":[{"app":"x","geometry":{"x":0,"y":0,"width":10,"height":10}},
                                                       {"app":"","geometry":{"x":0,"y":0,"width":10,"height":10}},
                                                       {"app":"y","geometry":{"x":1e300,"y":0,"width":10,"height":10}}]},
            {"id":"a","name":"duplicate"},
            {"name":"no id"},
            {"id":"b","x":1e300,"y":0}
        ]})";
        store.fromJson(QJsonDocument::fromJson(json).object());
        QCOMPARE(store.groups().size(), 1);
        QCOMPARE(store.groups()[0].members.size(), 1);
    }

    void nameWithoutPlaceholder()
    {
        GroupStore store;
        QCOMPARE(store.nextName(QStringLiteral("Gruppe")), QStringLiteral("Gruppe 1"));
    }
};

QTEST_GUILESS_MAIN(TestGroups)

#include "test_groups.moc"
