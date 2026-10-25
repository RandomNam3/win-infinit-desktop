# SPDX-License-Identifier: GPL-3.0-or-later
#
# The Win32 layer: monitors, top-level windows, window events, low-level input hooks,
# touchpad contacts, virtual desktops, fullscreen detection and moving windows.
# Plain C++ and the Windows SDK, no Qt.

add_library(wid_platform STATIC
    fullscreen.cpp
    inputhook.cpp
    monitors.cpp
    osversion.cpp
    toplevel.cpp
    touchpad.cpp
    virtualdesktops.cpp
    windowevents.cpp
    windowmover.cpp
)
target_include_directories(wid_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wid_platform PUBLIC advapi32 dwmapi hid ole32 shcore shell32 user32)
