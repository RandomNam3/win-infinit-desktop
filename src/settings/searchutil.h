/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QColor>
#include <QList>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>

namespace InfinitDesktop
{

/**
 * Base of the filter models: re-filters the rows after the filter criteria
 * changed, using begin/endFilterChange() where available (Qt >= 6.10) and
 * invalidateRowsFilter() on older Qt versions.
 */
class FilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    void beginFilterUpdate()
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
#endif
    }

    void endFilterUpdate()
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
        invalidateRowsFilter();
#endif
    }
};

} // namespace InfinitDesktop

namespace InfinitDesktop::SearchUtil
{

/**
 * Lower case text without diacritics, so "Arbeitsflache" finds "Arbeitsfläche".
 * If @p map is given it receives, for every character of the result, the index
 * of the character in @p text it came from.
 */
QString normalize(const QString &text, QList<int> *map = nullptr);

/// Normalized search terms of a query (split at white space).
QStringList terms(const QString &query);

/// True if every term occurs in the (normalized) @p haystack.
bool matches(const QString &haystack, const QStringList &terms);

/// HTML (Text.StyledText) with all occurrences of @p terms emphasized.
QString highlight(const QString &text, const QStringList &terms, const QColor &color);

} // namespace InfinitDesktop::SearchUtil
