/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariant>

#include <memory>

class QFileSystemWatcher;
class QSettings;

namespace InfinitDesktop
{

/**
 * %APPDATA%\WIN Infinit Desktop\settings.ini, the file the canvas service reads, in place of
 * KDE's kwinrc. Values are written like KConfig writes them (the service parses that):
 * bools as true/false, enums by choice name, colors as "r,g,b", lists comma separated.
 *
 * Changes are written shortly after the last edit (atomically, by QSettings) and the
 * service is asked to reload; it also notices the file itself. Edits from elsewhere (an
 * editor, another settings window) are picked up through a file watcher.
 */
class SettingsFile : public QObject
{
    Q_OBJECT

public:
    static SettingsFile *instance();
    /// Uses `path` instead of the user's file (screenshots, unit tests). Call before instance().
    static void setPath(const QString &path);
    static QString path();

    QVariant value(const QString &group, const QString &key) const;
    bool contains(const QString &group, const QString &key) const;
    void setValue(const QString &group, const QString &key, const QVariant &value);
    void remove(const QString &group, const QString &key);

    /// Writes pending changes now.
    void flush();
    bool hasPendingChanges() const
    {
        return m_writeTimer.isActive();
    }

Q_SIGNALS:
    /// Written; `reachedService` is false if the service did not answer the reload request.
    void applied(bool reachedService);
    /// The file changed on disk and has been read again (also after our own writes;
    /// listeners compare the values).
    void changedExternally();

private:
    explicit SettingsFile(QObject *parent);
    ~SettingsFile() override;

    void write();
    void fileChanged();
    void watch();

    std::unique_ptr<QSettings> m_settings;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer m_writeTimer;
};

} // namespace InfinitDesktop
