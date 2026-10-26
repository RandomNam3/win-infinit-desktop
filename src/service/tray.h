/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "win32util.h"

#include <functional>
#include <string>
#include <vector>

namespace Wid
{

/**
 * The icon in the notification area (Shell_NotifyIcon) with its menu. Comes back by itself
 * when Explorer restarts (TaskbarCreated).
 */
class Tray
{
public:
    enum class Action {
        TogglePause,
        BringWindowsBack,
        Settings,
        LeaveSafeMode,
        Quit,
        NewNote,
        ToggleNotes,
        ShowMap,
        GroupFromSelection,
        AreaFromSelection,
        OpenGroup, ///< index: the group's position (its shortcut slot)
    };
    struct State
    {
        bool paused = false;
        bool safeMode = false;
        std::wstring status; ///< first, disabled menu line
    };
    using Handler = std::function<void(Action, int index)>;
    /// Names of the groups for the "Open group" submenu, asked when the menu opens.
    using GroupNames = std::function<std::vector<std::wstring>()>;

    explicit Tray(Handler handler);
    ~Tray();
    Tray(const Tray &) = delete;
    Tray &operator=(const Tray &) = delete;

    bool show();
    void setGroupNames(GroupNames names)
    {
        m_groupNames = std::move(names);
    }
    void setState(const State &state);
    /// A balloon notification (shown by Windows as a toast).
    void notify(const std::wstring &title, const std::wstring &message);

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT message, WPARAM wParam, LPARAM lParam);
    bool addIcon();
    void showMenu();
    void updateTip();

    Handler m_handler;
    GroupNames m_groupNames;
    UniqueWindow m_window;
    UniqueIcon m_icon;
    State m_state;
    UINT m_taskbarCreated = 0;
    bool m_added = false;
};

} // namespace Wid
