/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "shortcuts.h"

#include <QSettings>

namespace Wid
{

namespace
{

// The Win32 constants, spelled out so that this file stays free of <windows.h> and can be
// unit tested anywhere.
constexpr unsigned ModAlt = 0x0001;
constexpr unsigned ModControl = 0x0002;
constexpr unsigned ModShift = 0x0004;
constexpr unsigned ModWin = 0x0008;

constexpr unsigned VkBack = 0x08;
constexpr unsigned VkTab = 0x09;
constexpr unsigned VkReturn = 0x0D;
constexpr unsigned VkShift = 0x10;
constexpr unsigned VkControl = 0x11;
constexpr unsigned VkMenu = 0x12;
constexpr unsigned VkEscape = 0x1B;
constexpr unsigned VkSpace = 0x20;
constexpr unsigned VkPrior = 0x21;
constexpr unsigned VkNext = 0x22;
constexpr unsigned VkEnd = 0x23;
constexpr unsigned VkHome = 0x24;
constexpr unsigned VkLeft = 0x25;
constexpr unsigned VkUp = 0x26;
constexpr unsigned VkRight = 0x27;
constexpr unsigned VkDown = 0x28;
constexpr unsigned VkInsert = 0x2D;
constexpr unsigned VkDelete = 0x2E;
constexpr unsigned VkLWin = 0x5B;
constexpr unsigned VkRWin = 0x5C;
constexpr unsigned VkNumpad0 = 0x60;
constexpr unsigned VkMultiply = 0x6A;
constexpr unsigned VkAdd = 0x6B;
constexpr unsigned VkSubtract = 0x6D;
constexpr unsigned VkDivide = 0x6F;
constexpr unsigned VkF1 = 0x70;
constexpr unsigned VkLShift = 0xA0;
constexpr unsigned VkRMenu = 0xA5;
constexpr unsigned VkOemPlus = 0xBB; // '=' / '+' key next to backspace
constexpr unsigned VkOemComma = 0xBC;
constexpr unsigned VkOemMinus = 0xBD;
constexpr unsigned VkOemPeriod = 0xBE;

struct KeyName
{
    const char *name;
    unsigned vk;
};

// Names as formatKeyCombo writes them; parsing also accepts a few aliases (below).
constexpr KeyName s_keyNames[] = {
    {"Backspace", VkBack}, {"Tab", VkTab},     {"Enter", VkReturn},  {"Esc", VkEscape},     {"Space", VkSpace},
    {"PgUp", VkPrior},     {"PgDown", VkNext}, {"End", VkEnd},       {"Home", VkHome},      {"Left", VkLeft},
    {"Up", VkUp},          {"Right", VkRight}, {"Down", VkDown},     {"Ins", VkInsert},     {"Del", VkDelete},
    {"Num*", VkMultiply},  {"Num+", VkAdd},    {"Num-", VkSubtract}, {"Num/", VkDivide},    {"=", VkOemPlus},
    {",", VkOemComma},     {"-", VkOemMinus},  {".", VkOemPeriod},
};

constexpr KeyName s_aliases[] = {
    {"Return", VkReturn}, {"Escape", VkEscape}, {"PageUp", VkPrior}, {"PageDown", VkNext}, {"Insert", VkInsert},
    {"Delete", VkDelete}, {"Plus", VkOemPlus},  {"Equal", VkOemPlus}, {"Minus", VkOemMinus}, {"+", VkAdd},
};

constexpr KeyCombo combo(unsigned modifiers, unsigned vk)
{
    return KeyCombo{modifiers, vk};
}

constexpr unsigned WinAlt = ModWin | ModAlt;
constexpr unsigned WinAltShift = ModWin | ModAlt | ModShift;
constexpr unsigned WinCtrlAlt = ModWin | ModControl | ModAlt;

std::optional<unsigned> keyFromName(const QString &name)
{
    if (name.isEmpty()) {
        return std::nullopt;
    }
    if (name.size() == 1) {
        const QChar c = name.at(0).toUpper();
        if ((c >= QLatin1Char('A') && c <= QLatin1Char('Z')) || (c >= QLatin1Char('0') && c <= QLatin1Char('9'))) {
            return c.unicode();
        }
    }
    for (const KeyName &key : s_keyNames) {
        if (name.compare(QLatin1String(key.name), Qt::CaseInsensitive) == 0) {
            return key.vk;
        }
    }
    for (const KeyName &key : s_aliases) {
        if (name.compare(QLatin1String(key.name), Qt::CaseInsensitive) == 0) {
            return key.vk;
        }
    }
    if ((name.startsWith(QLatin1Char('F')) || name.startsWith(QLatin1Char('f'))) && name.size() <= 3) {
        bool ok = false;
        const int number = name.mid(1).toInt(&ok);
        if (ok && number >= 1 && number <= 24) {
            return VkF1 + unsigned(number - 1);
        }
    }
    if (name.size() == 4 && name.startsWith(QLatin1String("Num"), Qt::CaseInsensitive) && name.at(3).isDigit()) {
        return VkNumpad0 + unsigned(name.at(3).digitValue());
    }
    return std::nullopt;
}

QString nameOfKey(unsigned vk)
{
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        return QString(QChar(char16_t(vk)));
    }
    if (vk >= VkF1 && vk < VkF1 + 24) {
        return QStringLiteral("F%1").arg(vk - VkF1 + 1);
    }
    if (vk >= VkNumpad0 && vk <= VkNumpad0 + 9) {
        return QStringLiteral("Num%1").arg(vk - VkNumpad0);
    }
    for (const KeyName &key : s_keyNames) {
        if (key.vk == vk) {
            return QLatin1String(key.name);
        }
    }
    return QStringLiteral("0x%1").arg(vk, 2, 16, QLatin1Char('0'));
}

} // namespace

bool isModifierKey(unsigned vk)
{
    return vk == VkShift || vk == VkControl || vk == VkMenu || vk == VkLWin || vk == VkRWin || (vk >= VkLShift && vk <= VkRMenu);
}

const QList<ShortcutDefinition> &shortcutDefinitions()
{
    static const QList<ShortcutDefinition> definitions{
        {ShortcutAction::ZoomIn, "InfinitDesktop ZoomIn", "Zoom In", true, {combo(WinAlt, VkOemPlus), combo(WinAlt, VkAdd)}},
        {ShortcutAction::ZoomOut, "InfinitDesktop ZoomOut", "Zoom Out", true, {combo(WinAlt, VkOemMinus), combo(WinAlt, VkSubtract)}},
        {ShortcutAction::ZoomReset, "InfinitDesktop ZoomReset", "Zoom to 100 %", false, {combo(WinAlt, '0')}},
        {ShortcutAction::Overview, "InfinitDesktop Overview", "Show All Windows", false, {combo(WinAlt, 'W')}},
        {ShortcutAction::Home, "InfinitDesktop Home", "Go Home", false, {combo(WinAlt, 'H')}},
        {ShortcutAction::CenterWindow, "InfinitDesktop CenterWindow", "Center Active Window", false, {combo(WinAlt, 'C')}},
        {ShortcutAction::PanLeft, "InfinitDesktop PanLeft", "Pan Left", true, {combo(WinAltShift, VkLeft)}},
        {ShortcutAction::PanRight, "InfinitDesktop PanRight", "Pan Right", true, {combo(WinAltShift, VkRight)}},
        {ShortcutAction::PanUp, "InfinitDesktop PanUp", "Pan Up", true, {combo(WinAltShift, VkUp)}},
        {ShortcutAction::PanDown, "InfinitDesktop PanDown", "Pan Down", true, {combo(WinAltShift, VkDown)}},
        {ShortcutAction::FocusLeft, "InfinitDesktop FocusLeft", "Focus Window to the Left", false, {combo(WinCtrlAlt, VkLeft)}},
        {ShortcutAction::FocusRight, "InfinitDesktop FocusRight", "Focus Window to the Right", false, {combo(WinCtrlAlt, VkRight)}},
        {ShortcutAction::FocusUp, "InfinitDesktop FocusUp", "Focus Window Above", false, {combo(WinCtrlAlt, VkUp)}},
        {ShortcutAction::FocusDown, "InfinitDesktop FocusDown", "Focus Window Below", false, {combo(WinCtrlAlt, VkDown)}},
        {ShortcutAction::GotoBookmark1, "InfinitDesktop GotoBookmark1", "Go to Bookmark 1", false, {combo(WinAlt, '1')}},
        {ShortcutAction::GotoBookmark2, "InfinitDesktop GotoBookmark2", "Go to Bookmark 2", false, {combo(WinAlt, '2')}},
        {ShortcutAction::GotoBookmark3, "InfinitDesktop GotoBookmark3", "Go to Bookmark 3", false, {combo(WinAlt, '3')}},
        {ShortcutAction::GotoBookmark4, "InfinitDesktop GotoBookmark4", "Go to Bookmark 4", false, {combo(WinAlt, '4')}},
        {ShortcutAction::SaveBookmark1, "InfinitDesktop SaveBookmark1", "Save Bookmark 1", false, {combo(WinCtrlAlt, '1')}},
        {ShortcutAction::SaveBookmark2, "InfinitDesktop SaveBookmark2", "Save Bookmark 2", false, {combo(WinCtrlAlt, '2')}},
        {ShortcutAction::SaveBookmark3, "InfinitDesktop SaveBookmark3", "Save Bookmark 3", false, {combo(WinCtrlAlt, '3')}},
        {ShortcutAction::SaveBookmark4, "InfinitDesktop SaveBookmark4", "Save Bookmark 4", false, {combo(WinCtrlAlt, '4')}},
        {ShortcutAction::TogglePin, "InfinitDesktop TogglePin", "Pin Window to Screen", false, {combo(WinAlt, 'T')}},
        {ShortcutAction::TogglePinAbove, "InfinitDesktop TogglePinAbove", "Pin Window to Screen, Always on Top", false, {combo(WinAltShift, 'T')}},
        {ShortcutAction::ToggleEnabled, "InfinitDesktop ToggleEnabled", "Pause / Resume", false, {}},
    };
    return definitions;
}

const ShortcutDefinition *findShortcut(ShortcutAction action)
{
    for (const ShortcutDefinition &definition : shortcutDefinitions()) {
        if (definition.action == action) {
            return &definition;
        }
    }
    return nullptr;
}

std::optional<KeyCombo> parseKeyCombo(const QString &text)
{
    QString rest = text.trimmed();
    if (rest.isEmpty()) {
        return std::nullopt;
    }
    KeyCombo result;
    // Split on '+', but a trailing "+" (as in "Win+Alt++") is the key itself.
    QStringList parts;
    while (!rest.isEmpty()) {
        const int plus = rest.indexOf(QLatin1Char('+'));
        if (plus < 0 || plus == rest.size() - 1) {
            parts.append(rest);
            break;
        }
        if (plus == 0) {
            return std::nullopt;
        }
        parts.append(rest.left(plus));
        rest = rest.mid(plus + 1);
    }
    for (int i = 0; i < parts.size() - 1; ++i) {
        const QString part = parts[i].trimmed().toLower();
        if (part == QLatin1String("win") || part == QLatin1String("meta") || part == QLatin1String("super")) {
            result.modifiers |= ModWin;
        } else if (part == QLatin1String("alt")) {
            result.modifiers |= ModAlt;
        } else if (part == QLatin1String("ctrl") || part == QLatin1String("control") || part == QLatin1String("strg")) {
            result.modifiers |= ModControl;
        } else if (part == QLatin1String("shift") || part == QLatin1String("umschalt")) {
            result.modifiers |= ModShift;
        } else {
            return std::nullopt;
        }
    }
    const std::optional<unsigned> key = keyFromName(parts.last().trimmed());
    if (!key || isModifierKey(*key)) {
        return std::nullopt;
    }
    result.vk = *key;
    return result;
}

QString formatKeyCombo(const KeyCombo &combo)
{
    QString text;
    if (combo.modifiers & ModWin) {
        text += QLatin1String("Win+");
    }
    if (combo.modifiers & ModControl) {
        text += QLatin1String("Ctrl+");
    }
    if (combo.modifiers & ModAlt) {
        text += QLatin1String("Alt+");
    }
    if (combo.modifiers & ModShift) {
        text += QLatin1String("Shift+");
    }
    return text + nameOfKey(combo.vk);
}

QList<KeyCombo> parseKeyCombos(const QString &text)
{
    QList<KeyCombo> result;
    const QStringList entries = text.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        if (const auto parsed = parseKeyCombo(entry)) {
            if (!result.contains(*parsed)) {
                result.append(*parsed);
            }
        }
    }
    return result;
}

QString formatKeyCombos(const QList<KeyCombo> &combos)
{
    QStringList parts;
    for (const KeyCombo &combo : combos) {
        parts.append(formatKeyCombo(combo));
    }
    return parts.join(QLatin1Char(';'));
}

ShortcutMap ShortcutMap::defaults()
{
    ShortcutMap map;
    for (const ShortcutDefinition &definition : shortcutDefinitions()) {
        for (const KeyCombo &combo : definition.defaults) {
            map.m_bindings.append(Binding{definition.action, combo});
        }
    }
    return map;
}

ShortcutMap ShortcutMap::load(QSettings &settings)
{
    ShortcutMap map;
    settings.beginGroup(QStringLiteral("Shortcuts"));
    for (const ShortcutDefinition &definition : shortcutDefinitions()) {
        const QString key = QLatin1String(definition.id);
        const QList<KeyCombo> combos = settings.contains(key) ? parseKeyCombos(settings.value(key).toString()) : definition.defaults;
        for (const KeyCombo &combo : combos) {
            // The first action wins a combo bound twice.
            if (!map.actionFor(combo)) {
                map.m_bindings.append(Binding{definition.action, combo});
            }
        }
    }
    settings.endGroup();
    return map;
}

std::optional<ShortcutAction> ShortcutMap::actionFor(const KeyCombo &combo) const
{
    for (const Binding &binding : m_bindings) {
        if (binding.combo == combo) {
            return binding.action;
        }
    }
    return std::nullopt;
}

QList<KeyCombo> ShortcutMap::combosFor(ShortcutAction action) const
{
    QList<KeyCombo> result;
    for (const Binding &binding : m_bindings) {
        if (binding.action == action) {
            result.append(binding.combo);
        }
    }
    return result;
}

} // namespace Wid
