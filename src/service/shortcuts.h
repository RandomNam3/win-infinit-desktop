/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QList>
#include <QString>

#include <optional>

class QSettings;

namespace Wid
{

/// The actions of KDE Infinite Desktop's shortcut table (src/shared/shortcutdefs.h there).
enum class ShortcutAction {
    ZoomIn,
    ZoomOut,
    ZoomReset,
    Overview,
    Home,
    CenterWindow,
    PanLeft,
    PanRight,
    PanUp,
    PanDown,
    FocusLeft,
    FocusRight,
    FocusUp,
    FocusDown,
    GotoBookmark1,
    GotoBookmark2,
    GotoBookmark3,
    GotoBookmark4,
    SaveBookmark1,
    SaveBookmark2,
    SaveBookmark3,
    SaveBookmark4,
    TogglePin,
    TogglePinAbove,
    ToggleEnabled,
    NewNote,
    ToggleNotes,
    ShowMap,
    GroupFromSelection,
    AreaFromSelection,
    SaveGroupLayout,
    OpenGroup1,
    OpenGroup2,
    OpenGroup3,
    OpenGroup4,
    OpenGroup5,
    OpenGroup6,
    OpenGroup7,
    OpenGroup8,
    OpenGroup9,
};

/// Groups with a shortcut of their own (OpenGroup1 … OpenGroup9).
inline constexpr int GroupShortcutCount = 9;

/// A key with modifiers: MOD_ALT/MOD_CONTROL/MOD_SHIFT/MOD_WIN bits and a virtual key.
struct KeyCombo
{
    unsigned modifiers = 0;
    unsigned vk = 0;

    bool isValid() const
    {
        return vk != 0;
    }
    friend bool operator==(const KeyCombo &a, const KeyCombo &b)
    {
        return a.modifiers == b.modifiers && a.vk == b.vk;
    }
};

struct ShortcutDefinition
{
    ShortcutAction action;
    /// Same id as the KDE action (QAction::objectName there), also the settings key.
    const char *id;
    const char *text;
    /// Held down, the key repeats the action (panning); otherwise repeats are ignored.
    bool repeats;
    QList<KeyCombo> defaults;
};

/// All shortcuts, in the order of the KDE table; defaults as there with Meta = Win.
const QList<ShortcutDefinition> &shortcutDefinitions();
const ShortcutDefinition *findShortcut(ShortcutAction action);

/// "Win+Alt+H", "Ctrl+Shift+Left", "Win+Alt+=" … Order of the modifiers does not matter,
/// case does not matter. Returns nullopt for anything it does not understand.
std::optional<KeyCombo> parseKeyCombo(const QString &text);
QString formatKeyCombo(const KeyCombo &combo);
/// Several combos separated by ';'. Invalid entries are skipped.
QList<KeyCombo> parseKeyCombos(const QString &text);
QString formatKeyCombos(const QList<KeyCombo> &combos);

/// The bound shortcuts: defaults, overridden by group [Shortcuts] of the settings file
/// (key = action id, value = combos; an empty value unbinds the action).
class ShortcutMap
{
public:
    static ShortcutMap defaults();
    static ShortcutMap load(QSettings &settings);

    std::optional<ShortcutAction> actionFor(const KeyCombo &combo) const;
    QList<KeyCombo> combosFor(ShortcutAction action) const;

private:
    struct Binding
    {
        ShortcutAction action;
        KeyCombo combo;
    };
    QList<Binding> m_bindings;
};

/// True for the virtual keys of Shift, Ctrl, Alt and Win (left, right and generic).
bool isModifierKey(unsigned vk);

} // namespace Wid
