/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later

    wid-notes.exe: notes on the canvas. Started by the canvas service when it needs it.
*/
#include "companion.h"
#include "notestore.h"

#include "i18n.h"
#include "ipcclient.h"
#include "localizer.h"
#include "pipenames.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>

using namespace InfinitDesktop;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    // All note windows render one after the other on this thread: no render thread and no
    // extra GPU context per note, which matters more than throughput for small, mostly
    // static windows.
    if (qEnvironmentVariableIsEmpty("QSG_RENDER_LOOP")) {
        qputenv("QSG_RENDER_LOOP", "basic");
    }
    QGuiApplication app(argc, argv);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setApplicationName(QStringLiteral("wid-notes"));
    QCoreApplication::setOrganizationName(QStringLiteral("WIN Infinit Desktop"));
    QCoreApplication::setApplicationVersion(QStringLiteral(WID_VERSION_STRING));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("WIN Infinit Desktop"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/notes/icons/note.svg")));
    Wid::Localizer::instance().loadForSystemLanguage();

    QCommandLineParser parser;
    parser.setApplicationDescription(i18n("Notes on the infinite canvas"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption newNoteOption(QStringLiteral("new-note"), i18n("Create a new note (also when the app is already running)."));
    parser.addOption(newNoteOption);
    parser.process(app);

    NoteStore store;
    store.load();

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QQmlEngine engine;
    engine.rootContext()->setContextObject(new Wid::LocalizedContext(&engine));

    Companion companion(&engine, &store);
    if (!companion.registerService()) {
        // Already running: hand the request over and leave.
        if (parser.isSet(newNoteOption)) {
            Wid::IpcClient client(Wid::notesServerName());
            client.callBlocking(QStringLiteral("newNote"), {}, 2s);
        }
        return 0;
    }
    companion.start();
    if (parser.isSet(newNoteOption)) {
        companion.newNote();
    }
    const int result = app.exec();
    store.flush();
    return result;
}
