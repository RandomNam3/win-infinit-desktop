# SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# wid-notes.exe: notes on the canvas (Qt Quick, no widgets). Started by the canvas service
# when it needs it; talks to it over the service's pipe and serves its own.

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Gui Qml Quick QuickControls2)

# The note store on its own, for the unit tests.
add_library(wid_notestore STATIC notestore.cpp notestore.h)
set_target_properties(wid_notestore PROPERTIES AUTOMOC ON)
target_include_directories(wid_notestore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wid_notestore PUBLIC wid_appshared Qt6::Core)

if(Qt6_VERSION VERSION_LESS 6.5)
    # loadFromModule() needs Qt 6.5; older Qt (Ubuntu 24.04) only compiles the code as a check.
    message(STATUS "Qt ${Qt6_VERSION}: the notes app needs Qt 6.5 or newer, its code is only compiled")
    add_library(wid_notes_check OBJECT companion.cpp companion.h)
    set_target_properties(wid_notes_check PROPERTIES AUTOMOC ON)
    target_link_libraries(wid_notes_check PRIVATE wid_appshared wid_notestore Qt6::Gui Qt6::Qml Qt6::Quick)
    return()
endif()

qt_policy(SET QTP0001 NEW)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

add_executable(wid-notes WIN32
    main.cpp
    companion.cpp
    companion.h
)
set_target_properties(wid-notes PROPERTIES AUTOMOC ON)

qt_add_qml_module(wid-notes
    URI InfinitDesktop.Notes
    VERSION 1.0
    QML_FILES qml/NoteWindow.qml
    DEPENDENCIES QtQuick
)
qt_add_resources(wid-notes "wid_notes_icons"
    PREFIX "/notes"
    BASE "${PROJECT_SOURCE_DIR}/src/settings"
    FILES "${PROJECT_SOURCE_DIR}/src/settings/icons/note.svg"
)
wid_add_catalogs(wid-notes)
wid_add_windows_resources(wid-notes wid-notes "WIN Infinit Desktop Notes")

target_link_libraries(wid-notes PRIVATE
    wid_appshared
    wid_notestore
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)
install(TARGETS wid-notes RUNTIME DESTINATION .)
