/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "searchutil.h"

#include <QRegularExpression>

namespace InfiniteDesktop::SearchUtil
{

QString normalize(const QString &text, QList<int> *map)
{
    QString result;
    result.reserve(text.size());
    if (map) {
        map->clear();
        map->reserve(text.size());
    }
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        const QString decomposed = QString(c).normalized(QString::NormalizationForm_KD);
        for (const QChar d : decomposed) {
            if (d.category() == QChar::Mark_NonSpacing || d.category() == QChar::Mark_SpacingCombining || d.category() == QChar::Mark_Enclosing) {
                continue;
            }
            result.append(d.toLower());
            if (map) {
                map->append(i);
            }
        }
    }
    return result;
}

QStringList terms(const QString &query)
{
    static const QRegularExpression separators(QStringLiteral("[\\s,]+"));
    return normalize(query.trimmed()).split(separators, Qt::SkipEmptyParts);
}

bool matches(const QString &haystack, const QStringList &terms)
{
    for (const QString &term : terms) {
        if (!haystack.contains(term)) {
            return false;
        }
    }
    return true;
}

QString highlight(const QString &text, const QStringList &terms, const QColor &color)
{
    if (terms.isEmpty()) {
        return text.toHtmlEscaped();
    }
    QList<int> map;
    const QString normalized = normalize(text, &map);
    QList<bool> marked(text.size(), false);
    for (const QString &term : terms) {
        int from = 0;
        while (true) {
            const int index = normalized.indexOf(term, from);
            if (index < 0) {
                break;
            }
            const int first = map.at(index);
            const int last = map.at(index + term.size() - 1);
            for (int i = first; i <= last; ++i) {
                marked[i] = true;
            }
            from = index + term.size();
        }
    }

    const QString open = QStringLiteral("<b><font color=\"%1\">").arg(color.name(QColor::HexRgb));
    const QString close = QStringLiteral("</font></b>");
    QString result;
    bool inside = false;
    for (int i = 0; i < text.size(); ++i) {
        if (marked.at(i) != inside) {
            result += inside ? close : open;
            inside = marked.at(i);
        }
        result += QString(text.at(i)).toHtmlEscaped();
    }
    if (inside) {
        result += close;
    }
    return result;
}

} // namespace InfiniteDesktop::SearchUtil
