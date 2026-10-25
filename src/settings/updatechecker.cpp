/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "updatechecker.h"

#include "i18n.h"
#include "settingsfile.h"

#include <QCoreApplication>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSysInfo>
#include <QVersionNumber>

#include <algorithm>

namespace InfinitDesktop
{

namespace
{

UpdateChecker *s_updateInstance = nullptr;

const QString s_repository = QStringLiteral("RandomNam3/win-infinit-desktop");
const QString s_configGroup = QStringLiteral("Updates");
const QString s_checkOnStartKey = QStringLiteral("CheckOnStart");
constexpr int TimeoutMs = 15000;

/// "v1.2.3" or "1.2.3" -> "1.2.3"; empty for anything else (the version ends up in a shell command).
QString versionOfTag(const QString &tag)
{
    static const QRegularExpression pattern(QStringLiteral("^v?(\\d+\\.\\d+\\.\\d+)$"));
    const QRegularExpressionMatch match = pattern.match(tag);
    return match.hasMatch() ? match.captured(1) : QString();
}

/// A PowerShell string literal ('…' with doubled quotes).
QString powerShellQuote(const QString &text)
{
    QString quoted = text;
    quoted.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

/// "x64" or "arm64": the part of the release package names for this computer.
QString packageArchitecture()
{
    const QString cpu = QSysInfo::currentCpuArchitecture();
    return cpu.startsWith(QLatin1String("arm")) ? QStringLiteral("arm64") : QStringLiteral("x64");
}

/// Only downloads from GitHub's own hosts end up in the update command.
bool isGitHubDownload(const QString &url)
{
    const QUrl parsed(url);
    return parsed.scheme() == QLatin1String("https") && (parsed.host() == QLatin1String("github.com") || parsed.host().endsWith(QLatin1String(".githubusercontent.com")));
}

} // namespace

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
{
}

UpdateChecker::~UpdateChecker() = default;

UpdateChecker *UpdateChecker::instance()
{
    if (!s_updateInstance) {
        s_updateInstance = new UpdateChecker(QCoreApplication::instance());
    }
    return s_updateInstance;
}

UpdateChecker *UpdateChecker::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    UpdateChecker *checker = instance();
    QJSEngine::setObjectOwnership(checker, QJSEngine::CppOwnership);
    return checker;
}

QString UpdateChecker::status() const
{
    return m_status;
}

QString UpdateChecker::currentVersion() const
{
    return QStringLiteral(WID_VERSION_STRING);
}

QString UpdateChecker::latestVersion() const
{
    return m_releases.isEmpty() ? QString() : m_releases.constFirst().toMap().value(QStringLiteral("version")).toString();
}

QVariantList UpdateChecker::releases() const
{
    return m_releases;
}

QString UpdateChecker::errorString() const
{
    return m_errorString;
}

QDateTime UpdateChecker::lastCheck() const
{
    return m_lastCheck;
}

bool UpdateChecker::checkOnStart() const
{
    const QVariant value = SettingsFile::instance()->value(s_configGroup, s_checkOnStartKey);
    return !value.isValid() || value.toString().compare(QLatin1String("false"), Qt::CaseInsensitive) != 0;
}

void UpdateChecker::setCheckOnStart(bool check)
{
    if (check == checkOnStart() || m_demo) {
        return;
    }
    SettingsFile::instance()->setValue(s_configGroup, s_checkOnStartKey, check ? QStringLiteral("true") : QStringLiteral("false"));
    Q_EMIT checkOnStartChanged();
}

QString UpdateChecker::releasesUrl() const
{
    return QStringLiteral("https://github.com/%1/releases").arg(s_repository);
}

void UpdateChecker::setDemoMode(bool demo)
{
    m_demo = demo;
    if (demo) {
        setStatus(QStringLiteral("uptodate"));
    }
}

void UpdateChecker::checkOnStartIfEnabled()
{
    if (!m_demo && checkOnStart()) {
        check();
    }
}

void UpdateChecker::check()
{
    if (m_demo || m_reply) {
        return;
    }
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
    }
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases?per_page=30").arg(s_repository)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("win-infinit-desktop-settings/%1").arg(currentVersion()));
    request.setTransferTimeout(TimeoutMs);
    QNetworkReply *reply = m_network->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        finished(reply);
    });
    setStatus(QStringLiteral("checking"));
}

void UpdateChecker::finished(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply != m_reply) {
        return;
    }
    m_reply.clear();
    m_lastCheck = QDateTime::currentDateTime();
    if (reply->error() != QNetworkReply::NoError) {
        m_errorString = reply->errorString();
        setStatus(QStringLiteral("error"));
        return;
    }
    QString error;
    const QVariantList releases = newerReleases(reply->readAll(), currentVersion(), &error, packageArchitecture());
    if (!error.isEmpty()) {
        m_errorString = error;
        setStatus(QStringLiteral("error"));
        return;
    }
    m_errorString.clear();
    m_releases = releases;
    setStatus(m_releases.isEmpty() ? QStringLiteral("uptodate") : QStringLiteral("available"));
}

QVariantList UpdateChecker::newerReleases(const QByteArray &json, const QString &current, QString *error, const QString &architecture)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (error) {
            *error = i18n("GitHub sent an unexpected answer.");
        }
        return {};
    }
    const QVersionNumber installed = QVersionNumber::fromString(current);
    QList<std::pair<QVersionNumber, QVariantMap>> found;
    const QJsonArray entries = document.array();
    for (const QJsonValue &value : entries) {
        const QJsonObject release = value.toObject();
        if (release.value(QStringLiteral("draft")).toBool() || release.value(QStringLiteral("prerelease")).toBool()) {
            continue;
        }
        const QString version = versionOfTag(release.value(QStringLiteral("tag_name")).toString());
        const QVersionNumber number = QVersionNumber::fromString(version);
        if (version.isEmpty() || number <= installed) {
            continue;
        }
        QVariantMap entry;
        entry.insert(QStringLiteral("version"), version);
        entry.insert(QStringLiteral("name"), release.value(QStringLiteral("name")).toString());
        entry.insert(QStringLiteral("notes"), release.value(QStringLiteral("body")).toString().trimmed());
        entry.insert(QStringLiteral("url"), release.value(QStringLiteral("html_url")).toString());
        entry.insert(QStringLiteral("date"), QDateTime::fromString(release.value(QStringLiteral("published_at")).toString(), Qt::ISODate).toLocalTime());
        // The package for this computer: win-infinit-desktop-<version>-<architecture>.zip.
        const QString packageName = QStringLiteral("win-infinit-desktop-%1-%2.zip").arg(version, architecture);
        const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue &asset : assets) {
            const QJsonObject object = asset.toObject();
            const QString url = object.value(QStringLiteral("browser_download_url")).toString();
            if (object.value(QStringLiteral("name")).toString().compare(packageName, Qt::CaseInsensitive) == 0 && isGitHubDownload(url)) {
                entry.insert(QStringLiteral("package"), url);
                break;
            }
        }
        found.append({number, entry});
    }
    std::sort(found.begin(), found.end(), [](const auto &a, const auto &b) {
        return a.first > b.first;
    });
    QVariantList result;
    for (const auto &item : std::as_const(found)) {
        result.append(item.second);
    }
    return result;
}

QString UpdateChecker::updateCommand() const
{
    const QString version = latestVersion();
    const QString package = m_releases.isEmpty() ? QString() : m_releases.constFirst().toMap().value(QStringLiteral("package")).toString();
    if (version.isEmpty() || package.isEmpty()) {
        return QString();
    }
    // Download the package, unpack it next to the old one in %TEMP% and run its installer.
    return QStringLiteral(
               "$ErrorActionPreference = 'Stop'; "
               "$dir = Join-Path $env:TEMP 'win-infinit-desktop-update-%1'; "
               "if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }; "
               "New-Item -ItemType Directory -Force $dir | Out-Null; "
               "$zip = Join-Path $dir 'package.zip'; "
               "Invoke-WebRequest -UseBasicParsing -Uri %2 -OutFile $zip; "
               "Expand-Archive -Path $zip -DestinationPath (Join-Path $dir 'package'); "
               "& (Join-Path $dir 'package\\install.ps1') -Update")
        .arg(version, powerShellQuote(package));
}

bool UpdateChecker::startUpdate()
{
    const QString command = updateCommand();
    if (command.isEmpty()) {
        return false;
    }
    // A PowerShell window that stays open, so the messages of the installer can be read.
    const QString script = QStringLiteral(
                               "try { %1; Write-Host ''; Write-Host %2 } "
                               "catch { Write-Host ''; Write-Host $_; Write-Host %3 }; "
                               "Read-Host %4 | Out-Null")
                               .arg(command,
                                    powerShellQuote(i18n("The update is installed. WIN Infinit Desktop has been started again.")),
                                    powerShellQuote(i18n("The update failed, see the messages above.")),
                                    powerShellQuote(i18n("Press Enter to close this window")));
    return QProcess::startDetached(QStringLiteral("powershell.exe"),
                                   {QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"), QStringLiteral("-Command"), script});
}

void UpdateChecker::setStatus(const QString &status)
{
    m_status = status;
    Q_EMIT changed();
}

} // namespace InfinitDesktop
