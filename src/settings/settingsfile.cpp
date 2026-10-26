/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "settingsfile.h"

#include "apppaths.h"
#include "serviceconnection.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QSettings>

using namespace std::chrono_literals;

namespace InfiniteDesktop
{

namespace
{
SettingsFile *s_instance = nullptr;
QString s_path;

QString keyPath(const QString &group, const QString &key)
{
    return group.isEmpty() ? key : group + QLatin1Char('/') + key;
}
} // namespace

SettingsFile::SettingsFile(QObject *parent)
    : QObject(parent)
{
    const QString file = path();
    QDir().mkpath(QFileInfo(file).absolutePath());
    m_settings = std::make_unique<QSettings>(file, QSettings::IniFormat);

    m_writeTimer.setSingleShot(true);
    m_writeTimer.setInterval(120ms);
    connect(&m_writeTimer, &QTimer::timeout, this, &SettingsFile::write);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &SettingsFile::fileChanged);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &SettingsFile::fileChanged);
    watch();

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &SettingsFile::flush);
}

SettingsFile::~SettingsFile()
{
    // Normally flushed at aboutToQuit; here only the file is written, the service is
    // not called anymore while the application is going away.
    if (m_writeTimer.isActive()) {
        m_writeTimer.stop();
        m_settings->sync();
    }
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

SettingsFile *SettingsFile::instance()
{
    if (!s_instance) {
        s_instance = new SettingsFile(QCoreApplication::instance());
    }
    return s_instance;
}

void SettingsFile::setPath(const QString &path)
{
    s_path = path;
}

QString SettingsFile::path()
{
    return s_path.isEmpty() ? Wid::appSettingsFilePath() : s_path;
}

void SettingsFile::watch()
{
    const QString file = path();
    const QString folder = QFileInfo(file).absolutePath();
    if (!m_watcher->directories().contains(folder) && QFileInfo::exists(folder)) {
        m_watcher->addPath(folder);
    }
    // A file replaced by an atomic write is a new file: add it again.
    if (!m_watcher->files().contains(file) && QFileInfo::exists(file)) {
        m_watcher->addPath(file);
    }
}

QVariant SettingsFile::value(const QString &group, const QString &key) const
{
    return m_settings->value(keyPath(group, key));
}

bool SettingsFile::contains(const QString &group, const QString &key) const
{
    return m_settings->contains(keyPath(group, key));
}

void SettingsFile::setValue(const QString &group, const QString &key, const QVariant &value)
{
    const QString name = keyPath(group, key);
    if (m_settings->contains(name) && m_settings->value(name) == value) {
        return;
    }
    m_settings->setValue(name, value);
    m_writeTimer.start();
}

void SettingsFile::remove(const QString &group, const QString &key)
{
    const QString name = keyPath(group, key);
    if (!m_settings->contains(name)) {
        return;
    }
    m_settings->remove(name);
    m_writeTimer.start();
}

void SettingsFile::flush()
{
    if (m_writeTimer.isActive()) {
        m_writeTimer.stop();
        write();
    }
}

void SettingsFile::write()
{
    m_settings->sync();
    if (m_settings->status() != QSettings::NoError) {
        qWarning("SettingsFile: cannot write %s", qPrintable(path()));
    }
    watch();
    // The service also watches the file; asking it directly makes the change immediate.
    ServiceConnection::instance()->call(QStringLiteral("reloadSettings"), {}, this, [this](const QJsonValue &, const QString &error) {
        Q_EMIT applied(error.isEmpty());
    });
}

void SettingsFile::fileChanged()
{
    watch();
    if (m_writeTimer.isActive()) {
        // Our own pending change wins; it is written in a moment anyway.
        return;
    }
    m_settings->sync();
    Q_EMIT changedExternally();
}

} // namespace InfiniteDesktop
