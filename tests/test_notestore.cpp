/*
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop and WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "notestore.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace InfiniteDesktop;

class TestNoteStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QDir(NoteStore::directory()).removeRecursively();
    }

    void cleanup()
    {
        QDir(NoteStore::directory()).removeRecursively();
    }

    void titles()
    {
        Note note(QStringLiteral("x"), nullptr);
        QCOMPARE(note.title(), QString());
        note.setText(QStringLiteral("\n\n## Shopping list  \n- milk"));
        QCOMPARE(note.title(), QStringLiteral("Shopping list"));
    }

    void createSaveLoad()
    {
        QString id;
        {
            NoteStore store;
            QSignalSpy added(&store, &NoteStore::noteAdded);
            Note *note = store.create(3);
            QVERIFY(note);
            QCOMPARE(added.size(), 1);
            id = note->id();
            QVERIFY(NoteStore::isValidId(id));
            note->setText(QStringLiteral("# Hello\n**bold**"));
            note->setWidth(10); // clamped
            note->setHeight(500);
            note->setHidden(true);
            QVERIFY(!note->isFormatted()); // the Markdown source is shown by default
            note->setFormatted(true);
            store.setNotesVisible(false);
            // Saved on destruction (flush) without waiting for the timer.
        }
        QVERIFY(QFile::exists(NoteStore::directory() + QLatin1Char('/') + id + QStringLiteral(".md")));

        NoteStore store;
        store.load();
        QCOMPARE(store.notes().size(), std::size_t(1));
        Note *note = store.find(id);
        QVERIFY(note);
        QCOMPARE(note->text(), QStringLiteral("# Hello\n**bold**"));
        QCOMPARE(note->title(), QStringLiteral("Hello"));
        QCOMPARE(note->color(), 3);
        QCOMPARE(note->width(), Note::MinimumWidth);
        QCOMPARE(note->height(), 500);
        QVERIFY(note->isHidden());
        QVERIFY(note->isFormatted());
        QVERIFY(!store.notesVisible());

        QSignalSpy removed(&store, &NoteStore::noteRemoved);
        store.remove(id);
        QCOMPARE(removed.size(), 1);
        QVERIFY(!store.find(id));
        QVERIFY(!QFile::exists(NoteStore::directory() + QLatin1Char('/') + id + QStringLiteral(".md")));
        // The object goes away with the next event loop round.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void rejectsForeignIds()
    {
        QVERIFY(!NoteStore::isValidId(QStringLiteral("../../etc/passwd")));
        QVERIFY(!NoteStore::isValidId(QString()));
        QDir().mkpath(NoteStore::directory());
        QFile index(NoteStore::directory() + QStringLiteral("/index.json"));
        QVERIFY(index.open(QIODevice::WriteOnly));
        index.write(R"({"notes":[{"id":"../evil"},{"id":"0f8c3a2e-1d2b-4c5d-8e9f-0a1b2c3d4e5f","color":99},{"id":"0f8c3a2e-1d2b-4c5d-8e9f-0a1b2c3d4e5f"}]})");
        index.close();
        NoteStore store;
        store.load();
        QCOMPARE(store.notes().size(), std::size_t(1));
        QCOMPARE(store.notes().front()->color(), Note::Colors - 1);
        // Notes from before the setting existed open with their Markdown source.
        QVERIFY(!store.notes().front()->isFormatted());
    }
};

QTEST_GUILESS_MAIN(TestNoteStore)

#include "test_notestore.moc"
