/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
// The settings schema and how values are stored in settings.ini (settingsschema.h,
// settingsfile.h): the format the canvas service reads.

#include "serviceconnection.h"
#include "settingsfile.h"
#include "settingsmeta.h"
#include "settingsschema.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace InfiniteDesktop;

class TestSettingsSchema : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_path;

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("settings.ini"));
        SettingsFile::setPath(m_path);
        ServiceConnection::setOffline(true);
    }

    void schemaIsComplete()
    {
        const SettingsSchema &schema = SettingsSchema::instance();
        QCOMPARE(schema.group(), QStringLiteral("Effect-infinitedesktop"));
        QVERIFY(schema.items().size() >= 45);
        for (const auto &item : schema.items()) {
            QVERIFY2(!item->label().isEmpty(), qPrintable(item->name()));
            QVERIFY2(!item->whatsThis().isEmpty(), qPrintable(item->name()));
            // Every entry is shown in a category.
            QVERIFY2(findSettingsEntry(item->name().toLatin1().constData()), qPrintable(item->name()));
            if (item->type() == SchemaItem::Type::Enum) {
                QVERIFY(!item->choices().isEmpty());
            }
        }
        // And every category entry exists in the schema.
        for (const SettingsEntryMeta &meta : settingsEntries()) {
            QVERIFY2(schema.find(QLatin1String(meta.key)), meta.key);
        }
    }

    void defaultsAreThoseOfTheService()
    {
        const SettingsSchema &schema = SettingsSchema::instance();
        // See struct Settings in src/service/settings.h.
        QCOMPARE(schema.find(QStringLiteral("DragDesktopToPan"))->getDefault().toBool(), true);
        QCOMPARE(schema.find(QStringLiteral("PanModifier"))->toStored(schema.find(QStringLiteral("PanModifier"))->getDefault()).toString(), QStringLiteral("MetaAlt"));
        QCOMPARE(schema.find(QStringLiteral("ZoomModifier"))->toStored(schema.find(QStringLiteral("ZoomModifier"))->getDefault()).toString(), QStringLiteral("Meta"));
        QCOMPARE(schema.find(QStringLiteral("PanSpeed"))->getDefault().toInt(), 100);
        QCOMPARE(schema.find(QStringLiteral("AnimationDuration"))->getDefault().toInt(), 280);
        QCOMPARE(schema.find(QStringLiteral("GridColor"))->getDefault().value<QColor>(), QColor(255, 255, 255));
        QCOMPARE(schema.find(QStringLiteral("OffscreenMarkers"))->toStored(schema.find(QStringLiteral("OffscreenMarkers"))->getDefault()).toString(),
                 QStringLiteral("WhileNavigating"));
        QVERIFY(schema.find(QStringLiteral("ExcludedWindowClasses"))->getDefault().toStringList().isEmpty());
        QCOMPARE(schema.find(QStringLiteral("MapSize"))->getDefault().toInt(), 70);
    }

    void storesLikeKConfig()
    {
        const SettingsSchema &schema = SettingsSchema::instance();
        SchemaItem *modifier = schema.find(QStringLiteral("PanModifier"));
        SchemaItem *color = schema.find(QStringLiteral("GridColor"));
        SchemaItem *list = schema.find(QStringLiteral("ExcludedWindowClasses"));
        SchemaItem *speed = schema.find(QStringLiteral("PanSpeed"));
        SchemaItem *grid = schema.find(QStringLiteral("CrashGuard"));
        QVERIFY(modifier && color && list && speed && grid);

        SettingsFile *file = SettingsFile::instance();
        modifier->setProperty(3);
        color->setProperty(QColor(10, 20, 30));
        list->setProperty(QStringList{QStringLiteral("notepad"), QStringLiteral(" "), QStringLiteral("notepad"), QStringLiteral("C:\\Tools\\x.exe")});
        speed->setProperty(9999); // clamped to the maximum
        grid->setProperty(false);
        for (SchemaItem *item : {modifier, color, list, speed, grid}) {
            item->writeConfig(*file);
        }
        file->flush();

        QSettings ini(m_path, QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("Effect-infinitedesktop"));
        QCOMPARE(ini.value(QStringLiteral("PanModifier")).toString(), modifier->choices().at(3).name);
        QCOMPARE(ini.value(QStringLiteral("GridColor")).toString(), QStringLiteral("10,20,30"));
        QCOMPARE(ini.value(QStringLiteral("ExcludedWindowClasses")).toString(), QStringLiteral("notepad,C:\\Tools\\x.exe"));
        QCOMPARE(ini.value(QStringLiteral("PanSpeed")).toString(), QStringLiteral("400"));
        QCOMPARE(ini.value(QStringLiteral("CrashGuard")).toString(), QStringLiteral("false"));
        ini.endGroup();

        // Back to the default: the entry disappears from the file.
        speed->setDefault();
        speed->writeConfig(*file);
        file->flush();
        QSettings again(m_path, QSettings::IniFormat);
        QVERIFY(!again.contains(QStringLiteral("Effect-infinitedesktop/PanSpeed")));
        QVERIFY(again.contains(QStringLiteral("Effect-infinitedesktop/PanModifier")));

        // And everything reads back.
        modifier->setDefault();
        color->setDefault();
        list->setDefault();
        modifier->readConfig(*file);
        color->readConfig(*file);
        list->readConfig(*file);
        QCOMPARE(modifier->property().toInt(), 3);
        QCOMPARE(color->property().value<QColor>(), QColor(10, 20, 30));
        QCOMPARE(list->property().toStringList(), (QStringList{QStringLiteral("notepad"), QStringLiteral("C:\\Tools\\x.exe")}));
    }

    void readsWhatPeopleWrite()
    {
        const SettingsSchema &schema = SettingsSchema::instance();
        SchemaItem *modifier = schema.find(QStringLiteral("PanModifier"));
        SchemaItem *speed = schema.find(QStringLiteral("PanSpeed"));
        SchemaItem *flag = schema.find(QStringLiteral("Momentum"));
        // Enum names in any case, numbers, garbage.
        QCOMPARE(modifier->fromStored(QStringLiteral("metashift")).toInt(), 3);
        QCOMPARE(modifier->fromStored(QStringLiteral("2")).toInt(), 2);
        QVERIFY(!modifier->fromStored(QStringLiteral("Hyper")).isValid());
        QCOMPARE(speed->fromStored(QStringLiteral("5")).toInt(), 20);
        QVERIFY(!speed->fromStored(QStringLiteral("fast")).isValid());
        QCOMPARE(flag->fromStored(QStringLiteral("TRUE")).toBool(), true);
        QCOMPARE(flag->fromStored(QStringLiteral("0")).toBool(), false);
        QVERIFY(!flag->fromStored(QStringLiteral("maybe")).isValid());
    }
};

QTEST_GUILESS_MAIN(TestSettingsSchema)
#include "test_settingsschema.moc"
