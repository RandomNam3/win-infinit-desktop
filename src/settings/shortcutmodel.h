/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
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

namespace InfiniteDesktop
{

struct ShortcutDefinition;

/**
 * The global shortcuts of the service (shortcutdefs.h).
 *
 * They are stored in settings.ini, group [Shortcuts]: key = action id, value = the
 * combinations in the service's notation ("Win+Alt+H;…", see keycombo.h). An action
 * without an entry has its default shortcuts; an empty value unbinds it.
 */
class ShortcutModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Shortcuts)
    QML_SINGLETON
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool allDefault READ allDefault NOTIFY allDefaultChanged)
    Q_PROPERTY(bool capturing READ capturing WRITE setCapturing NOTIFY capturingChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        FullTextRole,
        DescriptionRole,
        GroupRole,
        GroupTitleRole,
        GroupIndexRole,
        SequencesRole,
        SequenceTextsRole,
        KeysRole,
        IsDefaultRole,
        DefaultKeysRole,
        DefaultTextRole,
        SearchTextRole,
        LastRole = SearchTextRole,
    };

    explicit ShortcutModel(QObject *parent);
    ~ShortcutModel() override;

    static ShortcutModel *instance();
    static ShortcutModel *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    /// Do not read or write settings.ini at all (screenshot mode): defaults are shown.
    static void setOffline(bool offline);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool available() const;
    bool allDefault() const;
    bool capturing() const;
    void setCapturing(bool capturing);

    /**
     * Interprets a key press of the capture control. Returns a map with
     * "action" (one of "modifiers", "cancel", "clear", "needsModifier",
     * "accept"), "keys" (key names for display) and for "accept" the
     * "sequence" (portable text) and "text" (native text).
     */
    Q_INVOKABLE QVariantMap interpretKey(int key, int modifiers) const;
    /// Other actions that already use @p sequence: list of {action, component, own}.
    Q_INVOKABLE QVariantList conflicts(const QString &id, const QString &sequence) const;
    /// Sets shortcut number @p index of action @p id. With @p steal it is taken away from other actions first.
    Q_INVOKABLE bool assign(const QString &id, int index, const QString &sequence, bool steal);
    Q_INVOKABLE void clear(const QString &id, int index);
    Q_INVOKABLE void resetToDefault(const QString &id);
    Q_INVOKABLE void resetAll();
    /// Key names (for key caps) of a portable key sequence.
    Q_INVOKABLE QStringList keysFor(const QString &sequence) const;
    /// The shortcuts of action @p id.
    QList<QKeySequence> currentShortcuts(const QString &id) const;
    /// Key names of the first shortcut of action @p id, empty if it has none.
    Q_INVOKABLE QStringList currentKeys(const QString &id) const;
    Q_INVOKABLE QString title(const QString &id) const;
    Q_INVOKABLE QString groupTitle(const QString &group) const;
    Q_INVOKABLE QStringList groups() const;

    static QStringList keyNames(const QKeySequence &sequence);

    /// Sets all shortcuts of action @p id (used by the "All shortcuts" page).
    void applyShortcuts(const QString &id, const QList<QKeySequence> &shortcuts);

Q_SIGNALS:
    void allDefaultChanged();
    void capturingChanged();
    void shortcutChanged(const QString &id);

private:
    struct Entry {
        const ShortcutDefinition *definition = nullptr;
        QList<QKeySequence> current;
        QString id;
        QString title;
        QString group;
        QString searchText;
    };

    int rowOf(const QString &id) const;
    bool isDefault(const Entry &entry) const;
    void apply(int row, const QList<QKeySequence> &shortcuts);
    void updateSearchText(Entry &entry);
    QList<QKeySequence> readShortcuts(const Entry &entry) const;
    void reloadFromFile();

    QList<Entry> m_entries;
    QStringList m_groups;
    bool m_available = false;
    bool m_capturing = false;
    bool m_lastAllDefault = true;
};

class ShortcutFilterModel : public FilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ShortcutFilter)
    Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY groupChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QColor highlightColor READ highlightColor WRITE setHighlightColor NOTIFY highlightColorChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        HighlightedTitleRole = ShortcutModel::LastRole + 1,
        HighlightedDescriptionRole,
    };

    explicit ShortcutFilterModel(QObject *parent = nullptr);

    QString group() const;
    void setGroup(const QString &group);
    QString query() const;
    void setQuery(const QString &query);
    QColor highlightColor() const;
    void setHighlightColor(const QColor &color);
    int count() const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void groupChanged();
    void queryChanged();
    void highlightColorChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_group;
    QString m_query;
    QStringList m_terms;
    QColor m_highlightColor = QColor(0x3d, 0xae, 0xe9);
};

} // namespace InfiniteDesktop
