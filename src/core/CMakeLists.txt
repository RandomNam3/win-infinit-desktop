# SPDX-License-Identifier: GPL-3.0-or-later
#
# Pure canvas logic, taken unchanged from KDE Infinit Desktop (src/core, see
# docs/ARCHITEKTUR.md, section 16): camera, animations, velocity tracking, parking spots, navigation,
# window placement memory, groups, areas, note placements and the map. Only Qt Core,
# everything is unit tested.

add_library(infinitdesktop_core STATIC
    areas.cpp
    camera.cpp
    groups.cpp
    minimap.cpp
    motion.cpp
    navigation.cpp
    noteplacements.cpp
    parking.cpp
    placementmemory.cpp
    velocitytracker.cpp
)
target_include_directories(infinitdesktop_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(infinitdesktop_core PUBLIC Qt6::Core)
