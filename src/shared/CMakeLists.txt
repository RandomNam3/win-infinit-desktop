# SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Shared by the notes app and the settings app: the pipe protocol (client and server),
# translations from the .po catalog, and the paths of the user's files.

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Network Qml)

add_library(wid_appshared STATIC
    apppaths.h
    i18n.h
    ipcclient.cpp
    ipcclient.h
    jsonlineserver.cpp
    jsonlineserver.h
    legacy.h
    localizer.cpp
    localizer.h
    menutheme.h
    pipenames.h
)
set_target_properties(wid_appshared PROPERTIES AUTOMOC ON)
target_include_directories(wid_appshared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wid_appshared PUBLIC Qt6::Core Qt6::Network Qt6::Qml)

# Compiles the catalogs into `target` as :/i18n/<language>.po.
function(wid_add_catalogs target)
    set(catalog "${PROJECT_SOURCE_DIR}/po/de/win-infinite-desktop.po")
    set_source_files_properties("${catalog}" PROPERTIES QT_RESOURCE_ALIAS "de.po")
    qt_add_resources(${target} "${target}_catalogs"
        PREFIX "/i18n"
        FILES "${catalog}"
    )
endfunction()

# Icon and version information of a Windows program.
function(wid_add_windows_resources target name description)
    if(NOT WIN32)
        return()
    endif()
    set(WID_RC_NAME "${name}")
    set(WID_RC_DESCRIPTION "${description}")
    set(WID_VERSION_RC "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
    set(WID_ICON_FILE "${PROJECT_SOURCE_DIR}/data/win-infinite-desktop.ico")
    configure_file("${PROJECT_SOURCE_DIR}/src/shared/app.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/${name}.rc" @ONLY)
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${name}.rc")
endfunction()
