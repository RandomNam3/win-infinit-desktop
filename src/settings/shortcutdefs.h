/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "i18n.h"

#include <QKeySequence>
#include <QList>

namespace InfinitDesktop
{

/// Component name of the plugin's own shortcuts in the list of all shortcuts.
inline constexpr const char *ShortcutComponent = "win-infinit-desktop";
inline constexpr const char *ShortcutComponentDisplayName = "WIN Infinit Desktop";

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
    // Notes, groups, areas and the map
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

/// Number of groups that have an "open group" shortcut (OpenGroup1 …).
inline constexpr int GroupShortcutCount = 9;

struct ShortcutDefinition
{
    ShortcutAction action;
    /// Stable action id, the key in [Shortcuts] of settings.ini; never translate or change it.
    const char *id;
    KLazyLocalizedString text;
    KLazyLocalizedString description;
    /// Group id used by the settings UIs ("zoom", "pan", "focus", "bookmarks", "windows", "organize", "groups").
    const char *group;
    QList<QKeySequence> defaultShortcuts;
};

/// All shortcuts of the plugin, in display order.
const QList<ShortcutDefinition> &shortcutDefinitions();

const ShortcutDefinition *findShortcut(ShortcutAction action);

} // namespace InfinitDesktop
