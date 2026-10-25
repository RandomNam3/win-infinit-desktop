# SPDX-License-Identifier: GPL-3.0-or-later
#
# wid-media: real input and screenshots for the runtime tests and the documentation
# images. No Qt.

add_executable(wid-media main.cpp)
target_link_libraries(wid-media PRIVATE user32 gdi32 ole32 windowscodecs)
if(MINGW)
    target_link_options(wid-media PRIVATE -municode -static)
endif()
