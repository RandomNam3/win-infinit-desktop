/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later

    win-infinite-desktop.exe: the canvas service. Runs in the background with an icon in the
    notification area; the settings app and wid-ctl talk to it over IpcServer.
*/
#include "canvasservice.h"
#include "crashguard.h"
#include "ipcserver.h"
#include "legacy.h"
#include "logging.h"
#include "menutheme.h"
#include "osversion.h"
#include "settings.h"
#include "texts.h"
#include "tray.h"
#include "win32util.h"

#include <QGuiApplication>
#include <QJsonValue>

#include <windows.h>

#include <cstring>
#include <optional>
#include <string>
#include <vector>

using namespace Wid;

namespace
{

const wchar_t *const s_mutexName = L"Local\\WinInfiniteDesktop.Service";
/// Mutex and pipe of WIN Infinit Desktop 0.2 and older.
const wchar_t *const s_legacyMutexName = L"Local\\WinInfinitDesktop.Service";

/// Asks a running service of 0.2 or older to quit over its pipe (so it brings its windows back)
/// and waits until it has ended. Two services would fight over every window.
void quitLegacyService()
{
    UniqueHandle legacy(OpenMutexW(SYNCHRONIZE, FALSE, s_legacyMutexName));
    if (!legacy) {
        return;
    }
    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    const std::wstring pipe = L"\\\\.\\pipe\\win-infinit-desktop-" + std::to_wstring(session);
    if (!WaitNamedPipeW(pipe.c_str(), 2000)) {
        return;
    }
    const HANDLE raw = CreateFileW(pipe.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (raw == INVALID_HANDLE_VALUE) {
        return;
    }
    UniqueHandle client(raw);
    ULONG serverProcess = 0;
    GetNamedPipeServerProcessId(client.get(), &serverProcess);
    UniqueHandle process(OpenProcess(SYNCHRONIZE, FALSE, serverProcess));
    const char request[] = "{\"id\":1,\"method\":\"quit\",\"params\":{}}\n";
    DWORD written = 0;
    WriteFile(client.get(), request, DWORD(sizeof(request) - 1), &written, nullptr);
    // Its answer (or the closed pipe) shows the request was read before this end goes away.
    for (int waited = 0; waited < 3000; waited += 50) {
        DWORD available = 0;
        if (!PeekNamedPipe(client.get(), nullptr, 0, nullptr, &available, nullptr) || available > 0) {
            break;
        }
        Sleep(50);
    }
    client.reset();
    if (process) {
        WaitForSingleObject(process.get(), 20000);
    }
}

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
                    text(L"WIN Infinite Desktop needs Windows 11 version 24H2 or newer (build 26100); 25H2 or 26H2 is recommended.",
                         L"WIN Infinite Desktop braucht Windows 11 ab Version 24H2 (Build 26100); empfohlen sind 25H2 oder 26H2."),
                    L"WIN Infinite Desktop", MB_ICONERROR | MB_OK);
        return 1;
    }

    // One instance per session.
    UniqueHandle mutex(CreateMutexW(nullptr, FALSE, s_mutexName));
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }
    // The renamed version: the old service steps aside, its settings and canvases move over.
    quitLegacyService();
    migrateLegacyData(QString::fromLatin1(SettingsGroup));
    // Canvas menu and tray menu dark like the rest of Windows in dark mode.
    followSystemMenuTheme();

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("WIN Infinite Desktop"));
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

    // Tray actions run after the menu's own message loop, from the event loop.
    const auto onMonitorOfPointer = []() {
        POINT cursor = {};
        GetCursorPos(&cursor);
        return QPointF(cursor.x, cursor.y);
    };
    Tray tray([&](Tray::Action action, int index) {
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
        case Tray::Action::NewNote:
            service.organizer().newNote(std::nullopt);
            break;
        case Tray::Action::ToggleNotes:
            service.organizer().toggleNotes();
            break;
        case Tray::Action::ShowMap:
            service.organizer().toggleMap(service.monitorIndexAt(onMonitorOfPointer()));
            break;
        case Tray::Action::GroupFromSelection:
        case Tray::Action::AreaFromSelection:
            service.organizer().startSelection(action == Tray::Action::GroupFromSelection, service.monitorIndexAt(onMonitorOfPointer()));
            break;
        case Tray::Action::OpenGroup:
            service.organizer().openGroup(index);
            break;
        }
    });
    tray.setGroupNames([&service]() {
        std::vector<std::wstring> names;
        for (const auto &[id, name] : service.organizer().groupEntries()) {
            names.push_back(name.toStdWString());
        }
        return names;
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
    QObject::connect(&service, &CanvasService::ipcEvent, &app, [&ipc](const QString &event, const QJsonObject &params) {
        ipc.notify(event, params);
    });
    QObject::connect(&service, &CanvasService::quitRequested, &app, &QGuiApplication::quit);
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &app, [&]() {
        service.shutdown();
        guard.markClean();
    });

    if (safeMode) {
        tray.notify(L"WIN Infinite Desktop",
                    text(L"Started in safe mode after repeated crashes. Windows are not managed until you leave safe mode.",
                         L"Nach wiederholten Abstürzen im abgesicherten Modus gestartet. Fenster werden erst nach dem Verlassen verwaltet."));
    }
    return app.exec();
}
