/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "shortcutmodel.h"

#include "apppaths.h"
#include "globalshortcutsmodel.h"
#include "i18n.h"
#include "keycombo.h"
#include "serviceconnection.h"
#include "settingsfile.h"
#include "settingsmeta.h"
#include "shortcutdefs.h"

#include <QCoreApplication>
#include <QJSEngine>

#include <algorithm>

namespace InfinitDesktop
{

namespace
{

ShortcutModel *s_shortcutInstance = nullptr;
bool s_offline = false;

const QString s_group = QLatin1String(Wid::AppShortcutsGroup);

QString stripPrefix(const QString &text)
{
    // "Infinit Desktop: Zoom In" is good in the list of all shortcuts, but
    // redundant inside our own app.
    const int colon = text.indexOf(QLatin1String(": "));
    if (colon > 0 && colon <= 24) {
        return text.mid(colon + 2);
    }
    return text;
}

QList<QKeySequence> cleaned(const QList<QKeySequence> &list)
{
    // Only what the service can bind, each once (as the service reads it back).
    return KeyCombo::fromList(KeyCombo::toList(list));
}

QStringList toPortable(const QList<QKeySequence> &list)
{
    QStringList result;
    for (const QKeySequence &sequence : list) {
        result.append(sequence.toString(QKeySequence::PortableText));
    }
    return result;
}

QStringList toNative(const QList<QKeySequence> &list)
{
    QStringList result;
    for (const QKeySequence &sequence : list) {
        result.append(sequence.toString(QKeySequence::NativeText));
    }
    return result;
}

QVariantList toKeyLists(const QList<QKeySequence> &list)
{
    QVariantList result;
    for (const QKeySequence &sequence : list) {
        result.append(ShortcutModel::keyNames(sequence));
    }
    return result;
}

QString keyName(Qt::Key key)
{
    switch (key) {
    case Qt::Key_Left:
        return QStringLiteral("←");
    case Qt::Key_Up:
        return QStringLiteral("↑");
    case Qt::Key_Right:
        return QStringLiteral("→");
    case Qt::Key_Down:
        return QStringLiteral("↓");
    case Qt::Key_Space:
        return i18nc("keyboard key", "Space");
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return i18nc("keyboard key", "Enter");
    case Qt::Key_Escape:
        return i18nc("keyboard key", "Esc");
    case Qt::Key_Backspace:
        return i18nc("keyboard key", "Backspace");
    case Qt::Key_Delete:
        return i18nc("keyboard key", "Del");
    case Qt::Key_Insert:
        return i18nc("keyboard key", "Ins");
    case Qt::Key_PageUp:
        return i18nc("keyboard key", "PgUp");
    case Qt::Key_PageDown:
        return i18nc("keyboard key", "PgDown");
    case Qt::Key_Home:
        return i18nc("keyboard key", "Home");
    case Qt::Key_End:
        return i18nc("keyboard key", "End");
    case Qt::Key_Print:
        return i18nc("keyboard key", "Print");
    default:
        return QKeySequence(key).toString(QKeySequence::NativeText);
    }
}

QStringList modifierNames(Qt::KeyboardModifiers modifiers)
{
    QStringList names;
    if (modifiers & Qt::MetaModifier) {
        names.append(i18nc("keyboard key", "Win"));
    }
    if (modifiers & Qt::ControlModifier) {
        names.append(i18nc("keyboard key", "Ctrl"));
    }
    if (modifiers & Qt::AltModifier) {
        names.append(i18nc("keyboard key", "Alt"));
    }
    if (modifiers & Qt::ShiftModifier) {
        names.append(i18nc("keyboard key", "Shift"));
    }
    return names;
}

bool isModifierKey(int key)
{
    switch (key) {
    case 0:
    case Qt::Key_unknown:
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
        return true;
    default:
        return false;
    }
}

bool isShiftAsModifierAllowed(int key)
{
    // Shift + "5" arrives as "%" on many layouts, so Shift is only kept for
    // keys where it does not change the produced symbol (same rule as KKeySequenceWidget).
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35) {
        return true;
    }
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return true;
    }
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
    case Qt::Key_Backspace:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
    case Qt::Key_Escape:
    case Qt::Key_Print:
    case Qt::Key_Pause:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
    case Qt::Key_Insert:
    case Qt::Key_Delete:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
        return true;
    default:
        // Media and launch keys.
        return key >= Qt::Key_Back && key < Qt::Key_unknown;
    }
}

bool isOkWithoutModifier(int key)
{
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35) {
        return true;
    }
    switch (key) {
    case Qt::Key_Print:
    case Qt::Key_Pause:
        return true;
    default:
        return key >= Qt::Key_Back && key < Qt::Key_unknown;
    }
}

} // namespace

QStringList ShortcutModel::keyNames(const QKeySequence &sequence)
{
    QStringList names;
    for (int i = 0; i < sequence.count(); ++i) {
        if (i > 0) {
            names.append(QStringLiteral(","));
        }
        const QKeyCombination combination = sequence[i];
        names.append(modifierNames(combination.keyboardModifiers()));
        names.append(keyName(combination.key()));
    }
    return names;
}

ShortcutModel::ShortcutModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // The shortcuts live in settings.ini, group [Shortcuts]; the service reads them.
    m_available = true;
    for (const ShortcutDefinition &definition : shortcutDefinitions()) {
        Entry entry;
        entry.definition = &definition;
        entry.id = QString::fromLatin1(definition.id);
        entry.title = stripPrefix(definition.text.toString());
        entry.group = QString::fromLatin1(definition.group);
        entry.current = definition.defaultShortcuts;
        if (!m_groups.contains(entry.group)) {
            m_groups.append(entry.group);
        }

        if (!s_offline) {
            entry.current = readShortcuts(entry);
        }
        updateSearchText(entry);
        m_entries.append(entry);
    }

    if (!s_offline) {
        connect(SettingsFile::instance(), &SettingsFile::changedExternally, this, &ShortcutModel::reloadFromFile);
    }
    m_lastAllDefault = allDefault();

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
        setCapturing(false);
    });
}

ShortcutModel::~ShortcutModel()
{
    setCapturing(false);
}

ShortcutModel *ShortcutModel::instance()
{
    if (!s_shortcutInstance) {
        s_shortcutInstance = new ShortcutModel(QCoreApplication::instance());
    }
    return s_shortcutInstance;
}

ShortcutModel *ShortcutModel::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    ShortcutModel *model = instance();
    QJSEngine::setObjectOwnership(model, QJSEngine::CppOwnership);
    return model;
}

void ShortcutModel::setOffline(bool offline)
{
    s_offline = offline;
}

int ShortcutModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QHash<int, QByteArray> ShortcutModel::roleNames() const
{
    return {
        {IdRole, "shortcutId"},
        {TitleRole, "title"},
        {FullTextRole, "fullText"},
        {DescriptionRole, "description"},
        {GroupRole, "group"},
        {GroupTitleRole, "groupTitle"},
        {GroupIndexRole, "groupIndex"},
        {SequencesRole, "sequences"},
        {SequenceTextsRole, "sequenceTexts"},
        {KeysRole, "keys"},
        {IsDefaultRole, "isDefault"},
        {DefaultKeysRole, "defaultKeys"},
        {DefaultTextRole, "defaultText"},
        {SearchTextRole, "searchText"},
    };
}

QVariant ShortcutModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return QVariant();
    }
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return entry.title;
    case IdRole:
        return entry.id;
    case FullTextRole:
        return entry.definition->text.toString();
    case DescriptionRole:
        return entry.definition->description.toString();
    case GroupRole:
        return entry.group;
    case GroupTitleRole:
        return groupTitle(entry.group);
    case GroupIndexRole:
        return int(m_groups.indexOf(entry.group));
    case SequencesRole:
        return toPortable(entry.current);
    case SequenceTextsRole:
        return toNative(entry.current);
    case KeysRole:
        return toKeyLists(entry.current);
    case IsDefaultRole:
        return isDefault(entry);
    case DefaultKeysRole:
        return toKeyLists(cleaned(entry.definition->defaultShortcuts));
    case DefaultTextRole: {
        const QStringList texts = toNative(cleaned(entry.definition->defaultShortcuts));
        return texts.isEmpty() ? i18nc("@label no keyboard shortcut assigned", "None") : texts.join(i18nc("@label separator between alternative shortcuts", ", "));
    }
    case SearchTextRole:
        return entry.searchText;
    }
    return QVariant();
}

void ShortcutModel::updateSearchText(Entry &entry)
{
    entry.searchText = SearchUtil::normalize(QStringList{entry.title,
                                                         entry.definition->description.toString(),
                                                         groupTitle(entry.group),
                                                         toNative(entry.current).join(QLatin1Char(' ')),
                                                         i18nc("search keywords for shortcuts", "shortcut,keyboard,key,hotkey")}
                                                 .join(QLatin1Char(' ')));
}

int ShortcutModel::rowOf(const QString &id) const
{
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).id == id) {
            return row;
        }
    }
    return -1;
}

bool ShortcutModel::available() const
{
    return m_available;
}

bool ShortcutModel::isDefault(const Entry &entry) const
{
    // Compared as the service sees them ("+" and "=" are one key for it).
    return KeyCombo::toList(entry.current) == KeyCombo::toList(entry.definition->defaultShortcuts);
}

QList<QKeySequence> ShortcutModel::readShortcuts(const Entry &entry) const
{
    const SettingsFile *file = SettingsFile::instance();
    if (!file->contains(s_group, entry.id)) {
        return cleaned(entry.definition->defaultShortcuts);
    }
    // An empty value unbinds the action.
    return KeyCombo::fromList(file->value(s_group, entry.id).toString());
}

bool ShortcutModel::allDefault() const
{
    return std::all_of(m_entries.cbegin(), m_entries.cend(), [this](const Entry &entry) {
        return isDefault(entry);
    });
}

bool ShortcutModel::capturing() const
{
    return m_capturing;
}

void ShortcutModel::setCapturing(bool capturing)
{
    if (m_capturing == capturing) {
        return;
    }
    m_capturing = capturing;
    if (!s_offline) {
        // Keep the service from triggering the shortcuts we want to record.
        ServiceConnection::instance()->call(QStringLiteral("blockShortcuts"), {{QStringLiteral("blocked"), capturing}});
    }
    Q_EMIT capturingChanged();
}

QVariantMap ShortcutModel::interpretKey(int key, int modifiers) const
{
    Qt::KeyboardModifiers mods = Qt::KeyboardModifiers(modifiers) & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    QVariantMap result;

    if (isModifierKey(key)) {
        result.insert(QStringLiteral("action"), QStringLiteral("modifiers"));
        result.insert(QStringLiteral("keys"), modifierNames(mods));
        return result;
    }
    if (mods == Qt::NoModifier && key == Qt::Key_Escape) {
        result.insert(QStringLiteral("action"), QStringLiteral("cancel"));
        return result;
    }
    if (mods == Qt::NoModifier && (key == Qt::Key_Backspace || key == Qt::Key_Delete)) {
        result.insert(QStringLiteral("action"), QStringLiteral("clear"));
        return result;
    }
    if (key == Qt::Key_Backtab) {
        key = Qt::Key_Tab;
        mods |= Qt::ShiftModifier;
    }
    if ((mods & Qt::ShiftModifier) && !isShiftAsModifierAllowed(key)) {
        mods &= ~Qt::ShiftModifier;
    }

    const QKeySequence sequence(QKeyCombination(mods, Qt::Key(key)));
    result.insert(QStringLiteral("keys"), keyNames(sequence));
    if ((mods & ~Qt::ShiftModifier) == Qt::NoModifier && !isOkWithoutModifier(key)) {
        // A global shortcut without Win/Ctrl/Alt would break typing.
        result.insert(QStringLiteral("action"), QStringLiteral("needsModifier"));
        return result;
    }
    if (!KeyCombo::isSupported(sequence)) {
        // Media keys, Print and the like: the service cannot bind them.
        result.insert(QStringLiteral("action"), QStringLiteral("needsModifier"));
        return result;
    }
    result.insert(QStringLiteral("action"), QStringLiteral("accept"));
    result.insert(QStringLiteral("sequence"), sequence.toString(QKeySequence::PortableText));
    result.insert(QStringLiteral("text"), sequence.toString(QKeySequence::NativeText));
    return result;
}

QVariantList ShortcutModel::conflicts(const QString &id, const QString &sequenceText) const
{
    QVariantList result;
    // As the service reads it back ("+" next to backspace is the "=" key for it).
    const QKeySequence sequence = cleaned({QKeySequence::fromString(sequenceText, QKeySequence::PortableText)}).value(0);
    if (sequence.isEmpty()) {
        return result;
    }
    QStringList ownIds;
    for (const Entry &entry : m_entries) {
        ownIds.append(entry.id);
        if (entry.id != id && entry.current.contains(sequence)) {
            result.append(QVariantMap{
                {QStringLiteral("action"), entry.title},
                {QStringLiteral("component"), i18nc("@label name of this plugin", "Infinit Desktop")},
                {QStringLiteral("own"), true},
            });
        }
    }
    // Shortcuts of Windows itself: ours win while the service runs, theirs stop working.
    result.append(GlobalShortcutsModel::instance()->systemConflicts(sequence));
    return result;
}

bool ShortcutModel::assign(const QString &id, int index, const QString &sequenceText, bool steal)
{
    const int row = rowOf(id);
    // As the service reads it back ("+" next to backspace is the "=" key for it).
    const QKeySequence sequence = cleaned({QKeySequence::fromString(sequenceText, QKeySequence::PortableText)}).value(0);
    if (row < 0 || sequence.isEmpty() || index < 0) {
        return false;
    }

    if (steal) {
        for (int other = 0; other < m_entries.size(); ++other) {
            if (other != row && m_entries.at(other).current.contains(sequence)) {
                QList<QKeySequence> list = m_entries.at(other).current;
                list.removeAll(sequence);
                apply(other, list);
            }
        }
    }

    QList<QKeySequence> list = m_entries.at(row).current;
    list.removeAll(sequence);
    if (index < list.size()) {
        list[index] = sequence;
    } else {
        list.append(sequence);
    }
    apply(row, list);
    return m_entries.at(row).current.contains(sequence);
}

void ShortcutModel::clear(const QString &id, int index)
{
    const int row = rowOf(id);
    if (row < 0 || index < 0 || index >= m_entries.at(row).current.size()) {
        return;
    }
    QList<QKeySequence> list = m_entries.at(row).current;
    list.removeAt(index);
    apply(row, list);
}

void ShortcutModel::resetToDefault(const QString &id)
{
    const int row = rowOf(id);
    if (row >= 0) {
        apply(row, m_entries.at(row).definition->defaultShortcuts);
    }
}

void ShortcutModel::applyShortcuts(const QString &id, const QList<QKeySequence> &shortcuts)
{
    const int row = rowOf(id);
    if (row >= 0) {
        apply(row, shortcuts);
    }
}

void ShortcutModel::resetAll()
{
    for (int row = 0; row < m_entries.size(); ++row) {
        if (!isDefault(m_entries.at(row))) {
            apply(row, m_entries.at(row).definition->defaultShortcuts);
        }
    }
}

void ShortcutModel::apply(int row, const QList<QKeySequence> &shortcuts)
{
    Entry &entry = m_entries[row];
    entry.current = cleaned(shortcuts);
    if (!s_offline) {
        SettingsFile *file = SettingsFile::instance();
        if (isDefault(entry)) {
            // Without an entry the service uses its defaults (the same ones).
            file->remove(s_group, entry.id);
        } else {
            file->setValue(s_group, entry.id, KeyCombo::toList(entry.current));
        }
    }
    updateSearchText(entry);
    Q_EMIT dataChanged(index(row), index(row));
    Q_EMIT shortcutChanged(entry.id);
    GlobalShortcutsModel::instance()->updateOwnAction(entry.id, entry.current);
    const bool nowDefault = allDefault();
    if (nowDefault != m_lastAllDefault) {
        m_lastAllDefault = nowDefault;
        Q_EMIT allDefaultChanged();
    }
}

void ShortcutModel::reloadFromFile()
{
    if (SettingsFile::instance()->hasPendingChanges()) {
        return; // Our own pending change wins.
    }
    for (int row = 0; row < m_entries.size(); ++row) {
        Entry &entry = m_entries[row];
        const QList<QKeySequence> current = readShortcuts(entry);
        if (current != entry.current) {
            entry.current = current;
            updateSearchText(entry);
            Q_EMIT dataChanged(index(row), index(row));
            Q_EMIT shortcutChanged(entry.id);
            GlobalShortcutsModel::instance()->updateOwnAction(entry.id, entry.current);
            const bool nowDefault = allDefault();
            if (nowDefault != m_lastAllDefault) {
                m_lastAllDefault = nowDefault;
                Q_EMIT allDefaultChanged();
            }
        }
    }
}

QStringList ShortcutModel::keysFor(const QString &sequence) const
{
    return keyNames(QKeySequence::fromString(sequence, QKeySequence::PortableText));
}

QList<QKeySequence> ShortcutModel::currentShortcuts(const QString &id) const
{
    const int row = rowOf(id);
    return row < 0 ? QList<QKeySequence>() : m_entries.at(row).current;
}

QStringList ShortcutModel::currentKeys(const QString &id) const
{
    const int row = rowOf(id);
    if (row < 0 || m_entries.at(row).current.isEmpty()) {
        return QStringList();
    }
    return keyNames(m_entries.at(row).current.constFirst());
}

QString ShortcutModel::title(const QString &id) const
{
    const int row = rowOf(id);
    return row < 0 ? QString() : m_entries.at(row).title;
}

QString ShortcutModel::groupTitle(const QString &group) const
{
    if (group == QLatin1String("zoom")) {
        return i18nc("@title shortcut group", "Zoom");
    }
    if (group == QLatin1String("pan")) {
        return i18nc("@title shortcut group", "Panning");
    }
    if (group == QLatin1String("focus")) {
        return i18nc("@title shortcut group", "Window Focus");
    }
    if (group == QLatin1String("bookmarks")) {
        return i18nc("@title shortcut group", "Bookmarks");
    }
    if (group == QLatin1String("windows")) {
        return i18nc("@title shortcut group", "Windows");
    }
    if (group == QLatin1String("organize")) {
        return i18nc("@title shortcut group", "Notes, Groups & Map");
    }
    if (group == QLatin1String("groups")) {
        return i18nc("@title shortcut group", "Open Groups");
    }
    return group;
}

QStringList ShortcutModel::groups() const
{
    return m_groups;
}

// ---------------------------------------------------------------------------

ShortcutFilterModel::ShortcutFilterModel(QObject *parent)
    : FilterProxyModel(parent)
{
    setSourceModel(ShortcutModel::instance());
    connect(this, &QAbstractItemModel::rowsInserted, this, &ShortcutFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ShortcutFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ShortcutFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &ShortcutFilterModel::countChanged);
}

QString ShortcutFilterModel::group() const
{
    return m_group;
}

void ShortcutFilterModel::setGroup(const QString &group)
{
    if (m_group == group) {
        return;
    }
    beginFilterUpdate();
    m_group = group;
    endFilterUpdate();
    Q_EMIT groupChanged();
}

QString ShortcutFilterModel::query() const
{
    return m_query;
}

void ShortcutFilterModel::setQuery(const QString &query)
{
    if (m_query == query) {
        return;
    }
    beginFilterUpdate();
    m_query = query;
    m_terms = SearchUtil::terms(query);
    endFilterUpdate();
    if (rowCount() > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), {HighlightedTitleRole, HighlightedDescriptionRole});
    }
    Q_EMIT queryChanged();
}

QColor ShortcutFilterModel::highlightColor() const
{
    return m_highlightColor;
}

void ShortcutFilterModel::setHighlightColor(const QColor &color)
{
    if (m_highlightColor == color) {
        return;
    }
    m_highlightColor = color;
    if (rowCount() > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), {HighlightedTitleRole, HighlightedDescriptionRole});
    }
    Q_EMIT highlightColorChanged();
}

int ShortcutFilterModel::count() const
{
    return rowCount();
}

QHash<int, QByteArray> ShortcutFilterModel::roleNames() const
{
    QHash<int, QByteArray> roles = QSortFilterProxyModel::roleNames();
    roles.insert(HighlightedTitleRole, "highlightedTitle");
    roles.insert(HighlightedDescriptionRole, "highlightedDescription");
    return roles;
}

QVariant ShortcutFilterModel::data(const QModelIndex &index, int role) const
{
    if (role == HighlightedTitleRole) {
        return SearchUtil::highlight(QSortFilterProxyModel::data(index, ShortcutModel::TitleRole).toString(), m_terms, m_highlightColor);
    }
    if (role == HighlightedDescriptionRole) {
        return SearchUtil::highlight(QSortFilterProxyModel::data(index, ShortcutModel::DescriptionRole).toString(), m_terms, m_highlightColor);
    }
    return QSortFilterProxyModel::data(index, role);
}

bool ShortcutFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!m_group.isEmpty() && index.data(ShortcutModel::GroupRole).toString() != m_group) {
        return false;
    }
    if (m_terms.isEmpty()) {
        return true;
    }
    return SearchUtil::matches(index.data(ShortcutModel::SearchTextRole).toString(), m_terms);
}

} // namespace InfinitDesktop
