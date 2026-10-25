/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "searchutil.h"

#include <QAbstractListModel>
#include <QColor>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QThread;
class QQmlEngine;

namespace InfinitDesktop
{

/**
 * The applications of the Start menu (the shell's AppsFolder), used to show names and
 * icons for application keys and to pick applications for groups and the exclusion lists.
 *
 * @c appId is how an application is started: its AppUserModelID, or the path of the
 * program for classic programs without one. The service's application key is the same
 * text in lower case. Read on first use in a worker thread (the shell takes a moment).
 */
class ApplicationIndex : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Applications)
    QML_SINGLETON
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        AppIdRole = Qt::UserRole + 1,
        NameRole,
        GenericNameRole,
        IconNameRole,
        HasIconRole,
        WindowClassRole,
        NoDisplayRole,
        SearchTextRole,
        LastRole = SearchTextRole,
    };

    explicit ApplicationIndex(QObject *parent);
    ~ApplicationIndex() override;

    static ApplicationIndex *instance();
    static ApplicationIndex *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

    /**
     * Name and icon for a window class or desktop file name:
     * {id, name, iconName, hasIcon, known}. Unknown ids keep the id as name.
     */
    Q_INVOKABLE QVariantMap describe(const QString &id);
    /// Starts reading the Start menu (once); countChanged() when done.
    Q_INVOKABLE void load();
    /// Example applications instead of the Start menu (screenshots).
    static void setDemoMode(bool demo);

    /// Icon id for the "themeicon" image provider of a Start menu item or a program path.
    static QString shellIconName(const QString &parsingName);
    static QString fileIconName(const QString &path);

Q_SIGNALS:
    void countChanged();

private:
    struct Entry {
        /// AppUserModelID or program path, as it is started.
        QString id;
        /// id in lower case: the service's application key.
        QString key;
        QString name;
        QString genericName;
        QString iconName;
        QString windowClass;
        bool noDisplay = false;
        bool hasIcon = false;
        QString searchText;
    };

    int find(const QString &id) const;
    void setEntries(QList<Entry> entries);
    static QList<Entry> demoEntries();
    /// Reads the Start menu (Windows; empty elsewhere). Runs in the worker thread.
    static QList<Entry> readStartMenu();

    QList<Entry> m_entries;
    bool m_loaded = false;
    QThread *m_worker = nullptr;
};

/// Filters the application index for the picker.
class ApplicationFilterModel : public FilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ApplicationFilter)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(QColor highlightColor READ highlightColor WRITE setHighlightColor NOTIFY highlightColorChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        HighlightedNameRole = ApplicationIndex::LastRole + 1,
    };

    explicit ApplicationFilterModel(QObject *parent = nullptr);

    QString query() const;
    void setQuery(const QString &query);
    bool showHidden() const;
    void setShowHidden(bool showHidden);
    QColor highlightColor() const;
    void setHighlightColor(const QColor &color);
    int count() const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void queryChanged();
    void showHiddenChanged();
    void highlightColorChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_query;
    QStringList m_terms;
    bool m_showHidden = false;
    QColor m_highlightColor = QColor(0x3d, 0xae, 0xe9);
};

} // namespace InfinitDesktop
