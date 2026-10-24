/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "logging.h"
#include "settings.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>

#include <windows.h>

Q_LOGGING_CATEGORY(lcService, "wid.service", QtInfoMsg)

namespace Wid
{

namespace
{

QMutex s_logMutex;
constexpr qint64 MaxLogSize = 1024 * 1024;

void writeLog(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const char *level = "debug";
    switch (type) {
    case QtDebugMsg:
        level = "debug";
        break;
    case QtInfoMsg:
        level = "info";
        break;
    case QtWarningMsg:
        level = "warning";
        break;
    case QtCriticalMsg:
        level = "critical";
        break;
    case QtFatalMsg:
        level = "fatal";
        break;
    }
    const QString line = QStringLiteral("%1 %2 %3: %4\n")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs), QLatin1String(level),
                                  QLatin1String(context.category ? context.category : "default"), message);
    OutputDebugStringW(reinterpret_cast<const wchar_t *>(line.utf16()));
    QMutexLocker locker(&s_logMutex);
    const QString path = QDir(stateDirectory()).filePath(QStringLiteral("service.log"));
    QFile file(path);
    // One old generation is kept.
    if (file.size() > MaxLogSize) {
        QFile::remove(path + QStringLiteral(".1"));
        QFile::rename(path, path + QStringLiteral(".1"));
    }
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        file.write(line.toUtf8());
    }
}

} // namespace

void installLogging()
{
    qInstallMessageHandler(writeLog);
    setDebugLogging(qEnvironmentVariableIntValue("WID_DEBUG") != 0);
}

void setDebugLogging(bool enabled)
{
    const bool forced = qEnvironmentVariableIntValue("WID_DEBUG") != 0;
    QLoggingCategory::setFilterRules(enabled || forced ? QStringLiteral("wid.*.debug=true") : QStringLiteral("wid.*.debug=false"));
}

} // namespace Wid
