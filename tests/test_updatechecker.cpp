/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 KDE Infinite Desktop and WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
// Release list of the update check (GitHub's "list releases" answer).

#include "updatechecker.h"

#include <QTest>

using InfiniteDesktop::UpdateChecker;

class TestUpdateChecker : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void newerReleasesOnly()
    {
        // "\/" instead of "/" in the URLs: moc reads "//" in a raw string as a comment and skips the class
        const QByteArray json = R"([
            {"tag_name": "v0.3.0", "name": "WIN Infinite Desktop 0.3.0", "body": "Notes 0.3.0\n", "html_url": "https:\/\/example.org/0.3.0",
             "published_at": "2026-10-25T10:00:00Z", "draft": false, "prerelease": false,
             "assets": [
                {"name": "win-infinite-desktop-0.3.0-arm64.zip", "browser_download_url": "https:\/\/github.com/o/r/releases/download/v0.3.0/win-infinite-desktop-0.3.0-arm64.zip"},
                {"name": "win-infinite-desktop-0.3.0-x64.zip", "browser_download_url": "https:\/\/github.com/o/r/releases/download/v0.3.0/win-infinite-desktop-0.3.0-x64.zip",
                 "digest": "sha256:9F86D081884C7D659A2FEAA0C55AD015A3BF4F1B2B0B822CD15D6C15B0F00A08"}
             ]},
            {"tag_name": "v0.2.0", "body": "old", "draft": false, "prerelease": false},
            {"tag_name": "v0.1.0", "body": "older", "draft": false, "prerelease": false},
            {"tag_name": "v0.4.0", "body": "draft", "draft": true, "prerelease": false},
            {"tag_name": "v0.5.0", "body": "beta", "draft": false, "prerelease": true},
            {"tag_name": "v9.9.9; rm -rf ~", "body": "", "draft": false, "prerelease": false},
            {"tag_name": "v0.10.0", "body": "Notes 0.10.0", "draft": false, "prerelease": false,
             "assets": [{"name": "win-infinite-desktop-0.10.0-x64.zip", "browser_download_url": "https:\/\/evil.example/win-infinite-desktop-0.10.0-x64.zip"}]}
        ])";
        QString error;
        const QVariantList releases = UpdateChecker::newerReleases(json, QStringLiteral("0.2.0"), &error, QStringLiteral("x64"));
        QVERIFY(error.isEmpty());
        QCOMPARE(releases.size(), 2);
        // newest first, compared as versions (0.10.0 > 0.3.0)
        const QVariantMap first = releases.at(0).toMap();
        QCOMPARE(first.value(QStringLiteral("version")).toString(), QStringLiteral("0.10.0"));
        // A download from anywhere but GitHub is never used.
        QVERIFY(!first.contains(QStringLiteral("package")));
        const QVariantMap second = releases.at(1).toMap();
        QCOMPARE(second.value(QStringLiteral("version")).toString(), QStringLiteral("0.3.0"));
        QCOMPARE(second.value(QStringLiteral("notes")).toString(), QStringLiteral("Notes 0.3.0"));
        QCOMPARE(second.value(QStringLiteral("url")).toString(), QStringLiteral("https://example.org/0.3.0"));
        QCOMPARE(second.value(QStringLiteral("package")).toString(), QStringLiteral("https://github.com/o/r/releases/download/v0.3.0/win-infinite-desktop-0.3.0-x64.zip"));
        QVERIFY(second.value(QStringLiteral("date")).toDateTime().isValid());
        // The SHA-256 GitHub reports for the package, to check the download against.
        QCOMPARE(second.value(QStringLiteral("packageDigest")).toString(), QStringLiteral("sha256:9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"));

        const QVariantList arm = UpdateChecker::newerReleases(json, QStringLiteral("0.2.0"), &error, QStringLiteral("arm64"));
        QCOMPARE(arm.at(1).toMap().value(QStringLiteral("package")).toString(),
                 QStringLiteral("https://github.com/o/r/releases/download/v0.3.0/win-infinite-desktop-0.3.0-arm64.zip"));
        // No digest published: nothing to compare.
        QVERIFY(!arm.at(1).toMap().contains(QStringLiteral("packageDigest")));
    }

    void upToDate()
    {
        const QByteArray json = R"([{"tag_name": "v0.2.0", "draft": false, "prerelease": false}])";
        QString error;
        QVERIFY(UpdateChecker::newerReleases(json, QStringLiteral("0.2.0"), &error).isEmpty());
        QVERIFY(error.isEmpty());
        QVERIFY(UpdateChecker::newerReleases("[]", QStringLiteral("0.2.0"), &error).isEmpty());
        QVERIFY(error.isEmpty());
    }

    void unexpectedAnswer()
    {
        QString error;
        QVERIFY(UpdateChecker::newerReleases(R"({"message": "API rate limit exceeded"})", QStringLiteral("0.2.0"), &error).isEmpty());
        QVERIFY(!error.isEmpty());
        error.clear();
        QVERIFY(UpdateChecker::newerReleases("not json", QStringLiteral("0.2.0"), &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestUpdateChecker)
#include "test_updatechecker.moc"
