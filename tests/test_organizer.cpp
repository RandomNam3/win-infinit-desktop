/*
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "organizer.h"

#include <QTest>

using namespace Wid;
using Item = CanvasMenuItem;

class TestOrganizer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void menuItems()
    {
        // As in KDE Infinit Desktop: notes first, then selections, the map, the view.
        QCOMPARE(canvasMenuItems(true, false, false),
                 (QList<Item>{Item::NewNote, Item::ToggleNotes, Item::Separator, Item::NewGroup, Item::NewArea, Item::Map, Item::Separator, Item::ShowAll,
                              Item::Settings, Item::Quit}));
        // Notes switched off, over an area, zoomed out.
        QCOMPARE(canvasMenuItems(false, true, true),
                 (QList<Item>{Item::NewGroup, Item::NewArea, Item::RemoveArea, Item::Map, Item::Separator, Item::ShowAll, Item::ZoomReset, Item::Settings,
                              Item::Quit}));
    }

    void launchCommands()
    {
        QVERIFY(launchCommand(QString()).file.empty());
        QVERIFY(launchCommand(QStringLiteral("   ")).file.empty());
        LaunchCommand path = launchCommand(QStringLiteral("C:/Program Files/Mozilla Thunderbird/thunderbird.exe"));
        QVERIFY(path.isPath);
        QCOMPARE(QString::fromStdWString(path.file), QStringLiteral("C:\\Program Files\\Mozilla Thunderbird\\thunderbird.exe"));
        path = launchCommand(QStringLiteral("notepad.EXE"));
        QVERIFY(path.isPath);
        const LaunchCommand app = launchCommand(QStringLiteral("Microsoft.WindowsCalculator_8wekyb3d8bbwe!App"));
        QVERIFY(!app.isPath);
        QCOMPARE(QString::fromStdWString(app.file), QStringLiteral("shell:AppsFolder\\Microsoft.WindowsCalculator_8wekyb3d8bbwe!App"));
    }

    void handles()
    {
        QCOMPARE(parseHandle(QStringLiteral("1234")), std::optional<quintptr>(1234));
        QCOMPARE(parseHandle(QStringLiteral(" 0x1A2b ")), std::optional<quintptr>(0x1a2b));
        QVERIFY(!parseHandle(QStringLiteral("0")));
        QVERIFY(!parseHandle(QStringLiteral("-5")));
        QVERIFY(!parseHandle(QStringLiteral("12ab")));
        QVERIFY(!parseHandle(QString()));
    }

    void noteIds()
    {
        QVERIFY(isValidNoteId(QStringLiteral("note-20261024-abc_1")));
        QVERIFY(!isValidNoteId(QString()));
        QVERIFY(!isValidNoteId(QString(65, QLatin1Char('a'))));
        QVERIFY(!isValidNoteId(QStringLiteral("../evil")));
        QVERIFY(!isValidNoteId(QStringLiteral("a b")));
    }
};

QTEST_GUILESS_MAIN(TestOrganizer)

#include "test_organizer.moc"
