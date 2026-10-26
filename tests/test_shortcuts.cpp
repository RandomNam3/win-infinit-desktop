/*
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "settings.h"
#include "shortcuts.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <windows.h>

using namespace Wid;

class TestShortcuts : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parseAndFormat_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<unsigned>("modifiers");
        QTest::addColumn<unsigned>("vk");
        QTest::addColumn<QString>("formatted");
        QTest::newRow("letter") << QStringLiteral("Win+Alt+H") << unsigned(MOD_WIN | MOD_ALT) << unsigned('H') << QStringLiteral("Win+Alt+H");
        QTest::newRow("order and case") << QStringLiteral("alt+WIN+h") << unsigned(MOD_WIN | MOD_ALT) << unsigned('H') << QStringLiteral("Win+Alt+H");
        QTest::newRow("meta alias") << QStringLiteral("Meta+Alt+0") << unsigned(MOD_WIN | MOD_ALT) << unsigned('0') << QStringLiteral("Win+Alt+0");
        QTest::newRow("equal") << QStringLiteral("Win+Alt+=") << unsigned(MOD_WIN | MOD_ALT) << unsigned(VK_OEM_PLUS) << QStringLiteral("Win+Alt+=");
        QTest::newRow("trailing plus") << QStringLiteral("Win+Alt++") << unsigned(MOD_WIN | MOD_ALT) << unsigned(VK_ADD) << QStringLiteral("Win+Alt+Num+");
        QTest::newRow("numpad plus") << QStringLiteral("Win+Alt+Num+") << unsigned(MOD_WIN | MOD_ALT) << unsigned(VK_ADD) << QStringLiteral("Win+Alt+Num+");
        QTest::newRow("arrows") << QStringLiteral("Win+Ctrl+Alt+Left") << unsigned(MOD_WIN | MOD_CONTROL | MOD_ALT) << unsigned(VK_LEFT)
                                << QStringLiteral("Win+Ctrl+Alt+Left");
        QTest::newRow("function key") << QStringLiteral("Shift+F12") << unsigned(MOD_SHIFT) << unsigned(VK_F12) << QStringLiteral("Shift+F12");
        QTest::newRow("german") << QStringLiteral("Strg+Umschalt+Entf") << 0u << 0u << QString();
        QTest::newRow("modifier only") << QStringLiteral("Win+Alt") << 0u << 0u << QString();
        QTest::newRow("empty") << QString() << 0u << 0u << QString();
    }

    void parseAndFormat()
    {
        QFETCH(QString, text);
        QFETCH(unsigned, modifiers);
        QFETCH(unsigned, vk);
        QFETCH(QString, formatted);
        const std::optional<KeyCombo> combo = parseKeyCombo(text);
        if (vk == 0) {
            QVERIFY(!combo);
            return;
        }
        QVERIFY(combo);
        QCOMPARE(combo->modifiers, modifiers);
        QCOMPARE(combo->vk, vk);
        QCOMPARE(formatKeyCombo(*combo), formatted);
        QCOMPARE(parseKeyCombo(formatted), combo);
    }

    void defaultsMatchKde()
    {
        // Every KDE action is there, with Meta + Alt turned into Win + Alt.
        QCOMPARE(shortcutDefinitions().size(), 40);
        const ShortcutMap map = ShortcutMap::defaults();
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_ALT, 'H'}), std::optional(ShortcutAction::Home));
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_ALT | MOD_SHIFT, 'T'}), std::optional(ShortcutAction::TogglePinAbove));
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_CONTROL | MOD_ALT, '3'}), std::optional(ShortcutAction::SaveBookmark3));
        QVERIFY(!map.actionFor(KeyCombo{MOD_WIN, 'H'}));
        QVERIFY(map.combosFor(ShortcutAction::ToggleEnabled).isEmpty());
        // Notes, groups, areas and the map.
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_ALT, 'N'}), std::optional(ShortcutAction::NewNote));
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_ALT | MOD_SHIFT, 'N'}), std::optional(ShortcutAction::ToggleNotes));
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_ALT, 'M'}), std::optional(ShortcutAction::ShowMap));
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_ALT, 'G'}), std::optional(ShortcutAction::GroupFromSelection));
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_ALT, 'B'}), std::optional(ShortcutAction::AreaFromSelection));
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_ALT | MOD_SHIFT, 'G'}), std::optional(ShortcutAction::SaveGroupLayout));
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_ALT, VK_F1}), std::optional(ShortcutAction::OpenGroup1));
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_ALT, VK_F9}), std::optional(ShortcutAction::OpenGroup9));
        QCOMPARE(QLatin1String(findShortcut(ShortcutAction::OpenGroup9)->id), QLatin1String("InfiniteDesktop OpenGroup9"));
        QCOMPARE(int(ShortcutAction::OpenGroup9) - int(ShortcutAction::OpenGroup1) + 1, GroupShortcutCount);
    }

    void loadOverrides()
    {
        QTemporaryDir dir;
        QSettings file(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        file.beginGroup(QStringLiteral("Shortcuts"));
        file.setValue(QStringLiteral("InfiniteDesktop Home"), QStringLiteral("Ctrl+Alt+H; nonsense"));
        file.setValue(QStringLiteral("InfiniteDesktop ZoomIn"), QString());
        file.setValue(QStringLiteral("InfiniteDesktop ToggleEnabled"), QStringLiteral("Win+Alt+P"));
        // Bound twice: the first action in the table keeps it.
        file.setValue(QStringLiteral("InfiniteDesktop CenterWindow"), QStringLiteral("Ctrl+Alt+H"));
        file.endGroup();
        file.sync();
        const ShortcutMap map = ShortcutMap::load(file);
        QCOMPARE(map.actionFor(KeyCombo{MOD_CONTROL | MOD_ALT, 'H'}), std::optional(ShortcutAction::Home));
        QVERIFY(!map.actionFor(KeyCombo{MOD_WIN | MOD_ALT, 'H'}));
        QVERIFY(map.combosFor(ShortcutAction::ZoomIn).isEmpty());
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_ALT, 'P'}), std::optional(ShortcutAction::ToggleEnabled));
        QVERIFY(map.combosFor(ShortcutAction::CenterWindow).isEmpty());
        // Untouched actions keep their defaults.
        QCOMPARE(map.actionFor(KeyCombo{MOD_WIN | MOD_ALT, 'W'}), std::optional(ShortcutAction::Overview));
    }

    void settingsFromIni()
    {
        QTemporaryDir dir;
        QSettings file(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        file.beginGroup(QLatin1String(SettingsGroup));
        file.setValue(QStringLiteral("PanModifier"), QStringLiteral("CtrlAlt"));
        file.setValue(QStringLiteral("ZoomModifier"), QStringLiteral("Disabled"));
        file.setValue(QStringLiteral("ZoomStep"), 500); // clamped to 100
        file.setValue(QStringLiteral("MinZoom"), 1); // clamped to 2
        file.setValue(QStringLiteral("Momentum"), QStringLiteral("false"));
        file.setValue(QStringLiteral("RevealMode"), QStringLiteral("Center"));
        file.setValue(QStringLiteral("OffscreenMarkers"), QStringLiteral("Always"));
        file.setValue(QStringLiteral("GridColor"), QStringLiteral("10,20,30"));
        file.setValue(QStringLiteral("ExcludedWindowClasses"), QStringLiteral("Foo, BAR ,"));
        file.endGroup();
        file.sync();
        const Settings settings = Settings::load(file);
        QCOMPARE(settings.panModifier, GestureModifier::CtrlAlt);
        QCOMPARE(modifierBits(settings.panModifier), unsigned(MOD_CONTROL | MOD_ALT));
        QCOMPARE(settings.zoomModifier, GestureModifier::Disabled);
        QCOMPARE(modifierBits(settings.zoomModifier), 0u);
        QCOMPARE(settings.zoomStep, 2.0);
        QCOMPARE(settings.minZoom, 0.02);
        QVERIFY(!settings.momentum);
        QCOMPARE(settings.revealMode, InfiniteDesktop::RevealMode::Center);
        QCOMPARE(settings.offscreenMarkers, ShowMode::Always);
        QCOMPARE(settings.gridColor, QColor(10, 20, 30));
        QCOMPARE(settings.excludedClasses, (QStringList{QStringLiteral("foo"), QStringLiteral("bar")}));
        // Defaults as in the KDE schema.
        QVERIFY(settings.dragDesktopToPan);
        QCOMPARE(settings.flingThreshold, 1600.0);
        QCOMPARE(settings.animationDuration, std::chrono::milliseconds(280));
        QVERIFY(settings.notesEnabled);
        QCOMPARE(settings.noteColor, NoteColor::Yellow);
        QVERIFY(!settings.groupLayoutAutomatic);
        QVERIFY(settings.groupStartsApplications);
        QVERIFY(settings.groupZoomToFit);
        QCOMPARE(settings.showAreas, ShowMode::Always);
        QCOMPARE(settings.areaPadding, 48);
        QCOMPARE(settings.mapSize, 70);
    }

    void organizerSettingsFromIni()
    {
        QTemporaryDir dir;
        QSettings file(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        file.beginGroup(QLatin1String(SettingsGroup));
        file.setValue(QStringLiteral("NotesEnabled"), QStringLiteral("false"));
        file.setValue(QStringLiteral("NoteColor"), QStringLiteral("Pink"));
        file.setValue(QStringLiteral("GroupLayoutMode"), QStringLiteral("Automatic"));
        file.setValue(QStringLiteral("GroupStartsApplications"), QStringLiteral("false"));
        file.setValue(QStringLiteral("GroupZoomToFit"), QStringLiteral("false"));
        file.setValue(QStringLiteral("ShowAreas"), QStringLiteral("Never"));
        file.setValue(QStringLiteral("AreaPadding"), 900); // clamped to 400
        file.setValue(QStringLiteral("MapSize"), 10); // clamped to 30
        file.endGroup();
        file.sync();
        const Settings settings = Settings::load(file);
        QVERIFY(!settings.notesEnabled);
        QCOMPARE(settings.noteColor, NoteColor::Pink);
        QVERIFY(settings.groupLayoutAutomatic);
        QVERIFY(!settings.groupStartsApplications);
        QVERIFY(!settings.groupZoomToFit);
        QCOMPARE(settings.showAreas, ShowMode::Never);
        QCOMPARE(settings.areaPadding, 400);
        QCOMPARE(settings.mapSize, 30);
    }
};

QTEST_GUILESS_MAIN(TestShortcuts)

#include "test_shortcuts.moc"
