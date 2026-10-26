/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later

    wid-settings.exe: the settings app, the same Qt Quick UI as in KDE Infinite Desktop.
*/
#include "applicationindex.h"
#include "apppaths.h"
#include "effectstatus.h"
#include "globalshortcutsmodel.h"
#include "i18n.h"
#include "iconprovider.h"
#include "legacy.h"
#include "localizer.h"
#include "screenshotrunner.h"
#include "serviceconnection.h"
#include "settingsfile.h"
#include "shortcutmodel.h"
#include "themecontroller.h"
#include "updatechecker.h"
#include "windowplacements.h"
#include "workspacemodel.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>

#include "pipenames.h"

#ifdef Q_OS_WIN
#include <dwmapi.h>
#endif

using namespace InfiniteDesktop;

namespace
{

/// Windows draws the title bar: it follows the light or dark look of the app.
void applyTitleBarTheme(QWindow *window)
{
#ifdef Q_OS_WIN
    const BOOL dark = ThemeController::instance()->dark() ? TRUE : FALSE;
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (SUCCEEDED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark))) && window->isVisible()) {
        // Without a frame change an open window keeps its old title bar until it is activated.
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
    }
#else
    Q_UNUSED(window)
#endif
}

QSize parseSize(const QString &text)
{
    const QStringList parts = text.split(QLatin1Char('x'));
    if (parts.size() != 2) {
        return QSize();
    }
    return QSize(parts.at(0).toInt(), parts.at(1).toInt());
}

/// One settings window per session: a second start brings the first one to the front.
QString instanceServerName()
{
    return Wid::ipcServerName() + QStringLiteral("-settings");
}

bool activateRunningInstance(const QString &page)
{
    QLocalSocket socket;
    socket.connectToServer(instanceServerName());
    if (!socket.waitForConnected(300)) {
        return false;
    }
    socket.write(page.toUtf8().left(64) + '\n');
    socket.waitForBytesWritten(300);
    socket.disconnectFromServer();
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("wid-settings"));
    QCoreApplication::setOrganizationName(QStringLiteral("WIN Infinite Desktop"));
    QCoreApplication::setApplicationVersion(QStringLiteral(WID_VERSION_STRING));
    // Before the settings are read: the folders of WIN Infinit Desktop 0.2 and older.
    Wid::migrateLegacyData(QString::fromLatin1(Wid::AppSettingsGroup));
    Wid::Localizer::instance().loadForSystemLanguage();
    QGuiApplication::setApplicationDisplayName(i18nc("@title application name", "WIN Infinite Desktop"));

    QCommandLineParser parser;
    parser.setApplicationDescription(i18n("Settings for the infinite canvas of Windows"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption pageOption(QStringLiteral("page"), i18n("Open the given page, for example “mouse” or “shortcuts”."), i18nc("@info:shell value name", "page"));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                              i18n("Render every page into PNG files in <directory> and quit (for documentation)."),
                                              i18nc("@info:shell value name", "directory"));
    const QCommandLineOption screenshotSizeOption(QStringLiteral("screenshot-size"), i18n("Window size for --screenshot, for example 1120x760."),
                                                  i18nc("@info:shell value name", "size"));
    parser.addOption(pageOption);
    parser.addOption(screenshotOption);
    parser.addOption(screenshotSizeOption);
    parser.process(app);

    const bool screenshotMode = parser.isSet(screenshotOption);
    QTemporaryDir screenshotConfig;
    std::unique_ptr<QLocalServer> instanceServer;
    if (screenshotMode) {
        // Never touch the real settings.ini and never talk to the service while rendering screenshots.
        SettingsFile::setPath(QDir(screenshotConfig.path()).filePath(QStringLiteral("settings.ini")));
        ServiceConnection::setOffline(true);
        ShortcutModel::setOffline(true);
        ApplicationIndex::setDemoMode(true);
        GlobalShortcutsModel::setDemoMode(true);
        WindowPlacements::setDemoMode(true);
        WorkspaceModel::setDemoMode(true);
        if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        }
        QFont font(QStringLiteral("Segoe UI"));
        font.setPointSizeF(10);
        QGuiApplication::setFont(font);
    } else {
        if (activateRunningInstance(parser.value(pageOption))) {
            return 0;
        }
        instanceServer = std::make_unique<QLocalServer>();
        instanceServer->setSocketOptions(QLocalServer::UserAccessOption);
        if (!instanceServer->listen(instanceServerName())) {
            QLocalServer::removeServer(instanceServerName());
            instanceServer->listen(instanceServerName());
        }
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/infinitedesktop/icons/app-icon.svg")));

    ThemeController::instance()->setScreenshotMode(screenshotMode);
    if (screenshotMode) {
        ThemeController::instance()->setForcedAccent(QColor(0x00, 0x67, 0xc0));
        EffectStatus::instance()->setDemoState(QStringLiteral("active"));
        UpdateChecker::instance()->setDemoMode(true);
    }

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("idicon"), new IconProvider);
    engine.addImageProvider(QStringLiteral("themeicon"), new ThemeIconProvider);
    engine.rootContext()->setContextObject(new Wid::LocalizedContext(&engine));
    engine.setInitialProperties({{QStringLiteral("initialPage"), parser.value(pageOption)}});
    engine.loadFromModule("InfiniteDesktop.Settings", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (window && !screenshotMode) {
        applyTitleBarTheme(window);
        QObject::connect(ThemeController::instance(), &ThemeController::changed, window, [window]() {
            applyTitleBarTheme(window);
        });
    }

    if (screenshotMode) {
        if (!window) {
            return 1;
        }
        auto *runner = new ScreenshotRunner(window, QDir(parser.value(screenshotOption)).absolutePath(), parseSize(parser.value(screenshotSizeOption)), &app);
        runner->start();
    } else {
        if (instanceServer && window) {
            const QPointer<QQuickWindow> target(window);
            QObject::connect(instanceServer.get(), &QLocalServer::newConnection, &app, [server = instanceServer.get(), target]() {
                while (QLocalSocket *socket = server->nextPendingConnection()) {
                    QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
                    QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, target]() {
                        const QString page = QString::fromUtf8(socket->readLine(128)).trimmed();
                        if (!target) {
                            return;
                        }
                        if (!page.isEmpty()) {
                            QMetaObject::invokeMethod(target, "openPage", Q_ARG(QVariant, QVariant(page)));
                        }
                        target->showNormal();
                        target->raise();
                        target->requestActivate();
                    });
                }
            });
        }
        UpdateChecker::instance()->readInstallResult();
        UpdateChecker::instance()->checkOnStartIfEnabled();
    }

    return app.exec();
}
