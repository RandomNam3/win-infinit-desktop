/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace Wid
{

/**
 * Translations without KDE Frameworks: a gettext catalog (.po, compiled into the program
 * as a resource) and the i18n functions of KI18n with the same meaning, so that the QML
 * and the catalog can be shared with KDE Infinit Desktop.
 *
 *   i18n(text, args…), i18nc(context, text, args…),
 *   i18np(singular, plural, n, args…), i18ncp(context, singular, plural, n, args…)
 *
 * %1 … %9 are replaced in one pass (an argument containing "%2" stays as it is). For the
 * plural forms %1 is n. Untranslated texts are shown in English.
 */
class Catalog
{
public:
    /// Parses .po text. Fuzzy and empty translations are skipped. Returns false on garbage.
    bool parse(const QByteArray &po);
    bool load(const QString &path);
    void clear();

    bool isEmpty() const
    {
        return m_singular.isEmpty() && m_plural.isEmpty();
    }
    qsizetype size() const
    {
        return m_singular.size() + m_plural.size();
    }

    /// The translation, or a null QString.
    QString find(const QString &context, const QString &text) const;
    QString findPlural(const QString &context, const QString &singular, qint64 n) const;
    /// Index of the plural form for `n` (n != 1 for German and English).
    int pluralIndex(qint64 n) const;

private:
    static QString key(const QString &context, const QString &text);

    QHash<QString, QString> m_singular;
    QHash<QString, QStringList> m_plural;
    enum class PluralRule {
        NotOne, ///< n != 1
        Single, ///< nplurals=1
        Many, ///< n > 1 (French)
    } m_rule = PluralRule::NotOne;
};

class Localizer
{
public:
    static Localizer &instance();

    /// Loads ":/i18n/<language>.po" for the first matching UI language (or $WID_LANGUAGE).
    void loadForSystemLanguage();
    void setCatalog(Catalog catalog)
    {
        m_catalog = std::move(catalog);
    }
    const Catalog &catalog() const
    {
        return m_catalog;
    }
    /// "de", or "en" without a catalog.
    QString language() const
    {
        return m_language;
    }

    QString translate(const QString &context, const QString &text) const;
    QString translatePlural(const QString &context, const QString &singular, const QString &plural, qint64 n) const;

    /// Replaces %1 … %9 in one pass.
    static QString substitute(const QString &text, const QStringList &arguments);
    static QString argumentText(const QVariant &value);

private:
    Catalog m_catalog;
    QString m_language = QStringLiteral("en");
};

// ---- C++ API ---------------------------------------------------------------------------

namespace detail
{
inline QString toArgument(const QString &value)
{
    return value;
}
inline QString toArgument(const char *value)
{
    return QString::fromUtf8(value);
}
inline QString toArgument(const QStringView value)
{
    return value.toString();
}
template<typename T>
QString toArgument(const T &value)
    requires std::is_arithmetic_v<T>
{
    return QString::number(value);
}
template<typename... Args>
QStringList toArguments(const Args &...args)
{
    return QStringList{toArgument(args)...};
}
} // namespace detail

template<typename... Args>
QString i18n(const char *text, const Args &...args)
{
    return Localizer::substitute(Localizer::instance().translate(QString(), QString::fromUtf8(text)), detail::toArguments(args...));
}

template<typename... Args>
QString i18nc(const char *context, const char *text, const Args &...args)
{
    return Localizer::substitute(Localizer::instance().translate(QString::fromUtf8(context), QString::fromUtf8(text)), detail::toArguments(args...));
}

template<typename... Args>
QString i18np(const char *singular, const char *plural, qint64 n, const Args &...args)
{
    QStringList arguments = detail::toArguments(args...);
    arguments.prepend(QString::number(n));
    return Localizer::substitute(Localizer::instance().translatePlural(QString(), QString::fromUtf8(singular), QString::fromUtf8(plural), n), arguments);
}

template<typename... Args>
QString i18ncp(const char *context, const char *singular, const char *plural, qint64 n, const Args &...args)
{
    QStringList arguments = detail::toArguments(args...);
    arguments.prepend(QString::number(n));
    return Localizer::substitute(Localizer::instance().translatePlural(QString::fromUtf8(context), QString::fromUtf8(singular), QString::fromUtf8(plural), n),
                                 arguments);
}

/// A text translated when it is shown (for static tables), like KLazyLocalizedString.
struct LazyText
{
    const char *context = nullptr;
    const char *text = nullptr;

    QString toString() const
    {
        if (!text) {
            return QString();
        }
        return Localizer::instance().translate(context ? QString::fromUtf8(context) : QString(), QString::fromUtf8(text));
    }
    bool isEmpty() const
    {
        return !text || !*text;
    }
};

constexpr LazyText kli18n(const char *text)
{
    return LazyText{nullptr, text};
}
constexpr LazyText kli18nc(const char *context, const char *text)
{
    return LazyText{context, text};
}

// ---- QML API ---------------------------------------------------------------------------

/**
 * Context object of a QML engine that provides i18n() and friends, like KLocalizedContext:
 * engine.rootContext()->setContextObject(new LocalizedContext(&engine)).
 */
class LocalizedContext : public QObject
{
    Q_OBJECT

public:
    explicit LocalizedContext(QObject *parent = nullptr);

    Q_INVOKABLE QString i18n(const QString &text, const QVariant &p1 = {}, const QVariant &p2 = {}, const QVariant &p3 = {}, const QVariant &p4 = {},
                             const QVariant &p5 = {}, const QVariant &p6 = {}, const QVariant &p7 = {}, const QVariant &p8 = {}, const QVariant &p9 = {}) const;
    Q_INVOKABLE QString i18nc(const QString &context, const QString &text, const QVariant &p1 = {}, const QVariant &p2 = {}, const QVariant &p3 = {},
                              const QVariant &p4 = {}, const QVariant &p5 = {}, const QVariant &p6 = {}, const QVariant &p7 = {}, const QVariant &p8 = {},
                              const QVariant &p9 = {}) const;
    Q_INVOKABLE QString i18np(const QString &singular, const QString &plural, const QVariant &p1 = {}, const QVariant &p2 = {}, const QVariant &p3 = {},
                              const QVariant &p4 = {}, const QVariant &p5 = {}, const QVariant &p6 = {}, const QVariant &p7 = {}, const QVariant &p8 = {},
                              const QVariant &p9 = {}) const;
    Q_INVOKABLE QString i18ncp(const QString &context, const QString &singular, const QString &plural, const QVariant &p1 = {}, const QVariant &p2 = {},
                               const QVariant &p3 = {}, const QVariant &p4 = {}, const QVariant &p5 = {}, const QVariant &p6 = {}, const QVariant &p7 = {},
                               const QVariant &p8 = {}, const QVariant &p9 = {}) const;
};

} // namespace Wid
