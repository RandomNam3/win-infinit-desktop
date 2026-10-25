/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "effectstatus.h"

#include "apppaths.h"
#include "i18n.h"
#include "serviceconnection.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QJsonObject>
#include <QOperatingSystemVersion>
#include <QProcess>
#include <QSettings>
#include <QTimer>

using namespace std::chrono_literals;

namespace InfinitDesktop
{

namespace
{
EffectStatus *s_instance = nullptr;

#ifdef Q_OS_WIN
const QString s_runKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString s_runValue = QStringLiteral("WIN Infinit Desktop");
#endif

QString servicePath()
{
    return Wid::siblingProgram(QLatin1String(Wid::ServiceExecutable));
}
} // namespace

EffectStatus::EffectStatus(QObject *parent)
    : QObject(parent)
{
    ServiceConnection *connection = ServiceConnection::instance();
    connect(connection, &ServiceConnection::connectedChanged, this, &EffectStatus::refresh);
    connect(connection, &ServiceConnection::serviceEvent, this, [this](const QString &name, const QJsonObject &params) {
        if (name != QLatin1String("statusChanged") || m_demo) {
            return;
        }
        m_loaded = true;
        m_paused = params.value(QStringLiteral("paused")).toBool(m_paused);
        m_safeMode = params.value(QStringLiteral("safeMode")).toBool(m_safeMode);
        computeState();
    });
    QTimer::singleShot(0, this, &EffectStatus::refresh);
}

EffectStatus::~EffectStatus()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

EffectStatus *EffectStatus::instance()
{
    if (!s_instance) {
        s_instance = new EffectStatus(QCoreApplication::instance());
    }
    return s_instance;
}

EffectStatus *EffectStatus::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    EffectStatus *status = instance();
    QJSEngine::setObjectOwnership(status, QJSEngine::CppOwnership);
    return status;
}

bool EffectStatus::installed() const
{
    return m_demo || QFile::exists(servicePath());
}

bool EffectStatus::startsAtLogin() const
{
#ifdef Q_OS_WIN
    QSettings run(s_runKey, QSettings::NativeFormat);
    return run.contains(s_runValue);
#else
    return false;
#endif
}

void EffectStatus::setStartAtLogin(bool start)
{
#ifdef Q_OS_WIN
    QSettings run(s_runKey, QSettings::NativeFormat);
    if (start) {
        run.setValue(s_runValue, QLatin1Char('"') + QDir::toNativeSeparators(servicePath()) + QLatin1Char('"'));
    } else {
        run.remove(s_runValue);
    }
#else
    Q_UNUSED(start)
#endif
}

QString EffectStatus::osVersion() const
{
#ifdef Q_OS_WIN
    const QSettings info(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"), QSettings::NativeFormat);
    const QString release = info.value(QStringLiteral("DisplayVersion")).toString();
    const QString build = info.value(QStringLiteral("CurrentBuildNumber")).toString();
    if (!release.isEmpty() && !build.isEmpty()) {
        return i18nc("@info %1 is a release like 25H2, %2 a build number", "Windows 11 %1 (build %2)", release, build);
    }
#endif
    return QOperatingSystemVersion::current().name();
}

QString EffectStatus::appVersion() const
{
    return QStringLiteral(WID_VERSION_STRING);
}

void EffectStatus::setDemoState(const QString &state)
{
    m_demo = true;
    m_checked = true;
    m_loaded = state == QLatin1String("active") || state == QLatin1String("paused") || state == QLatin1String("safemode");
    m_paused = state == QLatin1String("paused");
    m_safeMode = state == QLatin1String("safemode");
    m_serviceVersion = m_loaded ? appVersion() : QString();
    m_state = state;
    Q_EMIT changed();
}

void EffectStatus::refresh()
{
    if (m_demo) {
        return;
    }
    ServiceConnection::instance()->call(QStringLiteral("state"), {}, this, [this](const QJsonValue &result, const QString &error) {
        m_checked = true;
        if (!error.isEmpty() || !result.isObject()) {
            m_loaded = false;
            m_serviceVersion.clear();
        } else {
            const QJsonObject state = result.toObject();
            m_loaded = true;
            m_paused = state.value(QStringLiteral("paused")).toBool();
            m_safeMode = state.value(QStringLiteral("safeMode")).toBool();
            m_serviceVersion = state.value(QStringLiteral("version")).toString();
        }
        computeState();
    });
}

void EffectStatus::computeState()
{
    QString state;
    if (!m_checked) {
        state = QStringLiteral("unknown");
    } else if (m_loaded) {
        state = m_safeMode ? QStringLiteral("safemode") : m_paused ? QStringLiteral("paused") : QStringLiteral("active");
    } else if (!installed()) {
        state = QStringLiteral("notinstalled");
    } else {
        state = QStringLiteral("disabled");
    }
    if (m_target >= 0 && (m_loaded == (m_target == 1) || state == QLatin1String("notinstalled"))) {
        // Reached the state that was asked for (or nothing more will happen).
        m_busy = false;
        m_target = -1;
    }
    m_state = state;
    Q_EMIT changed();
}

void EffectStatus::setEnabled(bool enabled)
{
    if (m_demo || m_busy) {
        return;
    }
    if (enabled) {
        if (!installed()) {
            Q_EMIT actionFailed(i18n("The canvas service is not installed next to this app."));
            return;
        }
        setStartAtLogin(true);
        if (!m_loaded && !QProcess::startDetached(servicePath(), {})) {
            Q_EMIT actionFailed(i18n("The canvas service could not be started."));
            return;
        }
    } else {
        setStartAtLogin(false);
        ServiceConnection::instance()->call(QStringLiteral("quit"));
    }
    m_busy = true;
    m_target = enabled ? 1 : 0;
    Q_EMIT changed();
    // The connection follows the service (connectedChanged); give up waiting after a while.
    QTimer::singleShot(8s, this, [this]() {
        if (m_busy) {
            m_busy = false;
            m_target = -1;
            refresh();
            Q_EMIT changed();
        }
    });
}

void EffectStatus::leaveSafeMode()
{
    if (m_demo) {
        return;
    }
    ServiceConnection::instance()->call(QStringLiteral("leaveSafeMode"), {}, this, [this](const QJsonValue &, const QString &error) {
        if (!error.isEmpty()) {
            Q_EMIT actionFailed(i18n("Safe mode could not be left: %1", error));
        }
        refresh();
    });
}

void EffectStatus::setPaused(bool paused)
{
    if (m_demo) {
        m_paused = paused;
        m_state = paused ? QStringLiteral("paused") : QStringLiteral("active");
        Q_EMIT changed();
        return;
    }
    ServiceConnection::instance()->call(QStringLiteral("setPaused"), {{QStringLiteral("paused"), paused}}, this, [this](const QJsonValue &, const QString &) {
        refresh();
    });
}

} // namespace InfinitDesktop
