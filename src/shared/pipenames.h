/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

// Names of the local sockets (named pipes) of the service and the notes app. Header only,
// so that every program of the project can include it without linking anything.

#include <QString>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Wid
{

/// The Windows session of this process: one service per session, one pipe name per session.
inline unsigned long currentSessionId()
{
#ifdef Q_OS_WIN
    DWORD id = 0;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &id)) {
        return id;
    }
#endif
    return 0;
}

/// Pipe of the canvas service (win-infinite-desktop.exe), see IpcServer.
inline QString ipcServerName()
{
    return QStringLiteral("win-infinite-desktop-%1").arg(currentSessionId());
}

/// Pipe of the notes app (wid-notes.exe): same framing as the service's pipe.
inline QString notesServerName()
{
    return ipcServerName() + QStringLiteral("-notes");
}

} // namespace Wid
