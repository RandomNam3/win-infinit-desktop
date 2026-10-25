/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "updatechecker.h"

#include "i18n.h"
#include "settingsfile.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>
#include <QVersionNumber>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

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

/// The installer's result and log, next to the state of the service.
QString updateFile(const QString &name)
{
    QString base = qEnvironmentVariable("LOCALAPPDATA");
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    }
    return QDir(base).filePath(QStringLiteral("WIN Infinit Desktop/") + name);
}

/// Console programs started from here (tar, PowerShell) run without a window.
void withoutWindow(QProcess &process)
{
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
        arguments->flags &= ~DWORD(CREATE_NEW_CONSOLE | DETACHED_PROCESS);
        arguments->flags |= CREATE_NO_WINDOW;
    });
#else
    Q_UNUSED(process)
#endif
}

bool isRunning(const QString &installStatus)
{
    return installStatus == QLatin1String("downloading") || installStatus == QLatin1String("unpacking") || installStatus == QLatin1String("installing");
}

bool processRunning(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, DWORD(pid));
    if (!process) {
        return false;
    }
    const bool running = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    CloseHandle(process);
    return running;
#else
    Q_UNUSED(pid)
    return false;
#endif
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
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases?per_page=30").arg(s_repository)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("win-infinit-desktop-settings/%1").arg(currentVersion()));
    request.setTransferTimeout(TimeoutMs);
    QNetworkReply *reply = network()->get(request);
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
                const QString digest = object.value(QStringLiteral("digest")).toString();
                if (digest.startsWith(QLatin1String("sha256:"))) {
                    entry.insert(QStringLiteral("packageDigest"), digest.toLower());
                }
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
    // Download the package, unpack it in %TEMP% and run its installer, which stops the
    // service and the apps (this one too), installs and starts the service again.
    return QStringLiteral(
               "$ErrorActionPreference = 'Stop'; "
               "$dir = Join-Path $env:TEMP 'win-infinit-desktop-update-%1'; "
               "if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }; "
               "New-Item -ItemType Directory -Force $dir | Out-Null; "
               "$zip = Join-Path $dir 'package.zip'; "
               "Invoke-WebRequest -UseBasicParsing -Uri %2 -OutFile $zip; "
               "Expand-Archive -Path $zip -DestinationPath (Join-Path $dir 'package'); "
               "& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $dir 'package\\install.ps1') -Update; "
               "if ($LASTEXITCODE -ne 0) { throw ('install.ps1 failed: ' + $LASTEXITCODE) }")
        .arg(version, powerShellQuote(package));
}

QNetworkAccessManager *UpdateChecker::network()
{
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
    }
    return m_network;
}

void UpdateChecker::setStatus(const QString &status)
{
    m_status = status;
    Q_EMIT changed();
}

// --- Installing --------------------------------------------------------------------------------

QString UpdateChecker::installStatus() const
{
    return m_installStatus;
}

double UpdateChecker::installProgress() const
{
    return m_installProgress;
}

QString UpdateChecker::installMessage() const
{
    return m_installMessage;
}

bool UpdateChecker::hasInstallLog() const
{
    return QFileInfo::exists(updateFile(QStringLiteral("update.log")));
}

void UpdateChecker::setInstallStatus(const QString &status, const QString &message)
{
    m_installStatus = status;
    m_installMessage = message;
    Q_EMIT installChanged();
}

void UpdateChecker::installUpdate()
{
    if (m_demo || isRunning(m_installStatus)) {
        return;
    }
    const QVariantMap release = m_releases.isEmpty() ? QVariantMap() : m_releases.constFirst().toMap();
    const QString version = release.value(QStringLiteral("version")).toString();
    const QString package = release.value(QStringLiteral("package")).toString();
    if (version.isEmpty() || !isGitHubDownload(package)) {
        setInstallStatus(QStringLiteral("failed"), i18n("This release has no package for this computer."));
        return;
    }
    m_updateDirectory = QDir(QDir::tempPath()).filePath(QStringLiteral("win-infinit-desktop-update-%1").arg(version));
    QDir(m_updateDirectory).removeRecursively();
    m_package = std::make_unique<QFile>(QDir(m_updateDirectory).filePath(QStringLiteral("package.zip")));
    if (!QDir().mkpath(m_updateDirectory) || !m_package->open(QIODevice::WriteOnly)) {
        m_package.reset();
        setInstallStatus(QStringLiteral("failed"), i18n("The download cannot be saved in the folder for temporary files."));
        return;
    }
    m_packageHash.reset();
    m_packageDigest = release.value(QStringLiteral("packageDigest")).toString();

    QNetworkRequest request{QUrl(package)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("win-infinit-desktop-settings/%1").arg(currentVersion()));
    // GitHub redirects to its storage host; the time limit applies while nothing arrives.
    request.setTransferTimeout(2 * TimeoutMs);
    QNetworkReply *reply = network()->get(request);
    m_download = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        if (reply != m_download || !m_package) {
            return;
        }
        const QByteArray data = reply->readAll();
        m_packageHash.addData(data);
        if (m_package->write(data) != data.size()) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, reply](qint64 received, qint64 total) {
        if (reply != m_download) {
            return;
        }
        m_installProgress = total > 0 ? double(received) / double(total) : -1;
        Q_EMIT installChanged();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        downloaded(reply);
    });
    m_installProgress = -1;
    setInstallStatus(QStringLiteral("downloading"));
}

void UpdateChecker::downloaded(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply != m_download) {
        return;
    }
    m_download.clear();
    bool saved = false;
    if (m_package) {
        const QByteArray rest = reply->readAll();
        m_packageHash.addData(rest);
        saved = m_package->write(rest) == rest.size() && m_package->flush() && m_package->error() == QFileDevice::NoError;
        m_package->close();
        m_package.reset();
    }
    if (reply->error() != QNetworkReply::NoError || !saved) {
        const QString reason = saved ? reply->errorString() : i18n("the file could not be saved");
        setInstallStatus(QStringLiteral("failed"), i18nc("@info %1 is the reason", "The download failed: %1", reason));
        return;
    }
    const QString digest = QStringLiteral("sha256:") + QString::fromLatin1(m_packageHash.result().toHex());
    if (!m_packageDigest.isEmpty() && digest != m_packageDigest) {
        setInstallStatus(QStringLiteral("failed"), i18n("The download is damaged: its checksum does not match the one on GitHub. Nothing was installed."));
        return;
    }
    unpack();
}

void UpdateChecker::unpack()
{
    const QString zip = QDir(m_updateDirectory).filePath(QStringLiteral("package.zip"));
    const QString target = QDir(m_updateDirectory).filePath(QStringLiteral("package"));
    if (!QDir().mkpath(target)) {
        setInstallStatus(QStringLiteral("failed"), i18n("The package could not be unpacked."));
        return;
    }
    // tar.exe of Windows (bsdtar) unpacks zip files.
    auto *process = new QProcess(this);
    process->setProgram(QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows"))).filePath(QStringLiteral("System32/tar.exe")));
    process->setArguments({QStringLiteral("-xf"), QDir::toNativeSeparators(zip), QStringLiteral("-C"), QDir::toNativeSeparators(target)});
    withoutWindow(*process);
    connect(process, &QProcess::finished, this, [this, process, target](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (status != QProcess::NormalExit || code != 0 || !QFileInfo::exists(QDir(target).filePath(QStringLiteral("install.ps1")))) {
            setInstallStatus(QStringLiteral("failed"), i18n("The package could not be unpacked."));
            return;
        }
        runInstaller();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            setInstallStatus(QStringLiteral("failed"), i18n("The package could not be unpacked."));
        }
    });
    m_installProgress = -1;
    setInstallStatus(QStringLiteral("unpacking"));
    process->start();
}

void UpdateChecker::runInstaller()
{
    const QString script = QDir(m_updateDirectory).filePath(QStringLiteral("package/install.ps1"));
    const QString log = updateFile(QStringLiteral("update.log"));
    const QString result = updateFile(QStringLiteral("update-result.json"));
    QDir().mkpath(QFileInfo(log).absolutePath());
    QFile::remove(result);
    // The installer closes this app, installs, starts the service and opens this app again.
    QProcess process;
    process.setProgram(QStringLiteral("powershell.exe"));
    process.setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"), QStringLiteral("-File"),
                          QDir::toNativeSeparators(script), QStringLiteral("-Update"), QStringLiteral("-Log"), QDir::toNativeSeparators(log),
                          QStringLiteral("-ResultFile"), QDir::toNativeSeparators(result), QStringLiteral("-OpenSettings")});
    process.setWorkingDirectory(QDir::tempPath());
    withoutWindow(process);
    if (!process.startDetached(&m_installer)) {
        setInstallStatus(QStringLiteral("failed"), i18n("PowerShell, which runs the installer, could not be started."));
        return;
    }
    m_installProgress = -1;
    setInstallStatus(QStringLiteral("installing"));
    // Normally the installer closes this app. Until then: does it report anything, or end
    // without a word (it could not start, or failed before it got to this app)?
    m_installWatch.setInterval(1000);
    connect(&m_installWatch, &QTimer::timeout, this, &UpdateChecker::watchInstaller, Qt::UniqueConnection);
    m_installWatch.start();
}

void UpdateChecker::watchInstaller()
{
    if (m_installStatus != QLatin1String("installing")) {
        m_installWatch.stop();
        return;
    }
    if (QFileInfo::exists(updateFile(QStringLiteral("update-result.json")))) {
        m_installWatch.stop();
        readInstallResult();
        return;
    }
    if (!processRunning(m_installer)) {
        m_installWatch.stop();
        // It may have written its result just before it ended.
        readInstallResult();
        if (m_installStatus == QLatin1String("installing")) {
            setInstallStatus(QStringLiteral("failed"), i18n("The installer ended without a result."));
        }
    }
}

void UpdateChecker::readInstallResult()
{
    // The packages of earlier updates: the installer ran from there and could not remove them.
    // A little later, when the installer that opened this app has ended.
    QTimer::singleShot(10000, this, [this]() {
        if (isRunning(m_installStatus)) {
            return;
        }
        const QDir temp(QDir::tempPath());
        const QStringList folders = temp.entryList({QStringLiteral("win-infinit-desktop-update-*")}, QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &folder : folders) {
            QDir(temp.filePath(folder)).removeRecursively();
        }
    });
    QFile file(updateFile(QStringLiteral("update-result.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject result = QJsonDocument::fromJson(file.read(64 * 1024)).object();
    file.close();
    file.remove();
    const QDateTime time = QDateTime::fromString(result.value(QStringLiteral("time")).toString(), Qt::ISODate);
    if (!time.isValid() || time.secsTo(QDateTime::currentDateTime()) > 24 * 3600) {
        return;
    }
    if (result.value(QStringLiteral("result")).toString() == QLatin1String("ok")) {
        setInstallStatus(QStringLiteral("done"), result.value(QStringLiteral("version")).toString());
    } else {
        setInstallStatus(QStringLiteral("failed"), result.value(QStringLiteral("message")).toString());
    }
}

void UpdateChecker::openInstallLog() const
{
    const QString log = updateFile(QStringLiteral("update.log"));
    if (QFileInfo::exists(log)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(log));
    }
}

} // namespace InfinitDesktop
