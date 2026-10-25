/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QString>

namespace Wid
{

/// %APPDATA%\WIN Infinit Desktop (roaming): settings and notes. Not created here.
inline QString roamingDataDirectory()
{
    QString base = qEnvironmentVariable("APPDATA");
    if (base.isEmpty() || QStandardPaths::isTestModeEnabled()) {
        base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    }
    return QDir(base).filePath(QStringLiteral("WIN Infinit Desktop"));
}

/// %APPDATA%\WIN Infinit Desktop\settings.ini, the file the service reads (settingsFilePath() there).
inline QString appSettingsFilePath()
{
    return QDir(roamingDataDirectory()).filePath(QStringLiteral("settings.ini"));
}

/// Settings group of the canvas settings, as in KDE's kwinrc.
inline constexpr const char *AppSettingsGroup = "Effect-infinitdesktop";
/// Settings group of the shortcuts (key = action id, value = "Win+Alt+H;…").
inline constexpr const char *AppShortcutsGroup = "Shortcuts";

/// A program installed next to this one ("wid-notes.exe", …).
inline QString siblingProgram(const QString &name)
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(name);
}

#ifdef Q_OS_WIN
inline constexpr const char *ServiceExecutable = "win-infinit-desktop.exe";
inline constexpr const char *NotesExecutable = "wid-notes.exe";
inline constexpr const char *SettingsExecutable = "wid-settings.exe";
#else
inline constexpr const char *ServiceExecutable = "win-infinit-desktop";
inline constexpr const char *NotesExecutable = "wid-notes";
inline constexpr const char *SettingsExecutable = "wid-settings";
#endif

} // namespace Wid
