/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "screenshotrunner.h"

#include "effectstatus.h"
#include "settingsmeta.h"

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QQuickWindow>
#include <QTimer>

namespace InfiniteDesktop
{

namespace
{
constexpr int SettleDelayMs = 450;
}

ScreenshotRunner::ScreenshotRunner(QQuickWindow *window, const QString &directory, const QSize &size, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_directory(directory)
    , m_size(size.isValid() ? size : QSize(1120, 760))
{
    const QSize narrow(640, m_size.height());
    for (const QString &scheme : {QStringLiteral("light"), QStringLiteral("dark")}) {
        addStep(scheme + QStringLiteral("-overview"), scheme + QStringLiteral(":overview"));
        for (const SettingsCategory &category : settingsCategories()) {
            const QString id = QString::fromLatin1(category.id);
            addStep(scheme + QLatin1Char('-') + id, scheme + QLatin1Char(':') + id);
        }
        addStep(scheme + QStringLiteral("-search"), scheme + QStringLiteral(":search:zoom"));
        addStep(scheme + QStringLiteral("-info-popover"), scheme + QStringLiteral(":info:mouse"));
    }
    addStep(QStringLiteral("light-narrow"), QStringLiteral("light:mouse"), narrow);
    addStep(QStringLiteral("dark-narrow-overview"), QStringLiteral("dark:overview"), narrow);
    addStep(QStringLiteral("light-safe-mode"), QStringLiteral("light:overview"), QSize(), QStringLiteral("safemode"));
    addStep(QStringLiteral("dark-turned-off"), QStringLiteral("dark:overview"), QSize(), QStringLiteral("disabled"));
    addStep(QStringLiteral("light-reset-dialog"), QStringLiteral("light:dialog"));
    addStep(QStringLiteral("light-shortcut-capture"), QStringLiteral("light:capture"));
    addStep(QStringLiteral("light-all-shortcuts"), QStringLiteral("light:allshortcuts"));
    addStep(QStringLiteral("dark-all-shortcuts"), QStringLiteral("dark:allshortcuts"));
    addStep(QStringLiteral("light-search-shortcuts"), QStringLiteral("light:search:explorer"));
    addStep(QStringLiteral("dark-color-picker"), QStringLiteral("dark:color"));
    addStep(QStringLiteral("light-dropdown"), QStringLiteral("light:dropdown"));
    addStep(QStringLiteral("light-remembered-windows"), QStringLiteral("light:remembered"));
    addStep(QStringLiteral("dark-remembered-windows"), QStringLiteral("dark:remembered"));
    addStep(QStringLiteral("light-application-list"), QStringLiteral("light:applist"));
    addStep(QStringLiteral("dark-application-list"), QStringLiteral("dark:applist"));
    addStep(QStringLiteral("light-application-picker"), QStringLiteral("light:picker"));
    addStep(QStringLiteral("dark-application-picker"), QStringLiteral("dark:picker"));
    addStep(QStringLiteral("light-no-results"), QStringLiteral("light:search:xyzzy"));
}

void ScreenshotRunner::addStep(const QString &fileName, const QString &command, const QSize &size, const QString &demoState)
{
    m_steps.append(Step{fileName, command, size.isValid() ? size : m_size, demoState.isEmpty() ? QStringLiteral("active") : demoState});
}

void ScreenshotRunner::start()
{
    QDir().mkpath(m_directory);
    QTimer::singleShot(SettleDelayMs, this, &ScreenshotRunner::runStep);
}

void ScreenshotRunner::runStep()
{
    ++m_index;
    if (!m_window || m_index >= m_steps.size()) {
        QCoreApplication::exit(m_failures == 0 ? 0 : 1);
        return;
    }
    const Step &step = m_steps.at(m_index);
    EffectStatus::instance()->setDemoState(step.demoState);
    if (m_window->size() != step.size) {
        m_window->resize(step.size);
    }
    QMetaObject::invokeMethod(m_window, "screenshotPrepare", Q_ARG(QVariant, QVariant(step.command)));
    QTimer::singleShot(SettleDelayMs, this, &ScreenshotRunner::capture);
}

void ScreenshotRunner::capture()
{
    const Step &step = m_steps.at(m_index);
    const QImage image = m_window->grabWindow();
    const QString path = QDir(m_directory).filePath(step.fileName + QStringLiteral(".png"));
    if (image.isNull() || !image.save(path)) {
        qWarning("Could not write screenshot %s", qPrintable(path));
        ++m_failures;
    } else {
        qInfo("Wrote %s", qPrintable(path));
    }
    QTimer::singleShot(0, this, &ScreenshotRunner::runStep);
}

} // namespace InfiniteDesktop
