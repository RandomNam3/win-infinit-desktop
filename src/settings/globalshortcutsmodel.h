/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "searchutil.h"

#include <QAbstractListModel>
#include <QColor>
#include <QKeySequence>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

namespace InfinitDesktop
{

/**
 * The shortcuts of WIN Infinit Desktop together with the documented default shortcuts of
 * Windows 11 (shell, Snap, virtual desktops, accessibility, Game Bar). Windows has no
 * interface to read or change them, so they come from a fixed table and only our own can
 * be edited.
 *
 * Key sequences used by more than one action are flagged as conflicts.
 */
class GlobalShortcutsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GlobalShortcuts)
    QML_SINGLETON
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(int conflictCount READ conflictCount NOTIFY conflictsChanged)
    Q_PROPERTY(int componentCount READ componentCount NOTIFY countsChanged)
    Q_PROPERTY(int count READ count NOTIFY countsChanged)

public:
    enum Role {
        KeyRole = Qt::UserRole + 1,
        ComponentIdRole,
        ComponentNameRole,
        ActionIdRole,
        ActionNameRole,
        SequencesRole,
        SequenceTextsRole,
        KeysRole,
        DefaultTextRole,
        IsDefaultRole,
        HasConflictRole,
        ConflictTextRole,
        IsOwnRole,
        EditableRole,
        SearchTextRole,
        LastRole = SearchTextRole,
    };

    explicit GlobalShortcutsModel(QObject *parent);

    static GlobalShortcutsModel *instance();
    static GlobalShortcutsModel *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    /// Screenshot mode: no D-Bus, show an example data set.
    static void setDemoMode(bool demo);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool available() const;
    bool loading() const;
    int conflictCount() const;
    int componentCount() const;
    int count() const;

    /// Reads everything again from KGlobalAccel (asynchronously).
    Q_INVOKABLE void reload();
    /// Other actions using @p sequence: list of {action, component, key}.
    Q_INVOKABLE QVariantList conflicts(const QString &key, const QString &sequence) const;
    Q_INVOKABLE bool assign(const QString &key, int slot, const QString &sequence, bool steal);
    Q_INVOKABLE void clear(const QString &key, int slot);
    Q_INVOKABLE void resetToDefault(const QString &key);
    /// Opens the shortcut page of System Settings.
    Q_INVOKABLE bool openSystemSettings() const;
    /// Windows shortcuts that use @p sequence: list of {action, component, own: false}.
    QVariantList systemConflicts(const QKeySequence &sequence);

    /// Called by ShortcutModel after one of our own actions changed.
    void updateOwnAction(const QString &actionId, const QList<QKeySequence> &keys);

Q_SIGNALS:
    void availableChanged();
    void loadingChanged();
    void conflictsChanged();
    void countsChanged();
    void changeFailed(const QString &message);

private:
    struct Entry {
        QString componentId;
        QString componentName;
        QString actionId;
        QString actionName;
        QList<QKeySequence> keys;
        QList<QKeySequence> defaultKeys;
        bool own = false;
        bool conflict = false;
        QString conflictText;
        QString searchText;
    };

    static QString keyOf(const QString &componentId, const QString &actionId);
    int rowOf(const QString &key) const;
    void setEntries(QList<Entry> entries);
    void updateConflicts();
    void updateSearchText(Entry &entry);
    bool setKeys(int row, const QList<QKeySequence> &keys);
    QList<Entry> windowsShortcuts();

    QList<Entry> m_entries;
    bool m_available = false;
    bool m_loading = false;
    int m_conflictCount = 0;
    int m_componentCount = 0;
};

class GlobalShortcutsFilterModel : public FilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GlobalShortcutsFilter)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool onlyConflicts READ onlyConflicts WRITE setOnlyConflicts NOTIFY onlyConflictsChanged)
    Q_PROPERTY(bool excludeOwn READ excludeOwn WRITE setExcludeOwn NOTIFY excludeOwnChanged)
    Q_PROPERTY(QColor highlightColor READ highlightColor WRITE setHighlightColor NOTIFY highlightColorChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        HighlightedActionNameRole = GlobalShortcutsModel::LastRole + 1,
        FirstInComponentRole,
        LastInComponentRole,
    };

    explicit GlobalShortcutsFilterModel(QObject *parent = nullptr);

    QString query() const;
    void setQuery(const QString &query);
    bool onlyConflicts() const;
    void setOnlyConflicts(bool onlyConflicts);
    bool excludeOwn() const;
    void setExcludeOwn(bool excludeOwn);
    QColor highlightColor() const;
    void setHighlightColor(const QColor &color);
    int count() const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void queryChanged();
    void onlyConflictsChanged();
    void excludeOwnChanged();
    void highlightColorChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_query;
    QStringList m_terms;
    bool m_onlyConflicts = false;
    bool m_excludeOwn = false;
    QColor m_highlightColor = QColor(0x3d, 0xae, 0xe9);
};

} // namespace InfinitDesktop
