/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
// Key combinations in the notation of the service's settings file (keycombo.h).

#include "keycombo.h"
#include "shortcutdefs.h"

#include <QTest>

using namespace InfiniteDesktop;

class TestKeyCombo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void toText_data()
    {
        QTest::addColumn<QKeySequence>("sequence");
        QTest::addColumn<QString>("text");
        QTest::newRow("letter") << QKeySequence(Qt::META | Qt::ALT | Qt::Key_H) << QStringLiteral("Win+Alt+H");
        QTest::newRow("order") << QKeySequence(Qt::SHIFT | Qt::ALT | Qt::CTRL | Qt::META | Qt::Key_Left) << QStringLiteral("Win+Ctrl+Alt+Shift+Left");
        QTest::newRow("equal") << QKeySequence(Qt::META | Qt::ALT | Qt::Key_Equal) << QStringLiteral("Win+Alt+=");
        QTest::newRow("plus is the same key") << QKeySequence(Qt::META | Qt::ALT | Qt::Key_Plus) << QStringLiteral("Win+Alt+=");
        QTest::newRow("keypad plus") << QKeySequence(QKeyCombination(Qt::MetaModifier | Qt::AltModifier | Qt::KeypadModifier, Qt::Key_Plus)) << QStringLiteral("Win+Alt+Num+");
        QTest::newRow("keypad digit") << QKeySequence(QKeyCombination(Qt::ControlModifier | Qt::KeypadModifier, Qt::Key_7)) << QStringLiteral("Ctrl+Num7");
        QTest::newRow("function key") << QKeySequence(Qt::META | Qt::ALT | Qt::Key_F9) << QStringLiteral("Win+Alt+F9");
        QTest::newRow("digit") << QKeySequence(Qt::META | Qt::CTRL | Qt::ALT | Qt::Key_1) << QStringLiteral("Win+Ctrl+Alt+1");
        QTest::newRow("page") << QKeySequence(Qt::ALT | Qt::Key_PageDown) << QStringLiteral("Alt+PgDown");
        QTest::newRow("print is not supported") << QKeySequence(Qt::META | Qt::Key_Print) << QString();
        QTest::newRow("chord is not supported") << QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_C) << QString();
    }
    void toText()
    {
        QFETCH(QKeySequence, sequence);
        QFETCH(QString, text);
        QCOMPARE(KeyCombo::toText(sequence), text);
        QCOMPARE(KeyCombo::isSupported(sequence), !text.isEmpty());
    }

    void fromText_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QKeySequence>("sequence");
        QTest::newRow("letter") << QStringLiteral("Win+Alt+H") << QKeySequence(Qt::META | Qt::ALT | Qt::Key_H);
        QTest::newRow("case and order") << QStringLiteral("alt+WIN+h") << QKeySequence(Qt::META | Qt::ALT | Qt::Key_H);
        QTest::newRow("german names") << QStringLiteral("Strg+Umschalt+Left") << QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Left);
        QTest::newRow("trailing plus") << QStringLiteral("Win+Alt++") << QKeySequence(QKeyCombination(Qt::MetaModifier | Qt::AltModifier | Qt::KeypadModifier, Qt::Key_Plus));
        QTest::newRow("keypad") << QStringLiteral("Win+Alt+Num-") << QKeySequence(QKeyCombination(Qt::MetaModifier | Qt::AltModifier | Qt::KeypadModifier, Qt::Key_Minus));
        QTest::newRow("alias") << QStringLiteral("Ctrl+Escape") << QKeySequence(Qt::CTRL | Qt::Key_Escape);
        QTest::newRow("unknown modifier") << QStringLiteral("Hyper+A") << QKeySequence();
        QTest::newRow("unknown key") << QStringLiteral("Win+Banana") << QKeySequence();
        QTest::newRow("empty") << QString() << QKeySequence();
    }
    void fromText()
    {
        QFETCH(QString, text);
        QFETCH(QKeySequence, sequence);
        QCOMPARE(KeyCombo::fromText(text), sequence);
    }

    void lists()
    {
        const QList<QKeySequence> list{QKeySequence(Qt::META | Qt::ALT | Qt::Key_Equal), QKeySequence(Qt::META | Qt::ALT | Qt::Key_Plus),
                                       QKeySequence(Qt::META | Qt::Key_Print), QKeySequence(QKeyCombination(Qt::MetaModifier | Qt::AltModifier | Qt::KeypadModifier, Qt::Key_Plus))};
        // Duplicates (as the service sees them) and unsupported keys drop out.
        QCOMPARE(KeyCombo::toList(list), QStringLiteral("Win+Alt+=;Win+Alt+Num+"));
        QCOMPARE(KeyCombo::fromList(QStringLiteral("Win+Alt+=;;nonsense;Win+Alt+Num+;Win+Alt+=")),
                 (QList<QKeySequence>{QKeySequence(Qt::META | Qt::ALT | Qt::Key_Equal), QKeySequence(QKeyCombination(Qt::MetaModifier | Qt::AltModifier | Qt::KeypadModifier, Qt::Key_Plus))}));
        QVERIFY(KeyCombo::fromList(QString()).isEmpty());
    }

    void everyDefaultCanBeBound()
    {
        for (const ShortcutDefinition &definition : shortcutDefinitions()) {
            for (const QKeySequence &sequence : definition.defaultShortcuts) {
                QVERIFY2(KeyCombo::isSupported(sequence), definition.id);
                QCOMPARE(KeyCombo::fromText(KeyCombo::toText(sequence)).toString(), sequence.toString());
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestKeyCombo)
#include "test_keycombo.moc"
