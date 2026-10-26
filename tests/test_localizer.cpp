/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
// The .po catalog reader and the i18n functions (localizer.h).

#include "localizer.h"

#include <QTest>

using namespace Wid;

class TestLocalizer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesEntries()
    {
        // Built from plain literals: moc would read lines starting with "#" as directives.
        const QByteArray po = "# A comment\n"
"msgid \"\"\n"
"msgstr \"\"\n"
"\"Language: de\\n\"\n"
"\"Plural-Forms: nplurals=2; plural=(n != 1);\\n\"\n"
"\n"
"msgid \"Zoom In\"\n"
"msgstr \"Vergrößern\"\n"
"\n"
"msgctxt \"@action:button\"\n"
"msgid \"Open\"\n"
"msgstr \"Öffnen\"\n"
"\n"
"msgid \"Open\"\n"
"msgstr \"Offen\"\n"
"\n"
"msgid \"\"\n"
"\"A long text that \"\n"
"\"goes on\"\n"
"msgstr \"\"\n"
"\"Ein langer Text, der \"\n"
"\"weitergeht\"\n"
"\n"
"msgid \"Quote \\\"this\\\"\\tand\\\\that\\n\"\n"
"msgstr \"Zitat „dies“\\tund\\\\das\\n\"\n"
"\n"
"#, fuzzy\n"
"msgid \"Fuzzy\"\n"
"msgstr \"Unscharf\"\n"
"\n"
"msgid \"Untranslated\"\n"
"msgstr \"\"\n"
"\n"
"msgctxt \"@info\"\n"
"msgid \"%1 window\"\n"
"msgid_plural \"%1 windows\"\n"
"msgstr[0] \"%1 Fenster\"\n"
"msgstr[1] \"%1 Fenster (viele)\"\n"
"\n"
"msgid \"Half\"\n"
"msgid_plural \"Halves\"\n"
"msgstr[0] \"Hälfte\"\n"
"msgstr[1] \"\"\n";
        Catalog catalog;
        QVERIFY(catalog.parse(po));
        QCOMPARE(catalog.find(QString(), QStringLiteral("Zoom In")), QStringLiteral("Vergrößern"));
        QCOMPARE(catalog.find(QStringLiteral("@action:button"), QStringLiteral("Open")), QStringLiteral("Öffnen"));
        QCOMPARE(catalog.find(QString(), QStringLiteral("Open")), QStringLiteral("Offen"));
        QCOMPARE(catalog.find(QString(), QStringLiteral("A long text that goes on")), QStringLiteral("Ein langer Text, der weitergeht"));
        QCOMPARE(catalog.find(QString(), QStringLiteral("Quote \"this\"\tand\\that\n")), QStringLiteral("Zitat „dies“\tund\\das\n"));
        QVERIFY(catalog.find(QString(), QStringLiteral("Fuzzy")).isNull());
        QVERIFY(catalog.find(QString(), QStringLiteral("Untranslated")).isNull());
        QCOMPARE(catalog.findPlural(QStringLiteral("@info"), QStringLiteral("%1 window"), 1), QStringLiteral("%1 Fenster"));
        QCOMPARE(catalog.findPlural(QStringLiteral("@info"), QStringLiteral("%1 window"), 0), QStringLiteral("%1 Fenster (viele)"));
        QCOMPARE(catalog.findPlural(QStringLiteral("@info"), QStringLiteral("%1 window"), 5), QStringLiteral("%1 Fenster (viele)"));
        // An incomplete plural entry is not used at all.
        QVERIFY(catalog.findPlural(QString(), QStringLiteral("Half"), 1).isNull());
    }

    void rejectsGarbage()
    {
        Catalog catalog;
        QVERIFY(!catalog.parse("this is not a catalog"));
        QVERIFY(!catalog.parse("msgid \"unterminated\nmsgstr \"x\""));
        QVERIFY(!catalog.parse(QByteArray()));
    }

    void substitutesInOnePass()
    {
        QCOMPARE(Localizer::substitute(QStringLiteral("%1 and %2"), {QStringLiteral("a%2"), QStringLiteral("b")}), QStringLiteral("a%2 and b"));
        QCOMPARE(Localizer::substitute(QStringLiteral("%2 before %1"), {QStringLiteral("x"), QStringLiteral("y")}), QStringLiteral("y before x"));
        // Missing arguments and "%" alone stay as they are.
        QCOMPARE(Localizer::substitute(QStringLiteral("100 % of %1 and %3"), {QStringLiteral("x")}), QStringLiteral("100 % of x and %3"));
        QCOMPARE(Localizer::substitute(QStringLiteral("no arguments %1"), {}), QStringLiteral("no arguments %1"));
    }

    void formatsQmlArguments()
    {
        QCOMPARE(Localizer::argumentText(QVariant(3.0)), QStringLiteral("3"));
        QCOMPARE(Localizer::argumentText(QVariant(-12)), QStringLiteral("-12"));
        QCOMPARE(Localizer::argumentText(QVariant(QStringLiteral("text"))), QStringLiteral("text"));
        QCOMPARE(Localizer::argumentText(QVariant(true)), QStringLiteral("true"));
    }

    void translatesWithTheCatalog()
    {
        Catalog catalog;
        QVERIFY(catalog.parse("msgid \"Hello %1\"\n"
"msgstr \"Hallo %1\"\n"
"\n"
"msgctxt \"@info\"\n"
"msgid \"%1 note\"\n"
"msgid_plural \"%1 notes\"\n"
"msgstr[0] \"%1 Notiz\"\n"
"msgstr[1] \"%1 Notizen\"\n"));
        Localizer::instance().setCatalog(std::move(catalog));
        QCOMPARE(i18n("Hello %1", QStringLiteral("Welt")), QStringLiteral("Hallo Welt"));
        QCOMPARE(i18n("Not in the catalog %1", 5), QStringLiteral("Not in the catalog 5"));
        QCOMPARE(i18ncp("@info", "%1 note", "%1 notes", 1), QStringLiteral("1 Notiz"));
        QCOMPARE(i18ncp("@info", "%1 note", "%1 notes", 4), QStringLiteral("4 Notizen"));
        QCOMPARE(i18np("%1 file", "%1 files", 2), QStringLiteral("2 files"));
        QCOMPARE(kli18n("Hello %1").toString(), QStringLiteral("Hallo %1"));

        const LocalizedContext context;
        QCOMPARE(context.i18n(QStringLiteral("Hello %1"), QStringLiteral("QML")), QStringLiteral("Hallo QML"));
        QCOMPARE(context.i18ncp(QStringLiteral("@info"), QStringLiteral("%1 note"), QStringLiteral("%1 notes"), 3.0), QStringLiteral("3 Notizen"));
        QCOMPARE(context.i18nc(QStringLiteral("@other"), QStringLiteral("Hello %1"), 7), QStringLiteral("Hello 7"));
        Localizer::instance().setCatalog(Catalog());
    }

    void theGermanCatalog()
    {
        // The catalog that is compiled into the apps.
        Catalog catalog;
        QVERIFY(catalog.load(QStringLiteral(WID_SOURCE_DIR "/po/de/win-infinite-desktop.po")));
        QVERIFY(catalog.size() > 500);
        QCOMPARE(catalog.find(QStringLiteral("@title:tab"), QStringLiteral("With Windows")), QStringLiteral("Mit Windows"));
        QCOMPARE(catalog.findPlural(QStringLiteral("@title:tab %1 is the number of conflicts"), QStringLiteral("With Windows · %1 conflict"), 2),
                 QStringLiteral("Mit Windows · %1 Konflikte"));
    }
};

QTEST_GUILESS_MAIN(TestLocalizer)
#include "test_localizer.moc"
