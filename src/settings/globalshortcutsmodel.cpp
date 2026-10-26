/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "globalshortcutsmodel.h"

#include "i18n.h"
#include "keycombo.h"
#include "shortcutdefs.h"
#include "shortcutmodel.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QHash>
#include <QJSEngine>
#include <QUrl>

#include <algorithm>

namespace InfiniteDesktop
{

namespace
{

GlobalShortcutsModel *s_globalInstance = nullptr;
bool s_demoMode = false;

const QString s_ownComponent = QString::fromLatin1(ShortcutComponent);

QList<QKeySequence> cleaned(const QList<QKeySequence> &list)
{
    QList<QKeySequence> result;
    for (const QKeySequence &sequence : list) {
        if (!sequence.isEmpty() && !result.contains(sequence)) {
            result.append(sequence);
        }
    }
    return result;
}

/// The same key for the service: own shortcuts are stored as it reads them back.
QKeySequence normalized(const QKeySequence &sequence)
{
    const QKeySequence service = KeyCombo::fromText(KeyCombo::toText(sequence));
    return service.isEmpty() ? sequence : service;
}

QString displayName(const QString &actionName, bool own)
{
    // Our actions are called "Infinite Desktop: …"; the badge says that already.
    if (own) {
        const int colon = actionName.indexOf(QLatin1String(": "));
        if (colon > 0 && colon <= 24) {
            return actionName.mid(colon + 2);
        }
    }
    return actionName;
}

QStringList nativeTexts(const QList<QKeySequence> &list)
{
    QStringList result;
    for (const QKeySequence &sequence : list) {
        result.append(sequence.toString(QKeySequence::NativeText));
    }
    return result;
}

} // namespace

GlobalShortcutsModel::GlobalShortcutsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

GlobalShortcutsModel *GlobalShortcutsModel::instance()
{
    if (!s_globalInstance) {
        s_globalInstance = new GlobalShortcutsModel(QCoreApplication::instance());
    }
    return s_globalInstance;
}

GlobalShortcutsModel *GlobalShortcutsModel::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    GlobalShortcutsModel *model = instance();
    QJSEngine::setObjectOwnership(model, QJSEngine::CppOwnership);
    return model;
}

void GlobalShortcutsModel::setDemoMode(bool demo)
{
    s_demoMode = demo;
}

QString GlobalShortcutsModel::keyOf(const QString &componentId, const QString &actionId)
{
    return componentId + QLatin1Char('\n') + actionId;
}

int GlobalShortcutsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QHash<int, QByteArray> GlobalShortcutsModel::roleNames() const
{
    return {
        {KeyRole, "shortcutKey"},
        {ComponentIdRole, "componentId"},
        {ComponentNameRole, "componentName"},
        {ActionIdRole, "actionId"},
        {ActionNameRole, "actionName"},
        {SequencesRole, "sequences"},
        {SequenceTextsRole, "sequenceTexts"},
        {KeysRole, "keys"},
        {DefaultTextRole, "defaultText"},
        {IsDefaultRole, "isDefault"},
        {HasConflictRole, "hasConflict"},
        {ConflictTextRole, "conflictText"},
        {IsOwnRole, "isOwn"},
        {EditableRole, "editable"},
        {SearchTextRole, "searchText"},
    };
}

QVariant GlobalShortcutsModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return QVariant();
    }
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case ActionNameRole:
        return entry.actionName.isEmpty() ? entry.actionId : displayName(entry.actionName, entry.own);
    case KeyRole:
        return keyOf(entry.componentId, entry.actionId);
    case ComponentIdRole:
        return entry.componentId;
    case ComponentNameRole:
        return entry.componentName.isEmpty() ? entry.componentId : entry.componentName;
    case ActionIdRole:
        return entry.actionId;
    case SequencesRole: {
        QStringList result;
        for (const QKeySequence &sequence : entry.keys) {
            result.append(sequence.toString(QKeySequence::PortableText));
        }
        return result;
    }
    case SequenceTextsRole:
        return nativeTexts(entry.keys);
    case KeysRole: {
        QVariantList result;
        for (const QKeySequence &sequence : entry.keys) {
            result.append(ShortcutModel::keyNames(sequence));
        }
        return result;
    }
    case DefaultTextRole: {
        const QStringList texts = nativeTexts(entry.defaultKeys);
        return texts.isEmpty() ? i18nc("@label no keyboard shortcut assigned", "None") : texts.join(i18nc("@label separator between alternative shortcuts", ", "));
    }
    case IsDefaultRole:
        return entry.keys == entry.defaultKeys;
    case HasConflictRole:
        return entry.conflict;
    case ConflictTextRole:
        return entry.conflictText;
    case IsOwnRole:
        return entry.own;
    case EditableRole:
        // Only our own actions: Windows has no interface to change its shortcuts.
        return entry.own;
    case SearchTextRole:
        return entry.searchText;
    }
    return QVariant();
}

bool GlobalShortcutsModel::available() const
{
    return m_available;
}

bool GlobalShortcutsModel::loading() const
{
    return m_loading;
}

int GlobalShortcutsModel::conflictCount() const
{
    return m_conflictCount;
}

int GlobalShortcutsModel::componentCount() const
{
    return m_componentCount;
}

int GlobalShortcutsModel::count() const
{
    return m_entries.size();
}

int GlobalShortcutsModel::rowOf(const QString &key) const
{
    for (int row = 0; row < m_entries.size(); ++row) {
        if (keyOf(m_entries.at(row).componentId, m_entries.at(row).actionId) == key) {
            return row;
        }
    }
    return -1;
}

void GlobalShortcutsModel::updateSearchText(Entry &entry)
{
    entry.searchText = SearchUtil::normalize(QStringList{entry.actionName, entry.componentName, entry.actionId, nativeTexts(entry.keys).join(QLatin1Char(' '))}
                                                 .join(QLatin1Char(' ')));
}

void GlobalShortcutsModel::reload()
{
    // Ours from the shortcut model, Windows' own from a fixed table: Windows has no interface
    // that lists the shortcuts of the shell, so the table holds the documented defaults.
    QList<Entry> list;
    ShortcutModel *own = ShortcutModel::instance();
    const QString ownName = QString::fromLatin1(ShortcutComponentDisplayName);
    for (const ShortcutDefinition &definition : shortcutDefinitions()) {
        Entry entry;
        entry.componentId = s_ownComponent;
        entry.componentName = ownName;
        entry.actionId = QString::fromLatin1(definition.id);
        entry.actionName = definition.text.toString();
        entry.defaultKeys = cleaned(definition.defaultShortcuts);
        entry.keys = cleaned(own->currentShortcuts(entry.actionId));
        entry.own = true;
        updateSearchText(entry);
        list.append(entry);
    }
    list.append(windowsShortcuts());
    if (!m_available) {
        m_available = true;
        Q_EMIT availableChanged();
    }
    setEntries(list);
}

void GlobalShortcutsModel::setEntries(QList<Entry> entries)
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::sort(entries.begin(), entries.end(), [&collator](const Entry &a, const Entry &b) {
        const QString componentA = a.componentName.isEmpty() ? a.componentId : a.componentName;
        const QString componentB = b.componentName.isEmpty() ? b.componentId : b.componentName;
        if (componentA != componentB) {
            const int order = collator.compare(componentA, componentB);
            return order != 0 ? order < 0 : a.componentId < b.componentId;
        }
        return collator.compare(a.actionName, b.actionName) < 0;
    });

    bool sameRows = entries.size() == m_entries.size();
    for (int row = 0; sameRows && row < entries.size(); ++row) {
        sameRows = entries.at(row).componentId == m_entries.at(row).componentId && entries.at(row).actionId == m_entries.at(row).actionId;
    }
    if (sameRows) {
        // Update in place so that views keep their scroll position.
        for (int row = 0; row < entries.size(); ++row) {
            const Entry &fresh = entries.at(row);
            Entry &current = m_entries[row];
            if (fresh.keys != current.keys || fresh.defaultKeys != current.defaultKeys || fresh.actionName != current.actionName) {
                current = fresh;
                Q_EMIT dataChanged(index(row), index(row));
            }
        }
    } else {
        beginResetModel();
        m_entries = entries;
        endResetModel();
    }

    QSet<QString> components;
    for (const Entry &entry : std::as_const(m_entries)) {
        components.insert(entry.componentId);
    }
    m_componentCount = components.size();
    Q_EMIT countsChanged();
    updateConflicts();
}

void GlobalShortcutsModel::updateConflicts()
{
    QHash<QKeySequence, QList<int>> users;
    for (int row = 0; row < m_entries.size(); ++row) {
        for (const QKeySequence &sequence : std::as_const(m_entries.at(row).keys)) {
            users[sequence].append(row);
        }
    }

    int conflicts = 0;
    for (int row = 0; row < m_entries.size(); ++row) {
        Entry &entry = m_entries[row];
        QStringList others;
        for (const QKeySequence &sequence : std::as_const(entry.keys)) {
            const QList<int> rows = users.value(sequence);
            for (int other : rows) {
                if (other == row) {
                    continue;
                }
                const Entry &otherEntry = m_entries.at(other);
                others.append(i18nc("@info %1 is a key combination, %2 an action, %3 an application", "%1 is also used by “%2” (%3)",
                                    sequence.toString(QKeySequence::NativeText),
                                    otherEntry.actionName.isEmpty() ? otherEntry.actionId : otherEntry.actionName,
                                    otherEntry.componentName.isEmpty() ? otherEntry.componentId : otherEntry.componentName));
            }
        }
        const bool conflict = !others.isEmpty();
        const QString text = others.join(QLatin1Char('\n'));
        if (conflict) {
            ++conflicts;
        }
        if (conflict != entry.conflict || text != entry.conflictText) {
            entry.conflict = conflict;
            entry.conflictText = text;
            Q_EMIT dataChanged(index(row), index(row), {HasConflictRole, ConflictTextRole});
        }
    }
    if (conflicts != m_conflictCount) {
        m_conflictCount = conflicts;
        Q_EMIT conflictsChanged();
    }
}

QVariantList GlobalShortcutsModel::conflicts(const QString &key, const QString &sequenceText) const
{
    QVariantList result;
    const QKeySequence sequence = QKeySequence::fromString(sequenceText, QKeySequence::PortableText);
    if (sequence.isEmpty()) {
        return result;
    }
    for (const Entry &entry : m_entries) {
        if (keyOf(entry.componentId, entry.actionId) == key || !entry.keys.contains(sequence)) {
            continue;
        }
        result.append(QVariantMap{
            {QStringLiteral("action"), entry.actionName.isEmpty() ? entry.actionId : entry.actionName},
            {QStringLiteral("component"), entry.componentName.isEmpty() ? entry.componentId : entry.componentName},
            {QStringLiteral("key"), keyOf(entry.componentId, entry.actionId)},
        });
    }
    return result;
}

bool GlobalShortcutsModel::setKeys(int row, const QList<QKeySequence> &requested)
{
    Entry &entry = m_entries[row];
    const QList<QKeySequence> keys = cleaned(requested);

    if (entry.own) {
        // Our own actions go through ShortcutModel, which writes settings.ini, keeps its
        // page up to date and calls updateOwnAction().
        ShortcutModel::instance()->applyShortcuts(entry.actionId, keys);
        return true;
    }
    Q_EMIT changeFailed(i18n("Shortcuts of Windows cannot be changed here. Give the Infinite Desktop action another shortcut instead."));
    return false;
}

bool GlobalShortcutsModel::assign(const QString &key, int slot, const QString &sequenceText, bool steal)
{
    const int row = rowOf(key);
    const QKeySequence sequence = QKeySequence::fromString(sequenceText, QKeySequence::PortableText);
    if (row < 0 || sequence.isEmpty() || slot < 0) {
        return false;
    }
    if (steal) {
        for (int other = 0; other < m_entries.size(); ++other) {
            if (other != row && m_entries.at(other).own && m_entries.at(other).keys.contains(sequence)) {
                QList<QKeySequence> keys = m_entries.at(other).keys;
                keys.removeAll(sequence);
                setKeys(other, keys);
            }
        }
    }
    QList<QKeySequence> keys = m_entries.at(row).keys;
    keys.removeAll(sequence);
    if (slot < keys.size()) {
        keys[slot] = sequence;
    } else {
        keys.append(sequence);
    }
    setKeys(row, keys);
    return m_entries.at(row).keys.contains(sequence);
}

void GlobalShortcutsModel::clear(const QString &key, int slot)
{
    const int row = rowOf(key);
    if (row < 0 || slot < 0 || slot >= m_entries.at(row).keys.size()) {
        return;
    }
    QList<QKeySequence> keys = m_entries.at(row).keys;
    keys.removeAt(slot);
    setKeys(row, keys);
}

void GlobalShortcutsModel::resetToDefault(const QString &key)
{
    const int row = rowOf(key);
    if (row >= 0) {
        setKeys(row, m_entries.at(row).defaultKeys);
    }
}

void GlobalShortcutsModel::updateOwnAction(const QString &actionId, const QList<QKeySequence> &keys)
{
    const int row = rowOf(keyOf(s_ownComponent, actionId));
    if (row < 0) {
        return;
    }
    Entry &entry = m_entries[row];
    const QList<QKeySequence> fresh = cleaned(keys);
    if (entry.keys == fresh) {
        return;
    }
    entry.keys = fresh;
    updateSearchText(entry);
    Q_EMIT dataChanged(index(row), index(row));
    updateConflicts();
}

bool GlobalShortcutsModel::openSystemSettings() const
{
    // The only shortcuts of Windows one can change are those of the Game Bar.
    return QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:gaming-gamebar")));
}

QVariantList GlobalShortcutsModel::systemConflicts(const QKeySequence &input)
{
    QVariantList result;
    if (m_entries.isEmpty()) {
        reload();
    }
    const QKeySequence sequence = normalized(input);
    for (const Entry &entry : m_entries) {
        if (entry.own) {
            continue;
        }
        for (const QKeySequence &keys : entry.keys) {
            if (normalized(keys) == sequence) {
                result.append(QVariantMap{
                    {QStringLiteral("action"), entry.actionName},
                    {QStringLiteral("component"), entry.componentName},
                    {QStringLiteral("own"), false},
                });
                break;
            }
        }
    }
    return result;
}

QList<GlobalShortcutsModel::Entry> GlobalShortcutsModel::windowsShortcuts()
{
    const QString windows = i18nc("@title shortcut component", "Windows");
    const QString snap = i18nc("@title shortcut component", "Windows: windows and Snap");
    const QString desktops = i18nc("@title shortcut component", "Windows: virtual desktops");
    const QString access = i18nc("@title shortcut component", "Windows: accessibility");
    const QString gameBar = i18nc("@title shortcut component", "Xbox Game Bar");
    const auto win = Qt::META;
    const auto winShift = Qt::META | Qt::SHIFT;
    const auto winCtrl = Qt::META | Qt::CTRL;
    const auto winAlt = Qt::META | Qt::ALT;

    QList<Entry> list;
    const auto add = [&list](const QString &componentId, const QString &component, const QString &actionId, const QString &name, const QList<QKeySequence> &keys) {
        Entry entry;
        entry.componentId = componentId;
        entry.componentName = component;
        entry.actionId = actionId;
        entry.actionName = name;
        entry.keys = keys;
        entry.defaultKeys = keys;
        list.append(entry);
    };
    const QString w = QStringLiteral("windows");
    add(w, windows, QStringLiteral("quick-settings"), i18nc("@action Windows shortcut", "Open quick settings"), {QKeySequence(win | Qt::Key_A)});
    add(w, windows, QStringLiteral("tray"), i18nc("@action Windows shortcut", "Focus the notification area"), {QKeySequence(win | Qt::Key_B)});
    add(w, windows, QStringLiteral("copilot"), i18nc("@action Windows shortcut", "Open Copilot"), {QKeySequence(win | Qt::Key_C)});
    add(w, windows, QStringLiteral("desktop"), i18nc("@action Windows shortcut", "Show or hide the desktop"), {QKeySequence(win | Qt::Key_D)});
    add(w, windows, QStringLiteral("explorer"), i18nc("@action Windows shortcut", "Open File Explorer"), {QKeySequence(win | Qt::Key_E)});
    add(w, windows, QStringLiteral("voice-typing"), i18nc("@action Windows shortcut", "Start voice typing"), {QKeySequence(win | Qt::Key_H)});
    add(w, windows, QStringLiteral("settings"), i18nc("@action Windows shortcut", "Open Settings"), {QKeySequence(win | Qt::Key_I)});
    add(w, windows, QStringLiteral("cast"), i18nc("@action Windows shortcut", "Cast to a display"), {QKeySequence(win | Qt::Key_K)});
    add(w, windows, QStringLiteral("lock"), i18nc("@action Windows shortcut", "Lock the computer"), {QKeySequence(win | Qt::Key_L)});
    add(w, windows, QStringLiteral("minimize-all"), i18nc("@action Windows shortcut", "Minimize all windows"), {QKeySequence(win | Qt::Key_M)});
    add(w, windows, QStringLiteral("restore-all"), i18nc("@action Windows shortcut", "Restore minimized windows"), {QKeySequence(winShift | Qt::Key_M)});
    add(w, windows, QStringLiteral("notifications"), i18nc("@action Windows shortcut", "Open notifications and calendar"), {QKeySequence(win | Qt::Key_N)});
    add(w, windows, QStringLiteral("project"), i18nc("@action Windows shortcut", "Choose a presentation display mode"), {QKeySequence(win | Qt::Key_P)});
    add(w, windows, QStringLiteral("run"), i18nc("@action Windows shortcut", "Open the Run dialog"), {QKeySequence(win | Qt::Key_R)});
    add(w, windows, QStringLiteral("search"), i18nc("@action Windows shortcut", "Open search"), {QKeySequence(win | Qt::Key_S), QKeySequence(win | Qt::Key_Q)});
    add(w, windows, QStringLiteral("taskbar"), i18nc("@action Windows shortcut", "Cycle through the apps on the taskbar"), {QKeySequence(win | Qt::Key_T)});
    add(w, windows, QStringLiteral("accessibility"), i18nc("@action Windows shortcut", "Open accessibility settings"), {QKeySequence(win | Qt::Key_U)});
    add(w, windows, QStringLiteral("clipboard"), i18nc("@action Windows shortcut", "Open the clipboard history"), {QKeySequence(win | Qt::Key_V)});
    add(w, windows, QStringLiteral("widgets"), i18nc("@action Windows shortcut", "Open widgets"), {QKeySequence(win | Qt::Key_W)});
    add(w, windows, QStringLiteral("quick-link"), i18nc("@action Windows shortcut", "Open the Quick Link menu"), {QKeySequence(win | Qt::Key_X)});
    add(w, windows, QStringLiteral("emoji"), i18nc("@action Windows shortcut", "Open the emoji panel"), {QKeySequence(win | Qt::Key_Period), QKeySequence(win | Qt::Key_Semicolon)});
    add(w, windows, QStringLiteral("input-language"), i18nc("@action Windows shortcut", "Switch the input language"), {QKeySequence(win | Qt::Key_Space)});
    add(w, windows, QStringLiteral("snipping"), i18nc("@action Windows shortcut", "Take a screenshot of a region"), {QKeySequence(winShift | Qt::Key_S)});
    add(w, windows, QStringLiteral("task-manager"), i18nc("@action Windows shortcut", "Open Task Manager"), {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Escape)});
    add(w, windows, QStringLiteral("switch-apps"), i18nc("@action Windows shortcut", "Switch between open apps"), {QKeySequence(Qt::ALT | Qt::Key_Tab)});
    add(w, windows, QStringLiteral("close"), i18nc("@action Windows shortcut", "Close the active window"), {QKeySequence(Qt::ALT | Qt::Key_F4)});
    add(w, windows, QStringLiteral("date-time"), i18nc("@action Windows shortcut", "Show date and time on the taskbar"), {QKeySequence(winAlt | Qt::Key_D)});
    add(w, windows, QStringLiteral("mute"), i18nc("@action Windows shortcut", "Mute the microphone in calls"), {QKeySequence(winAlt | Qt::Key_K)});
    for (int number = 1; number <= 9; ++number) {
        const Qt::Key key = Qt::Key(Qt::Key_0 + number);
        add(w, windows, QStringLiteral("taskbar-app-%1").arg(number), i18nc("@action Windows shortcut, %1 is a number", "Open app %1 on the taskbar", number),
            {QKeySequence(win | key)});
        add(w, windows, QStringLiteral("jump-list-%1").arg(number), i18nc("@action Windows shortcut, %1 is a number", "Open the jump list of app %1 on the taskbar", number),
            {QKeySequence(winAlt | key)});
    }
    const QString sn = QStringLiteral("windows-snap");
    add(sn, snap, QStringLiteral("task-view"), i18nc("@action Windows shortcut", "Open Task View"), {QKeySequence(win | Qt::Key_Tab)});
    add(sn, snap, QStringLiteral("snap-layouts"), i18nc("@action Windows shortcut", "Open snap layouts"), {QKeySequence(win | Qt::Key_Z)});
    add(sn, snap, QStringLiteral("maximize"), i18nc("@action Windows shortcut", "Maximize the window"), {QKeySequence(win | Qt::Key_Up)});
    add(sn, snap, QStringLiteral("minimize"), i18nc("@action Windows shortcut", "Restore or minimize the window"), {QKeySequence(win | Qt::Key_Down)});
    add(sn, snap, QStringLiteral("snap-left"), i18nc("@action Windows shortcut", "Snap the window to the left"), {QKeySequence(win | Qt::Key_Left)});
    add(sn, snap, QStringLiteral("snap-right"), i18nc("@action Windows shortcut", "Snap the window to the right"), {QKeySequence(win | Qt::Key_Right)});
    add(sn, snap, QStringLiteral("stretch"), i18nc("@action Windows shortcut", "Stretch the window to the top and bottom"), {QKeySequence(winShift | Qt::Key_Up)});
    add(sn, snap, QStringLiteral("monitor-left"), i18nc("@action Windows shortcut", "Move the window to the monitor on the left"), {QKeySequence(winShift | Qt::Key_Left)});
    add(sn, snap, QStringLiteral("monitor-right"), i18nc("@action Windows shortcut", "Move the window to the monitor on the right"), {QKeySequence(winShift | Qt::Key_Right)});
    add(sn, snap, QStringLiteral("minimize-others"), i18nc("@action Windows shortcut", "Minimize all windows except the active one"), {QKeySequence(win | Qt::Key_Home)});
    add(sn, snap, QStringLiteral("peek"), i18nc("@action Windows shortcut", "Peek at the desktop"), {QKeySequence(win | Qt::Key_Comma)});
    const QString vd = QStringLiteral("windows-desktops");
    add(vd, desktops, QStringLiteral("new"), i18nc("@action Windows shortcut", "Add a virtual desktop"), {QKeySequence(winCtrl | Qt::Key_D)});
    add(vd, desktops, QStringLiteral("close"), i18nc("@action Windows shortcut", "Close the virtual desktop"), {QKeySequence(winCtrl | Qt::Key_F4)});
    add(vd, desktops, QStringLiteral("left"), i18nc("@action Windows shortcut", "Switch to the virtual desktop on the left"), {QKeySequence(winCtrl | Qt::Key_Left)});
    add(vd, desktops, QStringLiteral("right"), i18nc("@action Windows shortcut", "Switch to the virtual desktop on the right"), {QKeySequence(winCtrl | Qt::Key_Right)});
    const QString ac = QStringLiteral("windows-accessibility");
    add(ac, access, QStringLiteral("magnifier-in"), i18nc("@action Windows shortcut", "Magnifier: zoom in"), {QKeySequence(win | Qt::Key_Plus), QKeySequence(win | Qt::Key_Equal)});
    add(ac, access, QStringLiteral("magnifier-out"), i18nc("@action Windows shortcut", "Magnifier: zoom out"), {QKeySequence(win | Qt::Key_Minus)});
    add(ac, access, QStringLiteral("magnifier-close"), i18nc("@action Windows shortcut", "Close the Magnifier"), {QKeySequence(win | Qt::Key_Escape)});
    add(ac, access, QStringLiteral("narrator"), i18nc("@action Windows shortcut", "Turn Narrator on or off"), {QKeySequence(winCtrl | Qt::Key_Return)});
    add(ac, access, QStringLiteral("color-filters"), i18nc("@action Windows shortcut", "Turn color filters on or off"), {QKeySequence(winCtrl | Qt::Key_C)});
    add(ac, access, QStringLiteral("keyboard"), i18nc("@action Windows shortcut", "Open the on-screen keyboard"), {QKeySequence(winCtrl | Qt::Key_O)});
    const QString gb = QStringLiteral("gamebar");
    add(gb, gameBar, QStringLiteral("open"), i18nc("@action Windows shortcut", "Open the Game Bar"), {QKeySequence(win | Qt::Key_G)});
    add(gb, gameBar, QStringLiteral("record-that"), i18nc("@action Windows shortcut", "Record the last moments"), {QKeySequence(winAlt | Qt::Key_G)});
    add(gb, gameBar, QStringLiteral("record"), i18nc("@action Windows shortcut", "Start or stop recording"), {QKeySequence(winAlt | Qt::Key_R)});
    add(gb, gameBar, QStringLiteral("timer"), i18nc("@action Windows shortcut", "Show or hide the recording timer"), {QKeySequence(winAlt | Qt::Key_T)});
    add(gb, gameBar, QStringLiteral("microphone"), i18nc("@action Windows shortcut", "Turn the microphone on or off while recording"), {QKeySequence(winAlt | Qt::Key_M)});
    add(gb, gameBar, QStringLiteral("hdr"), i18nc("@action Windows shortcut", "Turn HDR on or off"), {QKeySequence(winAlt | Qt::Key_B)});
    add(gb, gameBar, QStringLiteral("screenshot"), i18nc("@action Windows shortcut", "Take a screenshot of the game"), {QKeySequence(winAlt | Qt::Key_Print)});
    for (Entry &entry : list) {
        updateSearchText(entry);
    }
    return list;
}

// ---------------------------------------------------------------------------

GlobalShortcutsFilterModel::GlobalShortcutsFilterModel(QObject *parent)
    : FilterProxyModel(parent)
{
    setSourceModel(GlobalShortcutsModel::instance());
    connect(this, &QAbstractItemModel::rowsInserted, this, &GlobalShortcutsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &GlobalShortcutsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &GlobalShortcutsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &GlobalShortcutsFilterModel::countChanged);
    // Conflicts change the filter result of "only conflicts".
    connect(GlobalShortcutsModel::instance(), &GlobalShortcutsModel::conflictsChanged, this, [this] {
        if (m_onlyConflicts) {
            beginFilterUpdate();
            endFilterUpdate();
        }
    });
    // First/last flags of the neighbours change when rows appear or disappear.
    auto refreshEdges = [this] {
        if (rowCount() > 0) {
            Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), {FirstInComponentRole, LastInComponentRole});
        }
    };
    connect(this, &QAbstractItemModel::rowsInserted, this, refreshEdges);
    connect(this, &QAbstractItemModel::rowsRemoved, this, refreshEdges);
}

QString GlobalShortcutsFilterModel::query() const
{
    return m_query;
}

void GlobalShortcutsFilterModel::setQuery(const QString &query)
{
    if (m_query == query) {
        return;
    }
    beginFilterUpdate();
    m_query = query;
    m_terms = SearchUtil::terms(query);
    endFilterUpdate();
    if (rowCount() > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), {HighlightedActionNameRole, FirstInComponentRole, LastInComponentRole});
    }
    Q_EMIT queryChanged();
}

bool GlobalShortcutsFilterModel::onlyConflicts() const
{
    return m_onlyConflicts;
}

void GlobalShortcutsFilterModel::setOnlyConflicts(bool onlyConflicts)
{
    if (m_onlyConflicts == onlyConflicts) {
        return;
    }
    beginFilterUpdate();
    m_onlyConflicts = onlyConflicts;
    endFilterUpdate();
    Q_EMIT onlyConflictsChanged();
}

bool GlobalShortcutsFilterModel::excludeOwn() const
{
    return m_excludeOwn;
}

void GlobalShortcutsFilterModel::setExcludeOwn(bool excludeOwn)
{
    if (m_excludeOwn == excludeOwn) {
        return;
    }
    beginFilterUpdate();
    m_excludeOwn = excludeOwn;
    endFilterUpdate();
    Q_EMIT excludeOwnChanged();
}

QColor GlobalShortcutsFilterModel::highlightColor() const
{
    return m_highlightColor;
}

void GlobalShortcutsFilterModel::setHighlightColor(const QColor &color)
{
    if (m_highlightColor == color) {
        return;
    }
    m_highlightColor = color;
    if (rowCount() > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), {HighlightedActionNameRole});
    }
    Q_EMIT highlightColorChanged();
}

int GlobalShortcutsFilterModel::count() const
{
    return rowCount();
}

QHash<int, QByteArray> GlobalShortcutsFilterModel::roleNames() const
{
    QHash<int, QByteArray> roles = QSortFilterProxyModel::roleNames();
    roles.insert(HighlightedActionNameRole, "highlightedActionName");
    roles.insert(FirstInComponentRole, "firstInComponent");
    roles.insert(LastInComponentRole, "lastInComponent");
    return roles;
}

QVariant GlobalShortcutsFilterModel::data(const QModelIndex &index, int role) const
{
    if (role == HighlightedActionNameRole) {
        return SearchUtil::highlight(QSortFilterProxyModel::data(index, GlobalShortcutsModel::ActionNameRole).toString(), m_terms, m_highlightColor);
    }
    if (role == FirstInComponentRole || role == LastInComponentRole) {
        const int neighbour = index.row() + (role == FirstInComponentRole ? -1 : 1);
        if (neighbour < 0 || neighbour >= rowCount()) {
            return true;
        }
        return this->index(neighbour, 0).data(GlobalShortcutsModel::ComponentIdRole) != index.data(GlobalShortcutsModel::ComponentIdRole);
    }
    return QSortFilterProxyModel::data(index, role);
}

bool GlobalShortcutsFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (m_onlyConflicts && !index.data(GlobalShortcutsModel::HasConflictRole).toBool()) {
        return false;
    }
    if (m_excludeOwn && index.data(GlobalShortcutsModel::IsOwnRole).toBool()) {
        return false;
    }
    if (m_terms.isEmpty()) {
        return true;
    }
    return SearchUtil::matches(index.data(GlobalShortcutsModel::SearchTextRole).toString(), m_terms);
}

} // namespace InfiniteDesktop
