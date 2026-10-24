# SPDX-License-Identifier: GPL-3.0-or-later
#
# win-infinit-desktop.exe, the canvas service (Qt Gui without widgets and QML), and a
# static library of its parts for the unit tests.

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Gui Network)

add_library(wid_service STATIC
    canvasservice.cpp
    crashguard.cpp
    inputrouter.cpp
    ipcserver.cpp
    layered.cpp
    logging.cpp
    markerlayout.cpp
    markers.cpp
    overview.cpp
    settings.cpp
    shortcuts.cpp
    texts.cpp
    tray.cpp
    win32util.cpp
)
set_target_properties(wid_service PROPERTIES AUTOMOC ON)
target_include_directories(wid_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wid_service PUBLIC infinitdesktop_core wid_platform Qt6::Gui Qt6::Network)

# Version numbers for the resource file.
set(WID_VERSION_RC "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)

add_executable(win-infinit-desktop WIN32 main.cpp service.rc)
if(MSVC)
    target_sources(win-infinit-desktop PRIVATE win-infinit-desktop.manifest)
endif()
# version.h and the icon for service.rc
target_include_directories(win-infinit-desktop PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/data)
target_link_libraries(win-infinit-desktop PRIVATE wid_service)
install(TARGETS win-infinit-desktop RUNTIME DESTINATION .)
