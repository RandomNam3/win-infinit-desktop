/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later

    win-infinit-desktop.exe: the canvas service. Runs in the background with an icon in the
    notification area; the settings app and wid-ctl talk to it over IpcServer.
*/
#include "canvasservice.h"
#include "crashguard.h"
#include "ipcserver.h"
#include "logging.h"
#include "osversion.h"
#include "settings.h"
#include "texts.h"
#include "tray.h"
#include "win32util.h"

#include <QGuiApplication>
#include <QJsonValue>

#include <windows.h>

#include <cstring>

using namespace Wid;

namespace
{

const wchar_t *const s_mutexName = L"Local\\WinInfinitDesktop.Service";

bool hasArgument(int argc, char **argv, const char *argument)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], argument) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    // Physical pixels everywhere (the manifest says the same; this covers builds without it).
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const OsVersion os = currentOsVersion();
    if (osSupport(os) == OsSupport::Unsupported && !hasArgument(argc, argv, "--ignore-version")) {
        MessageBoxW(nullptr,
                    text(L"WIN Infinit Desktop needs Windows 11 version 24H2 or newer (build 26100); 25H2 or 26H2 is recommended.",
                         L"WIN Infinit Desktop braucht Windows 11 ab Version 24H2 (Build 26100); empfohlen sind 25H2 oder 26H2."),
                    L"WIN Infinit Desktop", MB_ICONERROR | MB_OK);
        return 1;
    }

    // One instance per session.
    UniqueHandle mutex(CreateMutexW(nullptr, FALSE, s_mutexName));
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("WIN Infinit Desktop"));
    QGuiApplication::setApplicationVersion(QStringLiteral(WID_VERSION_STRING));
    QGuiApplication::setQuitOnLastWindowClosed(false);
    installLogging();

    const Settings settings = Settings::loadDefaultFile();
    CrashGuard guard;
    const bool safeMode = guard.start(settings.crashGuard) || hasArgument(argc, argv, "--safe-mode");

    CanvasService service;
    if (!service.start(safeMode)) {
        return 1;
    }

    Tray tray([&](Tray::Action action) {
        switch (action) {
        case Tray::Action::TogglePause:
            service.setPaused(!service.isPaused());
            break;
        case Tray::Action::BringWindowsBack:
            service.bringWindowsBack();
            break;
        case Tray::Action::Settings:
            service.openSettingsApp();
            break;
        case Tray::Action::LeaveSafeMode:
            guard.leaveSafeMode();
            service.leaveSafeMode();
            break;
        case Tray::Action::Quit:
            QGuiApplication::quit();
            break;
        }
    });
    if (!hasArgument(argc, argv, "--no-tray")) {
        tray.show();
        service.setTray(&tray);
    }

    IpcServer ipc([&service](const QString &method, const QJsonObject &params, QString *error) {
        return service.handleIpc(method, params, error);
    });
    if (!ipc.listen()) {
        qCWarning(lcService) << "cannot listen on" << ipcServerName();
    }
    QObject::connect(&service, &CanvasService::statusChanged, &app, [&]() {
        if (!service.isSafeMode() && guard.isSafeMode()) {
            guard.leaveSafeMode();
        }
        ipc.notify(QStringLiteral("statusChanged"),
                   QJsonObject{{QStringLiteral("paused"), service.isPaused()}, {QStringLiteral("safeMode"), service.isSafeMode()}});
    });
    QObject::connect(&service, &CanvasService::quitRequested, &app, &QGuiApplication::quit);
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &app, [&]() {
        service.shutdown();
        guard.markClean();
    });

    if (safeMode) {
        tray.notify(L"WIN Infinit Desktop",
                    text(L"Started in safe mode after repeated crashes. Windows are not managed until you leave safe mode.",
                         L"Nach wiederholten Abstürzen im abgesicherten Modus gestartet. Fenster werden erst nach dem Verlassen verwaltet."));
    } else if (osSupport(os) == OsSupport::Untested) {
        tray.notify(L"WIN Infinit Desktop",
                    text(L"This Windows version has not been tested. Supported are Windows 11 25H2 and 26H2.",
                         L"Diese Windows-Version ist nicht getestet. Unterstützt werden Windows 11 25H2 und 26H2."));
    }
    return app.exec();
}
