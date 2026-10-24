# SPDX-License-Identifier: GPL-3.0-or-later
#
# wid-probe: a console tool that reports what the Windows interfaces the canvas relies
# on do on the machine it runs on. No Qt, a single executable.

add_executable(wid-probe main.cpp)
target_link_libraries(wid-probe PRIVATE wid_platform)
if(MINGW)
    target_link_options(wid-probe PRIVATE -municode -static)
endif()
install(TARGETS wid-probe RUNTIME DESTINATION .)
