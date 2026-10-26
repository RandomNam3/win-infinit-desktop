/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

// Up to version 0.2 the project was called "WIN Infinit Desktop". Its folders under %APPDATA%
// and %LOCALAPPDATA% are taken over once, by whichever program of this version starts first.
// Header only and Qt Core only, so that the service and both apps can use it.

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QList>
#include <QPair>
#include <QStringList>
#include <QVariant>

namespace Wid
{

/// Folder name of the data of this version and of 0.2 and older.
inline constexpr const char *DataFolderName = "WIN Infinite Desktop";
inline constexpr const char *LegacyDataFolderName = "WIN Infinit Desktop";
/// Settings group of the canvas settings up to 0.2 (now "Effect-infinitedesktop").
inline constexpr const char *LegacySettingsGroup = "Effect-infinitdesktop";

/// Moves `base`/`legacy` to `base`/`current` when only the legacy folder exists. A folder that
/// cannot be renamed (a file in it is open) is copied instead: files already present in the
/// new folder win, the old folder is removed as far as possible. Returns true if anything was
/// taken over.
inline bool migrateLegacyFolder(const QString &base, const QString &legacy, const QString &current)
{
    if (base.isEmpty()) {
        return false;
    }
    const QDir dir(base);
    const QString from = dir.filePath(legacy);
    const QString to = dir.filePath(current);
    if (!QFileInfo(from).isDir() || QFileInfo::exists(to)) {
        return false;
    }
    if (QDir().rename(from, to)) {
        return true;
    }
    bool copied = false;
    QDirIterator it(from, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        const QString target = QDir(to).filePath(QDir(from).relativeFilePath(file));
        if (QFileInfo::exists(target)) {
            continue;
        }
        QDir().mkpath(QFileInfo(target).absolutePath());
        copied = QFile::copy(file, target) || copied;
    }
    QDir(from).removeRecursively();
    return copied;
}

/// Renames the settings group of 0.2 in `iniPath` (keys already in the new group win).
inline bool migrateLegacySettingsGroup(const QString &iniPath, const QString &currentGroup)
{
    if (!QFileInfo::exists(iniPath)) {
        return false;
    }
    QSettings file(iniPath, QSettings::IniFormat);
    const QString legacy = QString::fromLatin1(LegacySettingsGroup);
    if (!file.childGroups().contains(legacy)) {
        return false;
    }
    file.beginGroup(legacy);
    const QStringList keys = file.allKeys();
    QList<QPair<QString, QVariant>> values;
    for (const QString &key : keys) {
        values.append({key, file.value(key)});
    }
    file.endGroup();
    file.beginGroup(currentGroup);
    for (const auto &[key, value] : values) {
        if (!file.contains(key)) {
            file.setValue(key, value);
        }
    }
    file.endGroup();
    file.remove(legacy);
    file.sync();
    return file.status() == QSettings::NoError;
}

/// Takes over the data of WIN Infinit Desktop 0.2 and older: %APPDATA% (settings, notes) and
/// %LOCALAPPDATA% (canvas state, logs). Cheap when there is nothing to do; call it before
/// anything opens a file there.
inline void migrateLegacyData(const QString &currentSettingsGroup)
{
    if (QStandardPaths::isTestModeEnabled()) {
        return;
    }
    const QString legacy = QString::fromLatin1(LegacyDataFolderName);
    const QString current = QString::fromLatin1(DataFolderName);
    const QString roaming = qEnvironmentVariable("APPDATA");
    migrateLegacyFolder(roaming, legacy, current);
    migrateLegacyFolder(qEnvironmentVariable("LOCALAPPDATA"), legacy, current);
    if (!roaming.isEmpty()) {
        migrateLegacySettingsGroup(QDir(roaming).filePath(current + QStringLiteral("/settings.ini")), currentSettingsGroup);
    }
}

} // namespace Wid
