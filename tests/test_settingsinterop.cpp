/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
// What the settings app writes is what the service reads: the settings app's writer
// (settingsschema.h, keycombo.h) against the service's reader (settings.h, shortcuts.h).

#include "keycombo.h"
#include "serviceconnection.h"
#include "settingsfile.h"
#include "settingsschema.h"
#include "shortcutdefs.h"

#include "settings.h"
#include "shortcuts.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace InfiniteDesktop;

class TestSettingsInterop : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_path;

    void write(const QString &key, const QVariant &value)
    {
        SchemaItem *item = SettingsSchema::instance().find(key);
        QVERIFY2(item, qPrintable(key));
        item->setProperty(value);
        item->writeConfig(*SettingsFile::instance());
    }

    Wid::Settings serviceSettings()
    {
        SettingsFile::instance()->flush();
        QSettings file(m_path, QSettings::IniFormat);
        return Wid::Settings::load(file);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("settings.ini"));
        SettingsFile::setPath(m_path);
        ServiceConnection::setOffline(true);
    }

    void defaultsMatch()
    {
        // An empty file: both sides use their defaults, which must be the same.
        const Wid::Settings service = serviceSettings();
        const Wid::Settings defaults;
        QCOMPARE(service.panModifier, defaults.panModifier);
        const SettingsSchema &schema = SettingsSchema::instance();
        const auto stored = [&schema](const char *key) {
            const SchemaItem *item = schema.find(QLatin1String(key));
            return item->toStored(item->getDefault()).toString();
        };
        QCOMPARE(Wid::parseGestureModifier(stored("PanModifier"), Wid::GestureModifier::Disabled), defaults.panModifier);
        QCOMPARE(Wid::parseGestureModifier(stored("ZoomModifier"), Wid::GestureModifier::Disabled), defaults.zoomModifier);
        QCOMPARE(Wid::parseShowMode(stored("OffscreenMarkers"), Wid::ShowMode::Never), defaults.offscreenMarkers);
        QCOMPARE(stored("PanSpeed").toInt() / 100.0, defaults.panSpeed);
        QCOMPARE(stored("AnimationDuration").toInt(), int(defaults.animationDuration.count()));
        QCOMPARE(stored("EdgePanMargin").toInt(), defaults.edgePanMargin);
        QCOMPARE(stored("GridSpacing").toInt(), defaults.gridSpacing);
        QCOMPARE(stored("Momentum") == QLatin1String("true"), defaults.momentum);
        QCOMPARE(stored("RememberWindowPlacement") == QLatin1String("true"), defaults.rememberWindowPlacement);
    }

    void serviceReadsTheApp()
    {
        write(QStringLiteral("DragDesktopToPan"), false);
        write(QStringLiteral("PanModifier"), 4); // CtrlAlt
        write(QStringLiteral("ZoomModifier"), 0); // Disabled
        write(QStringLiteral("PanSpeed"), 250);
        write(QStringLiteral("RevealMode"), 1); // Center
        write(QStringLiteral("RestorePlacementMode"), 1); // Screen
        write(QStringLiteral("GridColor"), QColor(12, 34, 56));
        write(QStringLiteral("OffscreenMarkers"), 2); // Always
        write(QStringLiteral("ExcludedWindowClasses"), QStringList{QStringLiteral("Notepad"), QStringLiteral("C:\\Tools\\App.exe")});
        const Wid::Settings settings = serviceSettings();
        QCOMPARE(settings.dragDesktopToPan, false);
        QCOMPARE(settings.panModifier, Wid::GestureModifier::CtrlAlt);
        QCOMPARE(settings.zoomModifier, Wid::GestureModifier::Disabled);
        QCOMPARE(settings.panSpeed, 2.5);
        QCOMPARE(settings.revealMode, InfiniteDesktop::RevealMode::Center);
        QCOMPARE(settings.restoreOnCanvas, false);
        QCOMPARE(settings.gridColor, QColor(12, 34, 56));
        QCOMPARE(settings.offscreenMarkers, Wid::ShowMode::Always);
        // The service compares lower case.
        QCOMPARE(settings.excludedClasses, (QStringList{QStringLiteral("notepad"), QStringLiteral("c:\\tools\\app.exe")}));
    }

    void shortcutsMatch()
    {
        // Every action both tables know has the same id and the same default keys.
        int compared = 0;
        for (const ShortcutDefinition &own : InfiniteDesktop::shortcutDefinitions()) {
            for (const Wid::ShortcutDefinition &service : Wid::shortcutDefinitions()) {
                if (QLatin1String(service.id) != QLatin1String(own.id)) {
                    continue;
                }
                ++compared;
                const QList<Wid::KeyCombo> parsed = Wid::parseKeyCombos(KeyCombo::toList(own.defaultShortcuts));
                QCOMPARE(Wid::formatKeyCombos(parsed), Wid::formatKeyCombos(service.defaults));
            }
        }
        QVERIFY(compared >= 25);
    }

    void serviceReadsWrittenShortcuts()
    {
        const QList<QKeySequence> keys{QKeySequence(Qt::META | Qt::ALT | Qt::Key_J), QKeySequence(QKeyCombination(Qt::MetaModifier | Qt::AltModifier | Qt::KeypadModifier, Qt::Key_Plus))};
        {
            QSettings file(m_path, QSettings::IniFormat);
            file.beginGroup(QStringLiteral("Shortcuts"));
            file.setValue(QStringLiteral("InfiniteDesktop ZoomIn"), KeyCombo::toList(keys));
            file.setValue(QStringLiteral("InfiniteDesktop ZoomOut"), QString());
        }
        QSettings file(m_path, QSettings::IniFormat);
        const Wid::ShortcutMap map = Wid::ShortcutMap::load(file);
        const QList<Wid::KeyCombo> zoomIn = map.combosFor(Wid::ShortcutAction::ZoomIn);
        QCOMPARE(Wid::formatKeyCombos(zoomIn), QStringLiteral("Win+Alt+J;Win+Alt+Num+"));
        // An empty value unbinds the action.
        QVERIFY(map.combosFor(Wid::ShortcutAction::ZoomOut).isEmpty());
        // Without an entry the defaults apply.
        QVERIFY(!map.combosFor(Wid::ShortcutAction::Home).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSettingsInterop)
#include "test_settingsinterop.moc"
