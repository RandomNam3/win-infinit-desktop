/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later

    wid-notes.exe: notes on the canvas. Started by the canvas service when it needs it.
*/
#include "companion.h"
#include "notestore.h"

#include "apppaths.h"
#include "i18n.h"
#include "legacy.h"
#include "ipcclient.h"
#include "localizer.h"
#include "menutheme.h"
#include "pipenames.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QPalette>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QStyleHints>

using namespace InfiniteDesktop;
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
    QCoreApplication::setOrganizationName(QStringLiteral("WIN Infinite Desktop"));
    QCoreApplication::setApplicationVersion(QStringLiteral(WID_VERSION_STRING));
    // Before the notes are read: the folder of WIN Infinit Desktop 0.2 and older.
    Wid::migrateLegacyData(QString::fromLatin1(Wid::AppSettingsGroup));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("WIN Infinite Desktop"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/notes/icons/note-app.svg")));
    Wid::Localizer::instance().loadForSystemLanguage();
    // Links in the formatted text lie on light paper in light and dark mode alike; the Markdown
    // reader takes their color from the application palette.
    QPalette palette = QGuiApplication::palette();
    palette.setColor(QPalette::Link, QColor(0x1d, 0x64, 0xb8));
    palette.setColor(QPalette::LinkVisited, QColor(0x1d, 0x64, 0xb8));
    QGuiApplication::setPalette(palette);
    // The note menus are native menus: dark in dark mode, like the canvas menu.
    Wid::followSystemMenuTheme();
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &app, []() {
        Wid::refreshMenuTheme();
    });

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
