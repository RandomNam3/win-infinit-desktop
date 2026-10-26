/*
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "legacy.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

class TestLegacy : public QObject
{
    Q_OBJECT

private:
    static void write(const QString &path, const QByteArray &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(content);
    }
    static QByteArray read(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

private Q_SLOTS:
    void movesTheOldFolder()
    {
        QTemporaryDir base;
        write(base.filePath(QStringLiteral("WIN Infinit Desktop/notes/a.md")), "note");
        QVERIFY(Wid::migrateLegacyFolder(base.path(), QStringLiteral("WIN Infinit Desktop"), QStringLiteral("WIN Infinite Desktop")));
        QCOMPARE(read(base.filePath(QStringLiteral("WIN Infinite Desktop/notes/a.md"))), QByteArray("note"));
        QVERIFY(!QFileInfo::exists(base.filePath(QStringLiteral("WIN Infinit Desktop"))));
        // Once moved, nothing is left to do.
        QVERIFY(!Wid::migrateLegacyFolder(base.path(), QStringLiteral("WIN Infinit Desktop"), QStringLiteral("WIN Infinite Desktop")));
    }

    void keepsAnExistingNewFolder()
    {
        QTemporaryDir base;
        write(base.filePath(QStringLiteral("WIN Infinit Desktop/settings.ini")), "old");
        write(base.filePath(QStringLiteral("WIN Infinite Desktop/settings.ini")), "new");
        QVERIFY(!Wid::migrateLegacyFolder(base.path(), QStringLiteral("WIN Infinit Desktop"), QStringLiteral("WIN Infinite Desktop")));
        QCOMPARE(read(base.filePath(QStringLiteral("WIN Infinite Desktop/settings.ini"))), QByteArray("new"));
    }

    void nothingWithoutAnOldFolder()
    {
        QTemporaryDir base;
        QVERIFY(!Wid::migrateLegacyFolder(base.path(), QStringLiteral("WIN Infinit Desktop"), QStringLiteral("WIN Infinite Desktop")));
        QVERIFY(!QFileInfo::exists(base.filePath(QStringLiteral("WIN Infinite Desktop"))));
        QVERIFY(!Wid::migrateLegacyFolder(QString(), QStringLiteral("a"), QStringLiteral("b")));
    }

    void renamesTheSettingsGroup()
    {
        QTemporaryDir base;
        const QString ini = base.filePath(QStringLiteral("settings.ini"));
        {
            QSettings file(ini, QSettings::IniFormat);
            file.setValue(QStringLiteral("Effect-infinitdesktop/ZoomStep"), 1.25);
            file.setValue(QStringLiteral("Effect-infinitdesktop/Edge"), QStringLiteral("old"));
            file.setValue(QStringLiteral("Effect-infinitedesktop/Edge"), QStringLiteral("new"));
            file.setValue(QStringLiteral("Shortcuts/ShowMap"), QStringLiteral("Win+Alt+M"));
        }
        QVERIFY(Wid::migrateLegacySettingsGroup(ini, QStringLiteral("Effect-infinitedesktop")));
        QSettings file(ini, QSettings::IniFormat);
        QVERIFY(!file.childGroups().contains(QStringLiteral("Effect-infinitdesktop")));
        QCOMPARE(file.value(QStringLiteral("Effect-infinitedesktop/ZoomStep")).toDouble(), 1.25);
        // A value of the new group wins.
        QCOMPARE(file.value(QStringLiteral("Effect-infinitedesktop/Edge")).toString(), QStringLiteral("new"));
        QCOMPARE(file.value(QStringLiteral("Shortcuts/ShowMap")).toString(), QStringLiteral("Win+Alt+M"));
        // Done once.
        QVERIFY(!Wid::migrateLegacySettingsGroup(ini, QStringLiteral("Effect-infinitedesktop")));
        QVERIFY(!Wid::migrateLegacySettingsGroup(base.filePath(QStringLiteral("missing.ini")), QStringLiteral("x")));
    }
};

QTEST_GUILESS_MAIN(TestLegacy)
#include "test_legacy.moc"
