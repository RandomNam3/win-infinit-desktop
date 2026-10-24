# SPDX-License-Identifier: GPL-3.0-or-later
#
# Pure canvas logic, taken unchanged from KDE Infinit Desktop (src/core, see
# UPSTREAM.md): camera, animations, velocity tracking, parking spots, navigation
# and window placement memory. Only Qt Core, everything is unit tested.

add_library(infinitdesktop_core STATIC
    camera.cpp
    motion.cpp
    navigation.cpp
    parking.cpp
    placementmemory.cpp
    velocitytracker.cpp
)
target_include_directories(infinitdesktop_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(infinitdesktop_core PUBLIC Qt6::Core)
