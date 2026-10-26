/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QCryptographicHash>
#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QFile;
class QJSEngine;
class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;

namespace InfiniteDesktop
{

/**
 * Looks for newer releases on GitHub and installs them.
 *
 * Every version is a GitHub release of win-infinite-desktop (tag vX.Y.Z, notes in
 * Markdown) with a package win-infinite-desktop-X.Y.Z-<x64|arm64>.zip. @c releases holds
 * the ones newer than this app, newest first (with "package", the download for this
 * computer, if there is one, and "packageDigest", its SHA-256 as GitHub reports it).
 *
 * The update downloads that package (with progress), checks its SHA-256, unpacks it to
 * %TEMP% with tar.exe and runs its install.ps1 without a window. The installer closes this
 * app, installs, starts the service and opens this app again; what it did is in
 * %LOCALAPPDATA%\WIN Infinite Desktop\update-result.json (read at the next start) and
 * update.log.
 *
 * @c status is one of "idle", "checking", "uptodate", "available" and "error";
 * @c installStatus one of "" (no update running), "downloading", "unpacking", "installing",
 * "done" and "failed".
 */
class UpdateChecker : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Updates)
    QML_SINGLETON
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QVariantList releases READ releases NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)
    Q_PROPERTY(QDateTime lastCheck READ lastCheck NOTIFY changed)
    Q_PROPERTY(bool checkOnStart READ checkOnStart WRITE setCheckOnStart NOTIFY checkOnStartChanged)
    Q_PROPERTY(QString releasesUrl READ releasesUrl CONSTANT)
    Q_PROPERTY(QString installStatus READ installStatus NOTIFY installChanged)
    /// Share of the download done (0 … 1), -1 while its size is unknown.
    Q_PROPERTY(double installProgress READ installProgress NOTIFY installChanged)
    /// "done": the version installed; "failed": what went wrong.
    Q_PROPERTY(QString installMessage READ installMessage NOTIFY installChanged)
    /// Whether the installer's log of the last update exists.
    Q_PROPERTY(bool hasInstallLog READ hasInstallLog NOTIFY installChanged)

public:
    explicit UpdateChecker(QObject *parent);
    ~UpdateChecker() override;

    static UpdateChecker *instance();
    static UpdateChecker *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    QString status() const;
    QString currentVersion() const;
    QString latestVersion() const;
    QVariantList releases() const;
    QString errorString() const;
    QDateTime lastCheck() const;
    bool checkOnStart() const;
    void setCheckOnStart(bool check);
    QString releasesUrl() const;
    QString installStatus() const;
    double installProgress() const;
    QString installMessage() const;
    bool hasInstallLog() const;

    /// Asks GitHub for the releases (does nothing while a check is running).
    Q_INVOKABLE void check();
    /// Installs the newest release (see the class description); does nothing while one runs.
    Q_INVOKABLE void installUpdate();
    /// The PowerShell command that does the same, to run by hand if the update fails.
    Q_INVOKABLE QString updateCommand() const;
    /// Opens the installer's log of the last update.
    Q_INVOKABLE void openInstallLog() const;

    /// Called once at startup: the result of an update the installer left behind.
    void readInstallResult();
    /// Called once at startup: checks if enabled.
    void checkOnStartIfEnabled();
    /// Screenshot mode: no network, always up to date.
    void setDemoMode(bool demo);

    /// Releases newer than @p current from the JSON of GitHub's "list releases" call, newest first.
    static QVariantList newerReleases(const QByteArray &json, const QString &current, QString *error = nullptr, const QString &architecture = QStringLiteral("x64"));

Q_SIGNALS:
    void changed();
    void checkOnStartChanged();
    void installChanged();

private:
    void finished(QNetworkReply *reply);
    void setStatus(const QString &status);
    QNetworkAccessManager *network();
    void setInstallStatus(const QString &status, const QString &message = QString());
    void downloaded(QNetworkReply *reply);
    void unpack();
    void runInstaller();
    /// While installing: the installer's result, or its end without one.
    void watchInstaller();

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QString m_status = QStringLiteral("idle");
    QVariantList m_releases;
    QString m_errorString;
    QDateTime m_lastCheck;
    bool m_demo = false;

    // The running update
    QPointer<QNetworkReply> m_download;
    std::unique_ptr<QFile> m_package;
    QCryptographicHash m_packageHash{QCryptographicHash::Sha256};
    QString m_packageDigest;
    QString m_updateDirectory;
    QString m_installStatus;
    double m_installProgress = -1;
    QString m_installMessage;
    qint64 m_installer = 0; ///< process id of the installer
    QTimer m_installWatch;
};

} // namespace InfiniteDesktop
