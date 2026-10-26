/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

namespace InfiniteDesktop
{

/**
 * The window positions the service remembers per application
 * (windowPlacements / forgetWindowPlacements on the service pipe).
 *
 * @c applications is a list of maps: name, iconName, hasIcon, appKey,
 * windowCount, summary and windows (list of maps: screen, size, flags, time).
 */
class WindowPlacements : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RememberedWindows)
    QML_SINGLETON
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QVariantList applications READ applications NOTIFY changed)
    Q_PROPERTY(int windowCount READ windowCount NOTIFY changed)

public:
    explicit WindowPlacements(QObject *parent);

    static WindowPlacements *instance();
    static WindowPlacements *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static void setDemoMode(bool demo);

    bool available() const;
    bool loading() const;
    QVariantList applications() const;
    int windowCount() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void forgetAll();
    /// Forgets the remembered windows of one application (key of the "applications" object).
    Q_INVOKABLE void forgetApplication(const QString &appKey);

    /// Parses the JSON document of windowPlacements(); public for testing.
    void parse(const QByteArray &json);

Q_SIGNALS:
    void changed();
    void forgotten();
    /// One application was forgotten, @p name is its display name.
    void applicationForgotten(const QString &name);
    void failed(const QString &message);

private:
    bool m_available = false;
    bool m_loading = false;
    QVariantList m_applications;
    int m_windowCount = 0;
};

} // namespace InfiniteDesktop
