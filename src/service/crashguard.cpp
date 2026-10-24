/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "crashguard.h"
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace Wid
{

CrashGuard::CrashGuard(const QString &path, QObject *parent)
    : QObject(parent)
    , m_path(path.isEmpty() ? QDir(stateDirectory()).filePath(QStringLiteral("crashguard")) : path)
{
    m_stableTimer.setSingleShot(true);
    m_stableTimer.setInterval(StableAfterMs);
    connect(&m_stableTimer, &QTimer::timeout, this, [this]() {
        if (m_armed) {
            writeCounter(0);
        }
    });
}

int CrashGuard::counter() const
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    bool ok = false;
    const int value = QString::fromLatin1(file.read(32)).trimmed().toInt(&ok);
    return ok && value > 0 ? value : 0;
}

void CrashGuard::writeCounter(int value) const
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QByteArray::number(value));
        file.commit();
    }
}

bool CrashGuard::start(bool enabled)
{
    const int count = counter();
    if (enabled && count >= Threshold) {
        m_safeMode = true;
        m_armed = false;
        return true;
    }
    m_safeMode = false;
    m_armed = true;
    // Written at once: a crash during startup must already count.
    writeCounter(count + 1);
    m_stableTimer.start();
    return false;
}

void CrashGuard::leaveSafeMode()
{
    m_safeMode = false;
    m_armed = true;
    writeCounter(1);
    m_stableTimer.start();
}

void CrashGuard::markClean()
{
    if (m_armed) {
        m_stableTimer.stop();
        writeCounter(0);
    }
    m_armed = false;
}

} // namespace Wid
