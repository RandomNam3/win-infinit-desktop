/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "settingsmodel.h"

#include "i18n.h"
#include "searchutil.h"
#include "settingsfile.h"
#include "settingsmeta.h"
#include "settingsschema.h"

#include <QCoreApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QLocale>
#include <QSet>
#include <QTextBoundaryFinder>

#include <algorithm>

namespace InfiniteDesktop
{

namespace
{

constexpr int ApplyDelayMs = 120;

SettingsModel *s_settingsInstance = nullptr;
CategoryModel *s_categoryInstance = nullptr;

QString unitSuffix(const char *unit)
{
    if (!unit) {
        return QString();
    }
    const QLatin1StringView u(unit);
    if (u == QLatin1StringView("px")) {
        return i18nc("unit suffix: pixels", " px");
    }
    if (u == QLatin1StringView("px/s")) {
        return i18nc("unit suffix: pixels per second", " px/s");
    }
    if (u == QLatin1StringView("%")) {
        return i18nc("unit suffix: percent", " %");
    }
    if (u == QLatin1StringView("ms")) {
        return i18nc("unit suffix: milliseconds", " ms");
    }
    return QLatin1Char(' ') + QString::fromLatin1(unit);
}

int stepFor(int minimum, int maximum)
{
    const int range = maximum - minimum;
    if (range > 2500) {
        return 50;
    }
    if (range > 500) {
        return 10;
    }
    return 1;
}

QString firstSentence(const QString &text)
{
    QTextBoundaryFinder finder(QTextBoundaryFinder::Sentence, text);
    const qsizetype end = finder.toNextBoundary();
    if (end <= 0 || end >= text.size()) {
        return text.trimmed();
    }
    return text.left(end).trimmed();
}

const SettingsCategory *findCategory(const QString &id)
{
    for (const SettingsCategory &category : settingsCategories()) {
        if (id == QLatin1String(category.id)) {
            return &category;
        }
    }
    return nullptr;
}

int categoryIndex(const QString &id)
{
    const auto &categories = settingsCategories();
    for (int i = 0; i < categories.size(); ++i) {
        if (id == QLatin1String(categories.at(i).id)) {
            return i;
        }
    }
    return categories.size();
}

QVariant unwrap(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QJSValue>()) {
        return value.value<QJSValue>().toVariant();
    }
    return value;
}

} // namespace

SettingsModel::SettingsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    SettingsFile *file = SettingsFile::instance();
    for (const auto &owned : SettingsSchema::instance().items()) {
        SchemaItem *item = owned.get();
        item->readConfig(*file);

        Entry entry;
        entry.item = item;
        entry.key = item->name();
        const QByteArray key = entry.key.toLatin1();
        entry.meta = findSettingsEntry(key.constData());
        entry.category = entry.meta ? QString::fromLatin1(entry.meta->category) : QStringLiteral("advanced");
        entry.categoryIndex = categoryIndex(entry.category);
        entry.hint = firstSentence(item->whatsThis());
        entry.keywords = entry.meta ? entry.meta->keywords.toString() : QString();
        entry.unit = unitSuffix(entry.meta ? entry.meta->unit : nullptr);
        entry.dependsOn = entry.meta && entry.meta->dependsOn ? QString::fromLatin1(entry.meta->dependsOn) : QString();

        switch (item->type()) {
        case SchemaItem::Type::Enum: {
            entry.type = QStringLiteral("enum");
            const auto choices = item->choices();
            for (const auto &choice : choices) {
                entry.choices.append(choice.label.isEmpty() ? choice.name : choice.label);
            }
            entry.minimum = 0;
            entry.maximum = entry.choices.size() - 1;
            break;
        }
        case SchemaItem::Type::Int:
            entry.type = QStringLiteral("int");
            entry.minimum = item->minValue().value_or(0);
            entry.maximum = item->maxValue().value_or(100000);
            entry.step = stepFor(entry.minimum, entry.maximum);
            break;
        case SchemaItem::Type::Bool:
            entry.type = QStringLiteral("bool");
            break;
        case SchemaItem::Type::Color:
            entry.type = QStringLiteral("color");
            break;
        case SchemaItem::Type::StringList:
            entry.type = QStringLiteral("stringlist");
            break;
        }

        const SettingsCategory *category = findCategory(entry.category);
        const QString categoryTitle = category ? category->title.toString() : QString();
        entry.searchText = SearchUtil::normalize(QStringList{item->label(), item->whatsThis(), entry.keywords, categoryTitle, entry.choices.join(QLatin1Char(' '))}
                                                     .join(QLatin1Char(' ')));
        m_entries.append(entry);
    }

    std::stable_sort(m_entries.begin(), m_entries.end(), [](const Entry &a, const Entry &b) {
        return a.categoryIndex < b.categoryIndex;
    });
    for (int row = 0; row < m_entries.size(); ++row) {
        if (!m_entries.at(row).dependsOn.isEmpty()) {
            m_dependents[m_entries.at(row).dependsOn].append(row);
        }
    }

    m_applyTimer.setSingleShot(true);
    m_applyTimer.setInterval(ApplyDelayMs);
    connect(&m_applyTimer, &QTimer::timeout, this, &SettingsModel::apply);

    connect(file, &SettingsFile::changedExternally, this, &SettingsModel::configChangedExternally);
    connect(file, &SettingsFile::applied, this, &SettingsModel::applied);

    m_lastAllDefault = allDefault();

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &SettingsModel::flush);
}

SettingsModel::~SettingsModel() = default;

SettingsModel *SettingsModel::instance()
{
    if (!s_settingsInstance) {
        s_settingsInstance = new SettingsModel(QCoreApplication::instance());
    }
    return s_settingsInstance;
}

SettingsModel *SettingsModel::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    SettingsModel *model = instance();
    QJSEngine::setObjectOwnership(model, QJSEngine::CppOwnership);
    return model;
}

int SettingsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QHash<int, QByteArray> SettingsModel::roleNames() const
{
    return {
        {KeyRole, "key"},
        {LabelRole, "label"},
        {InfoRole, "info"},
        {HintRole, "hint"},
        {CategoryRole, "category"},
        {CategoryTitleRole, "categoryTitle"},
        {CategoryIconRole, "categoryIcon"},
        {CategoryIndexRole, "categoryIndex"},
        {TypeRole, "type"},
        {ValueRole, "value"},
        {DefaultValueRole, "defaultValue"},
        {IsDefaultRole, "isDefault"},
        {MinRole, "min"},
        {MaxRole, "max"},
        {StepRole, "step"},
        {UnitRole, "unit"},
        {ChoicesRole, "choices"},
        {EnabledRole, "dependencyMet"},
        {DependsOnRole, "dependsOn"},
        {DependsOnLabelRole, "dependsOnLabel"},
        {KeywordsRole, "keywords"},
        {ValueTextRole, "valueText"},
        {DefaultTextRole, "defaultText"},
        {RangeTextRole, "rangeText"},
        {SearchTextRole, "searchText"},
    };
}

QVariant SettingsModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return QVariant();
    }
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return entry.item->label();
    case KeyRole:
        return entry.key;
    case InfoRole:
        return entry.item->whatsThis();
    case HintRole:
        return entry.hint;
    case CategoryRole:
        return entry.category;
    case CategoryTitleRole: {
        const SettingsCategory *category = findCategory(entry.category);
        return category ? category->title.toString() : QString();
    }
    case CategoryIconRole: {
        const SettingsCategory *category = findCategory(entry.category);
        return category ? QString::fromLatin1(category->icon) : QString();
    }
    case CategoryIndexRole:
        return entry.categoryIndex;
    case TypeRole:
        return entry.type;
    case ValueRole:
        return entry.item->property();
    case DefaultValueRole:
        return entry.item->getDefault();
    case IsDefaultRole:
        return entry.item->isDefault();
    case MinRole:
        return entry.minimum;
    case MaxRole:
        return entry.maximum;
    case StepRole:
        return entry.step;
    case UnitRole:
        return entry.unit.trimmed();
    case ChoicesRole:
        return entry.choices;
    case EnabledRole:
        return isEnabled(index.row());
    case DependsOnRole:
        return entry.dependsOn;
    case DependsOnLabelRole:
        return entry.dependsOn.isEmpty() ? QString() : label(entry.dependsOn);
    case KeywordsRole:
        return entry.keywords;
    case ValueTextRole:
        return formatValue(entry, entry.item->property());
    case DefaultTextRole:
        return formatValue(entry, entry.item->getDefault());
    case RangeTextRole:
        if (entry.type == QLatin1String("int")) {
            return i18nc("@label range of a setting, %1 is the minimum, %2 the maximum", "%1 – %2", formatValue(entry, entry.minimum), formatValue(entry, entry.maximum));
        }
        return QString();
    case SearchTextRole:
        return entry.searchText;
    }
    return QVariant();
}

int SettingsModel::rowOf(const QString &key) const
{
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).key == key) {
            return row;
        }
    }
    return -1;
}

bool SettingsModel::isEnabled(int row) const
{
    const Entry &entry = m_entries.at(row);
    if (entry.dependsOn.isEmpty()) {
        return true;
    }
    const int parentRow = rowOf(entry.dependsOn);
    if (parentRow < 0 || parentRow == row) {
        return true;
    }
    const Entry &parent = m_entries.at(parentRow);
    const QVariant value = parent.item->property();
    const bool on = parent.type == QLatin1String("enum") ? value.toInt() != 0 : value.toBool();
    return on && isEnabled(parentRow);
}

bool SettingsModel::isEnabled(const QString &key) const
{
    const int row = rowOf(key);
    return row < 0 ? true : isEnabled(row);
}

bool SettingsModel::allDefault() const
{
    for (const Entry &entry : m_entries) {
        if (!entry.item->isDefault()) {
            return false;
        }
    }
    return true;
}

int SettingsModel::countInCategory(const QString &category) const
{
    return int(std::count_if(m_entries.cbegin(), m_entries.cend(), [&category](const Entry &entry) {
        return entry.category == category;
    }));
}

QVariant SettingsModel::value(const QString &key) const
{
    const int row = rowOf(key);
    return row < 0 ? QVariant() : m_entries.at(row).item->property();
}

QString SettingsModel::label(const QString &key) const
{
    const int row = rowOf(key);
    return row < 0 ? QString() : m_entries.at(row).item->label();
}

QVariant SettingsModel::normalizedValue(const Entry &entry, const QVariant &input) const
{
    const QVariant value = unwrap(input);
    if (!value.isValid()) {
        return QVariant();
    }
    if (entry.type == QLatin1String("bool")) {
        return value.toBool();
    }
    if (entry.type == QLatin1String("int") || entry.type == QLatin1String("enum")) {
        bool ok = false;
        const int number = qRound(value.toDouble(&ok));
        if (!ok) {
            return QVariant();
        }
        return std::clamp(number, entry.minimum, entry.maximum);
    }
    if (entry.type == QLatin1String("color")) {
        QColor color;
        if (value.metaType() == QMetaType::fromType<QColor>()) {
            color = value.value<QColor>();
        } else {
            color = QColor::fromString(value.toString());
        }
        if (!color.isValid()) {
            return QVariant();
        }
        color.setAlpha(255);
        return color;
    }
    if (entry.type == QLatin1String("stringlist")) {
        QStringList result;
        const QStringList list = value.toStringList();
        for (const QString &item : list) {
            const QString trimmed = item.trimmed();
            if (!trimmed.isEmpty() && !result.contains(trimmed)) {
                result.append(trimmed);
            }
        }
        return result;
    }
    return value;
}

QString SettingsModel::formatValue(const Entry &entry, const QVariant &value) const
{
    if (entry.type == QLatin1String("bool")) {
        return value.toBool() ? i18nc("@label state of a switch", "On") : i18nc("@label state of a switch", "Off");
    }
    if (entry.type == QLatin1String("int")) {
        return QLocale().toString(value.toInt()) + entry.unit;
    }
    if (entry.type == QLatin1String("enum")) {
        return entry.choices.value(value.toInt());
    }
    if (entry.type == QLatin1String("color")) {
        return value.value<QColor>().name(QColor::HexRgb).toUpper();
    }
    if (entry.type == QLatin1String("stringlist")) {
        const int count = value.toStringList().size();
        return i18ncp("@label number of entries in a list", "%1 entry", "%1 entries", count);
    }
    return value.toString();
}

QString SettingsModel::formatValue(const QString &key, const QVariant &value) const
{
    const int row = rowOf(key);
    if (row < 0) {
        return QString();
    }
    const Entry &entry = m_entries.at(row);
    const QVariant normalized = normalizedValue(entry, value);
    return formatValue(entry, normalized.isValid() ? normalized : value);
}

void SettingsModel::setValue(const QString &key, const QVariant &value)
{
    const int row = rowOf(key);
    if (row < 0) {
        qWarning("SettingsStore: unknown setting %s", qPrintable(key));
        return;
    }
    const Entry &entry = m_entries.at(row);
    const QVariant normalized = normalizedValue(entry, value);
    if (!normalized.isValid() || entry.item->isEqual(normalized)) {
        return;
    }
    entry.item->setProperty(normalized);
    changed(row);
    scheduleApply();
}

void SettingsModel::resetToDefault(const QString &key)
{
    const int row = rowOf(key);
    if (row < 0 || m_entries.at(row).item->isDefault()) {
        return;
    }
    m_entries.at(row).item->setDefault();
    changed(row);
    scheduleApply();
}

void SettingsModel::resetAll()
{
    bool any = false;
    for (int row = 0; row < m_entries.size(); ++row) {
        if (!m_entries.at(row).item->isDefault()) {
            m_entries.at(row).item->setDefault();
            Q_EMIT valueChanged(m_entries.at(row).key);
            any = true;
        }
    }
    if (!any) {
        return;
    }
    Q_EMIT dataChanged(index(0), index(m_entries.size() - 1));
    if (m_lastAllDefault != allDefault()) {
        m_lastAllDefault = allDefault();
        Q_EMIT allDefaultChanged();
    }
    scheduleApply();
}

void SettingsModel::changed(int row)
{
    Q_EMIT dataChanged(index(row), index(row));
    const QString &key = m_entries.at(row).key;
    // Dependency chains are short, update every entry that (indirectly) depends on this one.
    QList<int> pending = m_dependents.value(key);
    QSet<int> seen;
    while (!pending.isEmpty()) {
        const int dependent = pending.takeFirst();
        if (seen.contains(dependent)) {
            continue;
        }
        seen.insert(dependent);
        Q_EMIT dataChanged(index(dependent), index(dependent), {EnabledRole});
        pending.append(m_dependents.value(m_entries.at(dependent).key));
    }
    Q_EMIT valueChanged(key);
    const bool nowDefault = allDefault();
    if (nowDefault != m_lastAllDefault) {
        m_lastAllDefault = nowDefault;
        Q_EMIT allDefaultChanged();
    }
}

void SettingsModel::scheduleApply()
{
    m_applyTimer.start();
}

void SettingsModel::flush()
{
    if (m_applyTimer.isActive()) {
        m_applyTimer.stop();
        apply();
    }
}

void SettingsModel::apply()
{
    SettingsFile *file = SettingsFile::instance();
    for (const Entry &entry : std::as_const(m_entries)) {
        entry.item->writeConfig(*file);
    }
    // Writes and asks the service to reload; SettingsFile emits applied() then.
    file->flush();
}

void SettingsModel::configChangedExternally()
{
    if (m_applyTimer.isActive()) {
        // Our own pending change wins.
        return;
    }
    const SettingsFile *file = SettingsFile::instance();
    for (int row = 0; row < m_entries.size(); ++row) {
        SchemaItem *item = m_entries.at(row).item;
        const QVariant before = item->property();
        item->readConfig(*file);
        if (item->property() != before) {
            changed(row);
        }
    }
}

// ---------------------------------------------------------------------------

SettingsFilterModel::SettingsFilterModel(QObject *parent)
    : FilterProxyModel(parent)
{
    setSourceModel(SettingsModel::instance());
    connect(this, &QAbstractItemModel::rowsInserted, this, &SettingsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &SettingsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &SettingsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &SettingsFilterModel::countChanged);
}

QString SettingsFilterModel::category() const
{
    return m_category;
}

void SettingsFilterModel::setCategory(const QString &category)
{
    if (m_category == category) {
        return;
    }
    beginFilterUpdate();
    m_category = category;
    endFilterUpdate();
    Q_EMIT categoryChanged();
}

QString SettingsFilterModel::query() const
{
    return m_query;
}

void SettingsFilterModel::setQuery(const QString &query)
{
    if (m_query == query) {
        return;
    }
    beginFilterUpdate();
    m_query = query;
    m_terms = SearchUtil::terms(query);
    endFilterUpdate();
    if (rowCount() > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), {HighlightedLabelRole, HighlightedHintRole});
    }
    Q_EMIT queryChanged();
}

QColor SettingsFilterModel::highlightColor() const
{
    return m_highlightColor;
}

void SettingsFilterModel::setHighlightColor(const QColor &color)
{
    if (m_highlightColor == color) {
        return;
    }
    m_highlightColor = color;
    if (rowCount() > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), {HighlightedLabelRole, HighlightedHintRole});
    }
    Q_EMIT highlightColorChanged();
}

int SettingsFilterModel::count() const
{
    return rowCount();
}

QHash<int, QByteArray> SettingsFilterModel::roleNames() const
{
    QHash<int, QByteArray> roles = QSortFilterProxyModel::roleNames();
    roles.insert(HighlightedLabelRole, "highlightedLabel");
    roles.insert(HighlightedHintRole, "highlightedHint");
    return roles;
}

QVariant SettingsFilterModel::data(const QModelIndex &index, int role) const
{
    if (role == HighlightedLabelRole) {
        return SearchUtil::highlight(QSortFilterProxyModel::data(index, SettingsModel::LabelRole).toString(), m_terms, m_highlightColor);
    }
    if (role == HighlightedHintRole) {
        return SearchUtil::highlight(QSortFilterProxyModel::data(index, SettingsModel::HintRole).toString(), m_terms, m_highlightColor);
    }
    return QSortFilterProxyModel::data(index, role);
}

bool SettingsFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!m_category.isEmpty() && index.data(SettingsModel::CategoryRole).toString() != m_category) {
        return false;
    }
    if (m_terms.isEmpty()) {
        return true;
    }
    return SearchUtil::matches(index.data(SettingsModel::SearchTextRole).toString(), m_terms);
}

// ---------------------------------------------------------------------------

CategoryModel::CategoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

CategoryModel *CategoryModel::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    if (!s_categoryInstance) {
        s_categoryInstance = new CategoryModel(QCoreApplication::instance());
    }
    QJSEngine::setObjectOwnership(s_categoryInstance, QJSEngine::CppOwnership);
    return s_categoryInstance;
}

int CategoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : settingsCategories().size();
}

QHash<int, QByteArray> CategoryModel::roleNames() const
{
    return {
        {IdRole, "categoryId"},
        {TitleRole, "title"},
        {SubtitleRole, "subtitle"},
        {IconRole, "iconName"},
        {IsShortcutsRole, "isShortcuts"},
        {EntryCountRole, "entryCount"},
    };
}

QVariant CategoryModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return QVariant();
    }
    const SettingsCategory &category = settingsCategories().at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return category.title.toString();
    case IdRole:
        return QString::fromLatin1(category.id);
    case SubtitleRole:
        return category.subtitle.toString();
    case IconRole:
        return QString::fromLatin1(category.icon);
    case IsShortcutsRole:
        return QLatin1String(category.id) == QLatin1String(ShortcutsCategoryId);
    case EntryCountRole:
        return SettingsModel::instance()->countInCategory(QString::fromLatin1(category.id));
    }
    return QVariant();
}

QString CategoryModel::title(const QString &id) const
{
    const SettingsCategory *category = findCategory(id);
    return category ? category->title.toString() : QString();
}

QString CategoryModel::subtitle(const QString &id) const
{
    const SettingsCategory *category = findCategory(id);
    return category ? category->subtitle.toString() : QString();
}

QString CategoryModel::icon(const QString &id) const
{
    const SettingsCategory *category = findCategory(id);
    return category ? QString::fromLatin1(category->icon) : QString();
}

QStringList CategoryModel::ids() const
{
    QStringList result;
    for (const SettingsCategory &category : settingsCategories()) {
        result.append(QString::fromLatin1(category.id));
    }
    return result;
}

} // namespace InfiniteDesktop
