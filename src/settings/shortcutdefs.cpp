/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "shortcutdefs.h"


namespace InfiniteDesktop
{

namespace
{
// Meta is the Windows key. The defaults are those of the service (shortcuts.cpp there).
constexpr auto MetaAlt = Qt::META | Qt::ALT;
constexpr auto MetaAltShift = Qt::META | Qt::ALT | Qt::SHIFT;
constexpr auto MetaCtrlAlt = Qt::META | Qt::CTRL | Qt::ALT;
}

const QList<ShortcutDefinition> &shortcutDefinitions()
{
    static const QList<ShortcutDefinition> definitions{
        {ShortcutAction::ZoomIn, "InfiniteDesktop ZoomIn", kli18n("Infinite Desktop: Zoom In"),
         kli18n("Zoom into the canvas around the mouse pointer."), "zoom",
         {QKeySequence(MetaAlt | Qt::Key_Equal), QKeySequence(QKeyCombination(Qt::MetaModifier | Qt::AltModifier | Qt::KeypadModifier, Qt::Key_Plus))}},
        {ShortcutAction::ZoomOut, "InfiniteDesktop ZoomOut", kli18n("Infinite Desktop: Zoom Out"),
         kli18n("Zoom out of the canvas around the mouse pointer."), "zoom",
         {QKeySequence(MetaAlt | Qt::Key_Minus), QKeySequence(QKeyCombination(Qt::MetaModifier | Qt::AltModifier | Qt::KeypadModifier, Qt::Key_Minus))}},
        {ShortcutAction::ZoomReset, "InfiniteDesktop ZoomReset", kli18n("Infinite Desktop: Zoom to 100 %"),
         kli18n("Return to 100 % zoom so that windows can be used directly again."), "zoom",
         {QKeySequence(MetaAlt | Qt::Key_0)}},
        {ShortcutAction::Overview, "InfiniteDesktop Overview", kli18n("Infinite Desktop: Show All Windows"),
         kli18n("Zoom out until every window of this screen is visible. Press again to go back."), "zoom",
         {QKeySequence(MetaAlt | Qt::Key_W)}},
        {ShortcutAction::Home, "InfiniteDesktop Home", kli18n("Infinite Desktop: Go Home"),
         kli18n("Jump to the origin of the canvas. Press again to return to where you were."), "pan",
         {QKeySequence(MetaAlt | Qt::Key_H)}},
        {ShortcutAction::CenterWindow, "InfiniteDesktop CenterWindow", kli18n("Infinite Desktop: Center Active Window"),
         kli18n("Move the canvas so that the active window is in the middle of the screen."), "focus",
         {QKeySequence(MetaAlt | Qt::Key_C)}},
        {ShortcutAction::PanLeft, "InfiniteDesktop PanLeft", kli18n("Infinite Desktop: Pan Left"),
         kli18n("Move the view to the left."), "pan", {QKeySequence(MetaAltShift | Qt::Key_Left)}},
        {ShortcutAction::PanRight, "InfiniteDesktop PanRight", kli18n("Infinite Desktop: Pan Right"),
         kli18n("Move the view to the right."), "pan", {QKeySequence(MetaAltShift | Qt::Key_Right)}},
        {ShortcutAction::PanUp, "InfiniteDesktop PanUp", kli18n("Infinite Desktop: Pan Up"),
         kli18n("Move the view up."), "pan", {QKeySequence(MetaAltShift | Qt::Key_Up)}},
        {ShortcutAction::PanDown, "InfiniteDesktop PanDown", kli18n("Infinite Desktop: Pan Down"),
         kli18n("Move the view down."), "pan", {QKeySequence(MetaAltShift | Qt::Key_Down)}},
        {ShortcutAction::FocusLeft, "InfiniteDesktop FocusLeft", kli18n("Infinite Desktop: Focus Window to the Left"),
         kli18n("Activate the nearest window to the left on the canvas and bring it into view."), "focus",
         {QKeySequence(MetaCtrlAlt | Qt::Key_Left)}},
        {ShortcutAction::FocusRight, "InfiniteDesktop FocusRight", kli18n("Infinite Desktop: Focus Window to the Right"),
         kli18n("Activate the nearest window to the right on the canvas and bring it into view."), "focus",
         {QKeySequence(MetaCtrlAlt | Qt::Key_Right)}},
        {ShortcutAction::FocusUp, "InfiniteDesktop FocusUp", kli18n("Infinite Desktop: Focus Window Above"),
         kli18n("Activate the nearest window above on the canvas and bring it into view."), "focus",
         {QKeySequence(MetaCtrlAlt | Qt::Key_Up)}},
        {ShortcutAction::FocusDown, "InfiniteDesktop FocusDown", kli18n("Infinite Desktop: Focus Window Below"),
         kli18n("Activate the nearest window below on the canvas and bring it into view."), "focus",
         {QKeySequence(MetaCtrlAlt | Qt::Key_Down)}},
        {ShortcutAction::GotoBookmark1, "InfiniteDesktop GotoBookmark1", kli18n("Infinite Desktop: Go to Bookmark 1"),
         kli18n("Jump to the canvas position saved as bookmark 1."), "bookmarks", {QKeySequence(MetaAlt | Qt::Key_1)}},
        {ShortcutAction::GotoBookmark2, "InfiniteDesktop GotoBookmark2", kli18n("Infinite Desktop: Go to Bookmark 2"),
         kli18n("Jump to the canvas position saved as bookmark 2."), "bookmarks", {QKeySequence(MetaAlt | Qt::Key_2)}},
        {ShortcutAction::GotoBookmark3, "InfiniteDesktop GotoBookmark3", kli18n("Infinite Desktop: Go to Bookmark 3"),
         kli18n("Jump to the canvas position saved as bookmark 3."), "bookmarks", {QKeySequence(MetaAlt | Qt::Key_3)}},
        {ShortcutAction::GotoBookmark4, "InfiniteDesktop GotoBookmark4", kli18n("Infinite Desktop: Go to Bookmark 4"),
         kli18n("Jump to the canvas position saved as bookmark 4."), "bookmarks", {QKeySequence(MetaAlt | Qt::Key_4)}},
        {ShortcutAction::SaveBookmark1, "InfiniteDesktop SaveBookmark1", kli18n("Infinite Desktop: Save Bookmark 1"),
         kli18n("Remember the current position and zoom as bookmark 1."), "bookmarks", {QKeySequence(MetaCtrlAlt | Qt::Key_1)}},
        {ShortcutAction::SaveBookmark2, "InfiniteDesktop SaveBookmark2", kli18n("Infinite Desktop: Save Bookmark 2"),
         kli18n("Remember the current position and zoom as bookmark 2."), "bookmarks", {QKeySequence(MetaCtrlAlt | Qt::Key_2)}},
        {ShortcutAction::SaveBookmark3, "InfiniteDesktop SaveBookmark3", kli18n("Infinite Desktop: Save Bookmark 3"),
         kli18n("Remember the current position and zoom as bookmark 3."), "bookmarks", {QKeySequence(MetaCtrlAlt | Qt::Key_3)}},
        {ShortcutAction::SaveBookmark4, "InfiniteDesktop SaveBookmark4", kli18n("Infinite Desktop: Save Bookmark 4"),
         kli18n("Remember the current position and zoom as bookmark 4."), "bookmarks", {QKeySequence(MetaCtrlAlt | Qt::Key_4)}},
        {ShortcutAction::TogglePin, "InfiniteDesktop TogglePin", kli18n("Infinite Desktop: Pin Window to Screen"),
         kli18n("Pin the active window to the screen so it does not move with the canvas, or release it again."), "windows",
         {QKeySequence(MetaAlt | Qt::Key_T)}},
        {ShortcutAction::TogglePinAbove, "InfiniteDesktop TogglePinAbove", kli18n("Infinite Desktop: Pin Window to Screen, Always on Top"),
         kli18n("Pin the active window to the screen and keep it above all other windows, or release it again."), "windows",
         {QKeySequence(MetaAltShift | Qt::Key_T)}},
        {ShortcutAction::ToggleEnabled, "InfiniteDesktop ToggleEnabled", kli18n("Infinite Desktop: Pause / Resume"),
         kli18n("Temporarily turn off the mouse and touchpad gestures of the canvas, so applications receive them. Shortcuts keep working."), "windows",
         {}},
        {ShortcutAction::NewNote, "InfiniteDesktop NewNote", kli18n("Infinite Desktop: New Note"),
         kli18n("Put a new note on the canvas at the mouse pointer."), "organize", {QKeySequence(MetaAlt | Qt::Key_N)}},
        {ShortcutAction::ToggleNotes, "InfiniteDesktop ToggleNotes", kli18n("Infinite Desktop: Show / Hide Notes"),
         kli18n("Hide all notes, or show them again."), "organize", {QKeySequence(MetaAltShift | Qt::Key_N)}},
        {ShortcutAction::ShowMap, "InfiniteDesktop ShowMap", kli18n("Infinite Desktop: Show Map"),
         kli18n("Show a map of this screen's canvas with its windows, groups and areas; click or drag on it to move the view."), "organize",
         {QKeySequence(MetaAlt | Qt::Key_M)}},
        {ShortcutAction::GroupFromSelection, "InfiniteDesktop GroupFromSelection", kli18n("Infinite Desktop: New Group from Selection"),
         kli18n("Draw a rectangle with the mouse: the windows inside become a new group."), "organize", {QKeySequence(MetaAlt | Qt::Key_G)}},
        {ShortcutAction::AreaFromSelection, "InfiniteDesktop AreaFromSelection", kli18n("Infinite Desktop: New Area from Selection"),
         kli18n("Draw a rectangle with the mouse: it becomes an area of the canvas that grows and shrinks with its windows."), "organize",
         {QKeySequence(MetaAlt | Qt::Key_B)}},
        {ShortcutAction::SaveGroupLayout, "InfiniteDesktop SaveGroupLayout", kli18n("Infinite Desktop: Save Group Arrangement"),
         kli18n("Remember how the windows of the active window's group are arranged now."), "organize",
         {QKeySequence(MetaAltShift | Qt::Key_G)}},
        {ShortcutAction::OpenGroup1, "InfiniteDesktop OpenGroup1", kli18n("Infinite Desktop: Open Group 1"),
         kli18n("Bring the windows of group 1 together at its place, start the missing ones, and jump there."), "groups",
         {QKeySequence(MetaAlt | Qt::Key_F1)}},
        {ShortcutAction::OpenGroup2, "InfiniteDesktop OpenGroup2", kli18n("Infinite Desktop: Open Group 2"),
         kli18n("Bring the windows of group 2 together at its place, start the missing ones, and jump there."), "groups",
         {QKeySequence(MetaAlt | Qt::Key_F2)}},
        {ShortcutAction::OpenGroup3, "InfiniteDesktop OpenGroup3", kli18n("Infinite Desktop: Open Group 3"),
         kli18n("Bring the windows of group 3 together at its place, start the missing ones, and jump there."), "groups",
         {QKeySequence(MetaAlt | Qt::Key_F3)}},
        {ShortcutAction::OpenGroup4, "InfiniteDesktop OpenGroup4", kli18n("Infinite Desktop: Open Group 4"),
         kli18n("Bring the windows of group 4 together at its place, start the missing ones, and jump there."), "groups",
         {QKeySequence(MetaAlt | Qt::Key_F4)}},
        {ShortcutAction::OpenGroup5, "InfiniteDesktop OpenGroup5", kli18n("Infinite Desktop: Open Group 5"),
         kli18n("Bring the windows of group 5 together at its place, start the missing ones, and jump there."), "groups",
         {QKeySequence(MetaAlt | Qt::Key_F5)}},
        {ShortcutAction::OpenGroup6, "InfiniteDesktop OpenGroup6", kli18n("Infinite Desktop: Open Group 6"),
         kli18n("Bring the windows of group 6 together at its place, start the missing ones, and jump there."), "groups",
         {QKeySequence(MetaAlt | Qt::Key_F6)}},
        {ShortcutAction::OpenGroup7, "InfiniteDesktop OpenGroup7", kli18n("Infinite Desktop: Open Group 7"),
         kli18n("Bring the windows of group 7 together at its place, start the missing ones, and jump there."), "groups",
         {QKeySequence(MetaAlt | Qt::Key_F7)}},
        {ShortcutAction::OpenGroup8, "InfiniteDesktop OpenGroup8", kli18n("Infinite Desktop: Open Group 8"),
         kli18n("Bring the windows of group 8 together at its place, start the missing ones, and jump there."), "groups",
         {QKeySequence(MetaAlt | Qt::Key_F8)}},
        {ShortcutAction::OpenGroup9, "InfiniteDesktop OpenGroup9", kli18n("Infinite Desktop: Open Group 9"),
         kli18n("Bring the windows of group 9 together at its place, start the missing ones, and jump there."), "groups",
         {QKeySequence(MetaAlt | Qt::Key_F9)}},
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

} // namespace InfiniteDesktop
