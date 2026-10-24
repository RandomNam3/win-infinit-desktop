# SPDX-License-Identifier: GPL-3.0-or-later
#
# wid-testwindow: a plain window for the runtime smoke test. No Qt.

add_executable(wid-testwindow main.cpp)
target_link_libraries(wid-testwindow PRIVATE user32 gdi32)
if(MINGW)
    target_link_options(wid-testwindow PRIVATE -municode -static)
endif()
