# SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# wid-settings.exe: the settings app with the Qt Quick UI of KDE Infinite Desktop
# (plain QtQuick + QtQuick.Controls.Basic). QML, icons, the settings schema and the
# translations are compiled into the executable.

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Gui Network Qml Quick QuickControls2)

# The parts without QML, also used by the unit tests.
add_library(wid_settings_core STATIC
    keycombo.cpp
    keycombo.h
    searchutil.cpp
    searchutil.h
    settingsfile.cpp
    settingsfile.h
    settingsmeta.cpp
    settingsmeta.h
    settingsschema.cpp
    settingsschema.h
    serviceconnection.cpp
    serviceconnection.h
    shortcutdefs.cpp
    shortcutdefs.h
)
set_target_properties(wid_settings_core PROPERTIES AUTOMOC ON)
target_include_directories(wid_settings_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wid_settings_core PUBLIC wid_appshared Qt6::Core Qt6::Gui Qt6::Network)
qt_add_resources(wid_settings_core "wid_settings_schema"
    PREFIX "/settings"
    FILES win-infinite-desktop.kcfg
)

set(model_sources
    applicationindex.cpp
    applicationindex.h
    effectstatus.cpp
    effectstatus.h
    globalshortcutsmodel.cpp
    globalshortcutsmodel.h
    settingsmodel.cpp
    settingsmodel.h
    shortcutmodel.cpp
    shortcutmodel.h
    themecontroller.cpp
    themecontroller.h
    updatechecker.cpp
    updatechecker.h
    windowplacements.cpp
    windowplacements.h
    workspacemodel.cpp
    workspacemodel.h
)

if(Qt6_VERSION VERSION_LESS 6.5)
    # loadFromModule() and the module layout of the QML need Qt 6.5; the CI builds the app
    # with 6.8. Older Qt (Ubuntu 24.04) only compiles the models, as a check.
    message(STATUS "Qt ${Qt6_VERSION}: the settings app needs Qt 6.5 or newer, only its models are compiled")
    add_library(wid_settings_models OBJECT ${model_sources})
    set_target_properties(wid_settings_models PROPERTIES AUTOMOC ON)
    target_link_libraries(wid_settings_models PRIVATE wid_settings_core Qt6::Qml Qt6::Quick)
    return()
endif()

# Module resources under :/qt/qml/<URI>.
qt_policy(SET QTP0001 NEW)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

add_executable(wid-settings WIN32)

target_sources(wid-settings PRIVATE
    main.cpp
    iconprovider.cpp
    iconprovider.h
    screenshotrunner.cpp
    screenshotrunner.h
)
if(WIN32)
    target_sources(wid-settings PRIVATE
        applicationindex_win.cpp
        shellitem_win.cpp
        shellitem_win.h
    )
    target_link_libraries(wid-settings PRIVATE ole32 shell32 uuid gdi32 dwmapi)
endif()
set_target_properties(wid-settings PROPERTIES AUTOMOC ON)

set(qml_files
    AllShortcutRow.qml
    AllShortcutsView.qml
    AppIcon.qml
    AppListEditor.qml
    AppPicker.qml
    CanvasIllustration.qml
    Card.qml
    CategoryPage.qml
    ColorControl.qml
    ConfirmDialog.qml
    ConflictBanner.qml
    Dropdown.qml
    FocusRing.qml
    Icon.qml
    IconButton.qml
    InfoButton.qml
    Keycap.qml
    Keycaps.qml
    KeyCapture.qml
    Main.qml
    NavItem.qml
    OverviewPage.qml
    PageHeader.qml
    PageScaffold.qml
    Popover.qml
    RememberedWindowsCard.qml
    SearchField.qml
    SearchPage.qml
    SectionLabel.qml
    SegmentedControl.qml
    SettingRow.qml
    Shadow.qml
    ShortcutRow.qml
    ShortcutsPage.qml
    Sidebar.qml
    SliderControl.qml
    StatusDot.qml
    StatusTexts.qml
    Stepper.qml
    Theme.qml
    Toast.qml
    ToggleSwitch.qml
    UiButton.qml
    UiScrollBar.qml
    UiText.qml
    UiTextField.qml
    UiToolTip.qml
    UpdatesCard.qml
    WorkspaceCards.qml
)
set(qml_paths)
foreach(file IN LISTS qml_files)
    # Place every file next to the module's qmldir so they share one implicit import.
    set_source_files_properties(qml/${file} PROPERTIES QT_RESOURCE_ALIAS ${file})
    list(APPEND qml_paths qml/${file})
endforeach()
set_source_files_properties(qml/Theme.qml qml/StatusTexts.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(wid-settings
    URI InfiniteDesktop.Settings
    VERSION 1.0
    QML_FILES ${qml_paths}
    SOURCES ${model_sources}
    DEPENDENCIES
        QtQuick
)

file(GLOB icon_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS icons/*.svg)
qt_add_resources(wid-settings "infinitedesktop_icons"
    PREFIX "/infinitedesktop"
    FILES ${icon_files}
)
wid_add_catalogs(wid-settings)
wid_add_windows_resources(wid-settings wid-settings "WIN Infinite Desktop Settings")

target_link_libraries(wid-settings PRIVATE
    wid_settings_core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

install(TARGETS wid-settings RUNTIME DESTINATION .)
