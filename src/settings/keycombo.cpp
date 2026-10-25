/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "keycombo.h"

#include <QStringList>

#include <optional>

namespace InfinitDesktop
{

namespace KeyCombo
{

namespace
{

struct KeyName
{
    Qt::Key key;
    const char *name;
};

// The names the service writes (shortcuts.cpp there), for keys without the keypad modifier.
constexpr KeyName s_names[] = {
    {Qt::Key_Backspace, "Backspace"}, {Qt::Key_Tab, "Tab"},   {Qt::Key_Backtab, "Tab"},    {Qt::Key_Return, "Enter"}, {Qt::Key_Enter, "Enter"},
    {Qt::Key_Escape, "Esc"},          {Qt::Key_Space, "Space"}, {Qt::Key_PageUp, "PgUp"},   {Qt::Key_PageDown, "PgDown"}, {Qt::Key_End, "End"},
    {Qt::Key_Home, "Home"},           {Qt::Key_Left, "Left"},   {Qt::Key_Up, "Up"},         {Qt::Key_Right, "Right"},  {Qt::Key_Down, "Down"},
    {Qt::Key_Insert, "Ins"},          {Qt::Key_Delete, "Del"},  {Qt::Key_Equal, "="},       {Qt::Key_Plus, "="},       {Qt::Key_Comma, ","},
    {Qt::Key_Minus, "-"},             {Qt::Key_Period, "."},
};

// Accepted when reading, as in the service.
constexpr KeyName s_aliases[] = {
    {Qt::Key_Return, "Return"}, {Qt::Key_Escape, "Escape"}, {Qt::Key_PageUp, "PageUp"}, {Qt::Key_PageDown, "PageDown"}, {Qt::Key_Insert, "Insert"},
    {Qt::Key_Delete, "Delete"}, {Qt::Key_Equal, "Plus"},    {Qt::Key_Equal, "Equal"},   {Qt::Key_Minus, "Minus"},
};

QString keyText(QKeyCombination combination)
{
    const Qt::Key key = combination.key();
    const bool keypad = combination.keyboardModifiers().testFlag(Qt::KeypadModifier);
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return QString(QChar(char16_t(key)));
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return keypad ? QStringLiteral("Num%1").arg(int(key - Qt::Key_0)) : QString(QChar(char16_t(key)));
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return QStringLiteral("F%1").arg(int(key - Qt::Key_F1) + 1);
    }
    if (keypad) {
        switch (key) {
        case Qt::Key_Asterisk:
            return QStringLiteral("Num*");
        case Qt::Key_Plus:
            return QStringLiteral("Num+");
        case Qt::Key_Minus:
            return QStringLiteral("Num-");
        case Qt::Key_Slash:
            return QStringLiteral("Num/");
        case Qt::Key_Enter:
        case Qt::Key_Return:
            return QStringLiteral("Enter");
        default:
            break;
        }
    }
    for (const KeyName &name : s_names) {
        if (name.key == key) {
            return QLatin1String(name.name);
        }
    }
    return QString();
}

std::optional<QKeyCombination> keyFromText(const QString &name)
{
    if (name.isEmpty()) {
        return std::nullopt;
    }
    if (name.size() == 1) {
        const QChar c = name.at(0).toUpper();
        if ((c >= QLatin1Char('A') && c <= QLatin1Char('Z')) || (c >= QLatin1Char('0') && c <= QLatin1Char('9'))) {
            return QKeyCombination(Qt::Key(c.unicode()));
        }
        if (c == QLatin1Char('+')) {
            return QKeyCombination(Qt::KeypadModifier, Qt::Key_Plus);
        }
    }
    if (name.size() == 4 && name.startsWith(QLatin1String("Num"), Qt::CaseInsensitive)) {
        const QChar last = name.at(3);
        if (last.isDigit()) {
            return QKeyCombination(Qt::KeypadModifier, Qt::Key(Qt::Key_0 + last.digitValue()));
        }
        switch (last.unicode()) {
        case '*':
            return QKeyCombination(Qt::KeypadModifier, Qt::Key_Asterisk);
        case '+':
            return QKeyCombination(Qt::KeypadModifier, Qt::Key_Plus);
        case '-':
            return QKeyCombination(Qt::KeypadModifier, Qt::Key_Minus);
        case '/':
            return QKeyCombination(Qt::KeypadModifier, Qt::Key_Slash);
        default:
            break;
        }
    }
    if ((name.startsWith(QLatin1Char('F')) || name.startsWith(QLatin1Char('f'))) && name.size() <= 3) {
        bool ok = false;
        const int number = name.mid(1).toInt(&ok);
        if (ok && number >= 1 && number <= 24) {
            return QKeyCombination(Qt::Key(Qt::Key_F1 + number - 1));
        }
    }
    for (const KeyName &key : s_names) {
        if (name.compare(QLatin1String(key.name), Qt::CaseInsensitive) == 0) {
            // The first entry of a name wins ("=" is Key_Equal, "Tab" is Key_Tab).
            return QKeyCombination(key.key);
        }
    }
    for (const KeyName &key : s_aliases) {
        if (name.compare(QLatin1String(key.name), Qt::CaseInsensitive) == 0) {
            return QKeyCombination(key.key);
        }
    }
    return std::nullopt;
}

} // namespace

QString toText(const QKeySequence &sequence)
{
    if (sequence.count() != 1) {
        return QString();
    }
    const QKeyCombination combination = sequence[0];
    const QString key = keyText(combination);
    if (key.isEmpty()) {
        return QString();
    }
    const Qt::KeyboardModifiers modifiers = combination.keyboardModifiers();
    QString text;
    if (modifiers & Qt::MetaModifier) {
        text += QLatin1String("Win+");
    }
    if (modifiers & Qt::ControlModifier) {
        text += QLatin1String("Ctrl+");
    }
    if (modifiers & Qt::AltModifier) {
        text += QLatin1String("Alt+");
    }
    if (modifiers & Qt::ShiftModifier) {
        text += QLatin1String("Shift+");
    }
    return text + key;
}

bool isSupported(const QKeySequence &sequence)
{
    return !toText(sequence).isEmpty();
}

QKeySequence fromText(const QString &input)
{
    QString rest = input.trimmed();
    if (rest.isEmpty()) {
        return QKeySequence();
    }
    // Split on '+', but a trailing "+" (as in "Win+Alt++") is the key itself.
    QStringList parts;
    while (!rest.isEmpty()) {
        const int plus = rest.indexOf(QLatin1Char('+'));
        if (plus < 0 || plus == rest.size() - 1) {
            parts.append(rest);
            break;
        }
        if (plus == 0) {
            return QKeySequence();
        }
        parts.append(rest.left(plus));
        rest = rest.mid(plus + 1);
    }
    Qt::KeyboardModifiers modifiers;
    for (int i = 0; i < parts.size() - 1; ++i) {
        const QString part = parts[i].trimmed().toLower();
        if (part == QLatin1String("win") || part == QLatin1String("meta") || part == QLatin1String("super")) {
            modifiers |= Qt::MetaModifier;
        } else if (part == QLatin1String("alt")) {
            modifiers |= Qt::AltModifier;
        } else if (part == QLatin1String("ctrl") || part == QLatin1String("control") || part == QLatin1String("strg")) {
            modifiers |= Qt::ControlModifier;
        } else if (part == QLatin1String("shift") || part == QLatin1String("umschalt")) {
            modifiers |= Qt::ShiftModifier;
        } else {
            return QKeySequence();
        }
    }
    const std::optional<QKeyCombination> key = keyFromText(parts.last().trimmed());
    if (!key) {
        return QKeySequence();
    }
    return QKeySequence(QKeyCombination(modifiers | key->keyboardModifiers(), key->key()));
}

QString toList(const QList<QKeySequence> &sequences)
{
    QStringList parts;
    for (const QKeySequence &sequence : sequences) {
        const QString text = toText(sequence);
        if (!text.isEmpty() && !parts.contains(text)) {
            parts.append(text);
        }
    }
    return parts.join(QLatin1Char(';'));
}

QList<QKeySequence> fromList(const QString &text)
{
    QList<QKeySequence> result;
    const QStringList entries = text.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        const QKeySequence sequence = fromText(entry);
        if (!sequence.isEmpty() && !result.contains(sequence)) {
            result.append(sequence);
        }
    }
    return result;
}

} // namespace KeyCombo

} // namespace InfinitDesktop
