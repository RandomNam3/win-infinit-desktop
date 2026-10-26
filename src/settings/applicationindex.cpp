/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "applicationindex.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QThread>

#include <algorithm>

namespace InfiniteDesktop
{

namespace
{
ApplicationIndex *s_applicationsInstance = nullptr;
bool s_demoMode = false;

bool looksLikePath(const QString &id)
{
    return id.size() > 3 && id.at(1) == QLatin1Char(':') && (id.at(2) == QLatin1Char('\\') || id.at(2) == QLatin1Char('/'));
}

/// "notepad" for C:\Windows\notepad.exe, the program name the exclusion lists also accept.
QString programName(const QString &path)
{
    return QFileInfo(path).completeBaseName().toLower();
}
} // namespace

ApplicationIndex::ApplicationIndex(QObject *parent)
    : QAbstractListModel(parent)
{
}

ApplicationIndex::~ApplicationIndex()
{
    if (m_worker) {
        // Reading the Start menu takes a second at most; it must not outlive the model.
        m_worker->wait();
        delete m_worker;
        m_worker = nullptr;
    }
    if (s_applicationsInstance == this) {
        s_applicationsInstance = nullptr;
    }
}

ApplicationIndex *ApplicationIndex::instance()
{
    if (!s_applicationsInstance) {
        s_applicationsInstance = new ApplicationIndex(QCoreApplication::instance());
    }
    return s_applicationsInstance;
}

ApplicationIndex *ApplicationIndex::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    ApplicationIndex *index = instance();
    QJSEngine::setObjectOwnership(index, QJSEngine::CppOwnership);
    return index;
}

void ApplicationIndex::setDemoMode(bool demo)
{
    s_demoMode = demo;
}

QString ApplicationIndex::shellIconName(const QString &parsingName)
{
    // Hex keeps backslashes, colons and braces out of the image URL.
    return QStringLiteral("shell-") + QString::fromLatin1(parsingName.toUtf8().toHex());
}

QString ApplicationIndex::fileIconName(const QString &path)
{
    return QStringLiteral("file-") + QString::fromLatin1(QDir::toNativeSeparators(path).toUtf8().toHex());
}

QList<ApplicationIndex::Entry> ApplicationIndex::demoEntries()
{
    QList<Entry> entries;
    const auto add = [&entries](const QString &id, const QString &name) {
        Entry entry;
        entry.id = id;
        entry.key = id.toLower();
        entry.name = name;
        entry.windowClass = looksLikePath(id) ? programName(id) : QString();
        entry.searchText = SearchUtil::normalize(name + QLatin1Char(' ') + id);
        entries.append(entry);
    };
    add(QStringLiteral("Microsoft.OutlookForWindows_8wekyb3d8bbwe!Microsoft.OutlookforWindows"), QStringLiteral("Outlook"));
    add(QStringLiteral("MSEdge"), QStringLiteral("Microsoft Edge"));
    add(QStringLiteral("Microsoft.VisualStudioCode"), QStringLiteral("Visual Studio Code"));
    add(QStringLiteral("Microsoft.WindowsTerminal_8wekyb3d8bbwe!App"), QStringLiteral("Terminal"));
    add(QStringLiteral("Microsoft.WindowsNotepad_8wekyb3d8bbwe!App"), QStringLiteral("Notepad"));
    add(QStringLiteral("Microsoft.Windows.Explorer"), QStringLiteral("File Explorer"));
    add(QStringLiteral("308046B0AF4A39CB"), QStringLiteral("Firefox"));
    add(QStringLiteral("C:\\Program Files\\Mozilla Thunderbird\\thunderbird.exe"), QStringLiteral("Thunderbird"));
    return entries;
}

void ApplicationIndex::load()
{
    if (m_loaded || m_worker) {
        return;
    }
    if (s_demoMode) {
        m_loaded = true;
        setEntries(demoEntries());
        return;
    }
#ifdef Q_OS_WIN
    const QPointer<ApplicationIndex> self(this);
    m_worker = QThread::create([self, this]() {
        QList<Entry> entries = readStartMenu();
        // Delivered on the model's thread; dropped if the model is gone by then.
        QMetaObject::invokeMethod(
            this,
            [self, entries = std::move(entries)]() mutable {
                if (self) {
                    self->m_loaded = true;
                    self->setEntries(std::move(entries));
                }
            },
            Qt::QueuedConnection);
    });
    connect(m_worker, &QThread::finished, this, [this]() {
        if (m_worker) {
            m_worker->deleteLater();
            m_worker = nullptr;
        }
    });
    m_worker->start(QThread::LowPriority);
#else
    m_loaded = true;
#endif
}

void ApplicationIndex::setEntries(QList<Entry> entries)
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::sort(entries.begin(), entries.end(), [&collator](const Entry &a, const Entry &b) {
        return collator.compare(a.name, b.name) < 0;
    });
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    Q_EMIT countChanged();
}

int ApplicationIndex::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

int ApplicationIndex::count() const
{
    return m_entries.size();
}

QHash<int, QByteArray> ApplicationIndex::roleNames() const
{
    return {
        {AppIdRole, "appId"},
        {NameRole, "name"},
        {GenericNameRole, "genericName"},
        {IconNameRole, "iconName"},
        {HasIconRole, "hasIcon"},
        {WindowClassRole, "windowClass"},
        {NoDisplayRole, "noDisplay"},
        {SearchTextRole, "searchText"},
    };
}

QVariant ApplicationIndex::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return QVariant();
    }
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return entry.name;
    case AppIdRole:
        return entry.id;
    case GenericNameRole:
        return entry.genericName;
    case IconNameRole:
        return entry.iconName;
    case HasIconRole:
        return entry.hasIcon;
    case WindowClassRole:
        return entry.windowClass;
    case NoDisplayRole:
        return entry.noDisplay;
    case SearchTextRole:
        return entry.searchText;
    }
    return QVariant();
}

int ApplicationIndex::find(const QString &id) const
{
    if (id.isEmpty()) {
        return -1;
    }
    const QString key = id.trimmed().toLower();
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).key == key) {
            return row;
        }
    }
    // "notepad" for a program, but only if that is unambiguous.
    int match = -1;
    for (int row = 0; row < m_entries.size(); ++row) {
        if (!m_entries.at(row).windowClass.isEmpty() && m_entries.at(row).windowClass == key) {
            if (match >= 0) {
                return -1;
            }
            match = row;
        }
    }
    return match;
}

QVariantMap ApplicationIndex::describe(const QString &id)
{
    load();
    const int row = find(id);
    if (row >= 0) {
        const Entry &entry = m_entries.at(row);
        return {
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), entry.name},
            {QStringLiteral("iconName"), entry.iconName},
            {QStringLiteral("hasIcon"), entry.hasIcon},
            {QStringLiteral("known"), true},
        };
    }
    if (looksLikePath(id)) {
        // A program that is not in the Start menu: its file name and its own icon.
        const QFileInfo info(id);
        return {
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), info.completeBaseName()},
            {QStringLiteral("iconName"), fileIconName(id)},
            {QStringLiteral("hasIcon"), !s_demoMode && info.exists()},
            {QStringLiteral("known"), false},
        };
    }
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), id},
        {QStringLiteral("iconName"), QString()},
        {QStringLiteral("hasIcon"), false},
        {QStringLiteral("known"), false},
    };
}

#ifndef Q_OS_WIN
QList<ApplicationIndex::Entry> ApplicationIndex::readStartMenu()
{
    return {};
}
#endif

// ---------------------------------------------------------------------------

ApplicationFilterModel::ApplicationFilterModel(QObject *parent)
    : FilterProxyModel(parent)
{
    ApplicationIndex::instance()->load();
    setSourceModel(ApplicationIndex::instance());
    connect(this, &QAbstractItemModel::rowsInserted, this, &ApplicationFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ApplicationFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ApplicationFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &ApplicationFilterModel::countChanged);
}

QString ApplicationFilterModel::query() const
{
    return m_query;
}

void ApplicationFilterModel::setQuery(const QString &query)
{
    if (m_query == query) {
        return;
    }
    beginFilterUpdate();
    m_query = query;
    m_terms = SearchUtil::terms(query);
    endFilterUpdate();
    if (rowCount() > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), {HighlightedNameRole});
    }
    Q_EMIT queryChanged();
}

bool ApplicationFilterModel::showHidden() const
{
    return m_showHidden;
}

void ApplicationFilterModel::setShowHidden(bool showHidden)
{
    if (m_showHidden == showHidden) {
        return;
    }
    beginFilterUpdate();
    m_showHidden = showHidden;
    endFilterUpdate();
    Q_EMIT showHiddenChanged();
}

QColor ApplicationFilterModel::highlightColor() const
{
    return m_highlightColor;
}

void ApplicationFilterModel::setHighlightColor(const QColor &color)
{
    if (m_highlightColor == color) {
        return;
    }
    m_highlightColor = color;
    if (rowCount() > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), {HighlightedNameRole});
    }
    Q_EMIT highlightColorChanged();
}

int ApplicationFilterModel::count() const
{
    return rowCount();
}

QHash<int, QByteArray> ApplicationFilterModel::roleNames() const
{
    QHash<int, QByteArray> roles = QSortFilterProxyModel::roleNames();
    roles.insert(HighlightedNameRole, "highlightedName");
    return roles;
}

QVariant ApplicationFilterModel::data(const QModelIndex &index, int role) const
{
    if (role == HighlightedNameRole) {
        return SearchUtil::highlight(QSortFilterProxyModel::data(index, ApplicationIndex::NameRole).toString(), m_terms, m_highlightColor);
    }
    return QSortFilterProxyModel::data(index, role);
}

bool ApplicationFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!m_showHidden && m_terms.isEmpty() && index.data(ApplicationIndex::NoDisplayRole).toBool()) {
        return false;
    }
    if (m_terms.isEmpty()) {
        return true;
    }
    return SearchUtil::matches(index.data(ApplicationIndex::SearchTextRole).toString(), m_terms);
}

} // namespace InfiniteDesktop
