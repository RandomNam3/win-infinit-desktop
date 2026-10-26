/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "localizer.h"

#include <QFile>
#include <QJSValue>
#include <QLocale>

#include <cmath>
#include <optional>

namespace Wid
{

namespace
{

/// Unescapes the body of one quoted .po string ("…" without the quotes).
QByteArray unescape(const QByteArray &text)
{
    QByteArray result;
    result.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c != '\\' || i + 1 >= text.size()) {
            result.append(c);
            continue;
        }
        const char next = text[++i];
        switch (next) {
        case 'n':
            result.append('\n');
            break;
        case 't':
            result.append('\t');
            break;
        case 'r':
            result.append('\r');
            break;
        case '"':
            result.append('"');
            break;
        case '\\':
            result.append('\\');
            break;
        default:
            result.append('\\');
            result.append(next);
            break;
        }
    }
    return result;
}

/// The quoted string at the start of `text`, or nullopt.
std::optional<QByteArray> quoted(const QByteArray &input)
{
    const QByteArray text = input.trimmed();
    if (text.size() < 2 || text.front() != '"' || text.back() != '"') {
        return std::nullopt;
    }
    return unescape(text.mid(1, text.size() - 2));
}

struct Entry
{
    QByteArray context;
    QByteArray id;
    QByteArray idPlural;
    QList<QByteArray> strings;
    bool hasContext = false;
    bool fuzzy = false;
};

} // namespace

// ---------------------------------------------------------------------------------------

QString Catalog::key(const QString &context, const QString &text)
{
    return context.isEmpty() ? text : context + QChar(0x04) + text;
}

void Catalog::clear()
{
    m_singular.clear();
    m_plural.clear();
    m_rule = PluralRule::NotOne;
}

bool Catalog::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    return parse(file.readAll());
}

bool Catalog::parse(const QByteArray &po)
{
    clear();
    Entry entry;
    // What the continuation lines ("…") belong to: 0 none, 1 msgctxt, 2 msgid, 3 msgid_plural, 4 msgstr[n].
    int target = 0;
    int stringIndex = 0;
    bool sawAny = false;

    const auto commit = [this, &entry]() {
        if (!entry.id.isEmpty() && !entry.fuzzy) {
            const QString context = entry.hasContext ? QString::fromUtf8(entry.context) : QString();
            const QString id = QString::fromUtf8(entry.id);
            if (entry.idPlural.isEmpty()) {
                if (!entry.strings.isEmpty() && !entry.strings.constFirst().isEmpty()) {
                    m_singular.insert(key(context, id), QString::fromUtf8(entry.strings.constFirst()));
                }
            } else {
                QStringList forms;
                bool complete = !entry.strings.isEmpty();
                for (const QByteArray &form : std::as_const(entry.strings)) {
                    complete = complete && !form.isEmpty();
                    forms.append(QString::fromUtf8(form));
                }
                if (complete) {
                    m_plural.insert(key(context, id), forms);
                }
            }
        } else if (entry.id.isEmpty() && !entry.strings.isEmpty()) {
            // The header: only the plural rule matters here.
            const QByteArray header = entry.strings.constFirst();
            const qsizetype plural = header.indexOf("plural=");
            if (plural >= 0) {
                QByteArray rule = header.mid(plural + 7);
                rule = rule.left(rule.indexOf('\n'));
                rule.replace(' ', "");
                if (header.contains("nplurals=1")) {
                    m_rule = PluralRule::Single;
                } else if (rule.startsWith("(n>1)") || rule.startsWith("n>1")) {
                    m_rule = PluralRule::Many;
                } else {
                    m_rule = PluralRule::NotOne;
                }
            }
        }
        entry = Entry();
    };

    const QList<QByteArray> lines = po.split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith('#')) {
            if (line.startsWith("#,") && line.contains("fuzzy")) {
                if (target >= 2) {
                    commit();
                    target = 0;
                }
                entry.fuzzy = true;
            }
            continue;
        }
        if (line.startsWith('"')) {
            const auto text = quoted(line);
            if (!text) {
                return false;
            }
            switch (target) {
            case 1:
                entry.context += *text;
                break;
            case 2:
                entry.id += *text;
                break;
            case 3:
                entry.idPlural += *text;
                break;
            case 4:
                entry.strings[stringIndex] += *text;
                break;
            default:
                return false;
            }
            continue;
        }
        const qsizetype space = line.indexOf(' ');
        if (space < 0) {
            return false;
        }
        const QByteArray keyword = line.left(space);
        const auto text = quoted(line.mid(space + 1));
        if (!text) {
            return false;
        }
        sawAny = true;
        if (keyword == "msgctxt") {
            if (target >= 2) {
                commit();
            }
            const bool fuzzy = entry.fuzzy;
            entry = Entry();
            entry.fuzzy = fuzzy;
            entry.hasContext = true;
            entry.context = *text;
            target = 1;
        } else if (keyword == "msgid") {
            if (target >= 3) {
                // The previous entry ends here (an entry without msgctxt).
                commit();
            }
            entry.id = *text;
            target = 2;
        } else if (keyword == "msgid_plural") {
            entry.idPlural = *text;
            target = 3;
        } else if (keyword == "msgstr") {
            entry.strings = QList<QByteArray>{*text};
            stringIndex = 0;
            target = 4;
        } else if (keyword.startsWith("msgstr[") && keyword.endsWith(']')) {
            bool ok = false;
            const int index = keyword.mid(7, keyword.size() - 8).toInt(&ok);
            if (!ok || index < 0 || index > 5) {
                return false;
            }
            while (entry.strings.size() <= index) {
                entry.strings.append(QByteArray());
            }
            entry.strings[index] = *text;
            stringIndex = index;
            target = 4;
        } else {
            return false;
        }
    }
    commit();
    return sawAny;
}

QString Catalog::find(const QString &context, const QString &text) const
{
    return m_singular.value(key(context, text));
}

int Catalog::pluralIndex(qint64 n) const
{
    switch (m_rule) {
    case PluralRule::Single:
        return 0;
    case PluralRule::Many:
        return n > 1 ? 1 : 0;
    case PluralRule::NotOne:
        break;
    }
    return n != 1 ? 1 : 0;
}

QString Catalog::findPlural(const QString &context, const QString &singular, qint64 n) const
{
    const auto it = m_plural.constFind(key(context, singular));
    if (it == m_plural.constEnd() || it->isEmpty()) {
        return QString();
    }
    return it->value(std::min<int>(pluralIndex(n), int(it->size()) - 1));
}

// ---------------------------------------------------------------------------------------

Localizer &Localizer::instance()
{
    static Localizer localizer;
    return localizer;
}

void Localizer::loadForSystemLanguage()
{
    QStringList languages;
    const QString forced = qEnvironmentVariable("WID_LANGUAGE");
    if (!forced.isEmpty()) {
        languages.append(forced);
    } else {
        languages = QLocale::system().uiLanguages();
    }
    for (const QString &language : std::as_const(languages)) {
        const QString code = language.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0).toLower();
        if (code == QLatin1String("en")) {
            break;
        }
        Catalog catalog;
        if (catalog.load(QStringLiteral(":/i18n/%1.po").arg(code))) {
            m_catalog = std::move(catalog);
            m_language = code;
            return;
        }
    }
    m_catalog.clear();
    m_language = QStringLiteral("en");
}

QString Localizer::translate(const QString &context, const QString &text) const
{
    const QString translated = m_catalog.find(context, text);
    return translated.isNull() ? text : translated;
}

QString Localizer::translatePlural(const QString &context, const QString &singular, const QString &plural, qint64 n) const
{
    const QString translated = m_catalog.findPlural(context, singular, n);
    if (!translated.isNull()) {
        return translated;
    }
    return n == 1 ? singular : plural;
}

QString Localizer::substitute(const QString &text, const QStringList &arguments)
{
    if (arguments.isEmpty()) {
        return text;
    }
    QString result;
    result.reserve(text.size() + 16);
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('%') && i + 1 < text.size() && text.at(i + 1).isDigit() && text.at(i + 1) != QLatin1Char('0')) {
            const int index = text.at(i + 1).digitValue();
            if (index <= arguments.size()) {
                result += arguments.at(index - 1);
                ++i;
                continue;
            }
        }
        result += c;
    }
    return result;
}

QString Localizer::argumentText(const QVariant &input)
{
    QVariant value = input;
    if (value.metaType() == QMetaType::fromType<QJSValue>()) {
        value = value.value<QJSValue>().toVariant();
    }
    switch (value.typeId()) {
    case QMetaType::Double:
    case QMetaType::Float: {
        const double number = value.toDouble();
        if (std::isfinite(number) && std::floor(number) == number && std::abs(number) < 1e15) {
            return QString::number(qint64(number));
        }
        return QLocale().toString(number);
    }
    case QMetaType::Bool:
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    default:
        return value.toString();
    }
}

// ---------------------------------------------------------------------------------------

namespace
{

QStringList collect(std::initializer_list<const QVariant *> parameters)
{
    QStringList result;
    int last = -1;
    int index = 0;
    for (const QVariant *parameter : parameters) {
        if (parameter->isValid()) {
            last = index;
        }
        ++index;
    }
    index = 0;
    for (const QVariant *parameter : parameters) {
        if (index > last) {
            break;
        }
        result.append(Localizer::argumentText(*parameter));
        ++index;
    }
    return result;
}

qint64 countOf(const QVariant &value)
{
    QVariant v = value;
    if (v.metaType() == QMetaType::fromType<QJSValue>()) {
        v = v.value<QJSValue>().toVariant();
    }
    return qint64(std::llround(v.toDouble()));
}

} // namespace

LocalizedContext::LocalizedContext(QObject *parent)
    : QObject(parent)
{
}

QString LocalizedContext::i18n(const QString &text, const QVariant &p1, const QVariant &p2, const QVariant &p3, const QVariant &p4, const QVariant &p5,
                               const QVariant &p6, const QVariant &p7, const QVariant &p8, const QVariant &p9) const
{
    return Localizer::substitute(Localizer::instance().translate(QString(), text), collect({&p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8, &p9}));
}

QString LocalizedContext::i18nc(const QString &context, const QString &text, const QVariant &p1, const QVariant &p2, const QVariant &p3, const QVariant &p4,
                                const QVariant &p5, const QVariant &p6, const QVariant &p7, const QVariant &p8, const QVariant &p9) const
{
    return Localizer::substitute(Localizer::instance().translate(context, text), collect({&p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8, &p9}));
}

QString LocalizedContext::i18np(const QString &singular, const QString &plural, const QVariant &p1, const QVariant &p2, const QVariant &p3, const QVariant &p4,
                                const QVariant &p5, const QVariant &p6, const QVariant &p7, const QVariant &p8, const QVariant &p9) const
{
    const qint64 n = countOf(p1);
    return Localizer::substitute(Localizer::instance().translatePlural(QString(), singular, plural, n), collect({&p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8, &p9}));
}

QString LocalizedContext::i18ncp(const QString &context, const QString &singular, const QString &plural, const QVariant &p1, const QVariant &p2,
                                 const QVariant &p3, const QVariant &p4, const QVariant &p5, const QVariant &p6, const QVariant &p7, const QVariant &p8,
                                 const QVariant &p9) const
{
    const qint64 n = countOf(p1);
    return Localizer::substitute(Localizer::instance().translatePlural(context, singular, plural, n), collect({&p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8, &p9}));
}

} // namespace Wid
