# SPDX-License-Identifier: GPL-3.0-or-later
#
# wid-ctl: command line client of the canvas service (scripts, tests, debugging).

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Network)
add_executable(wid-ctl main.cpp ${PROJECT_SOURCE_DIR}/src/service/ipcserver.cpp)
set_target_properties(wid-ctl PROPERTIES AUTOMOC ON)
target_include_directories(wid-ctl PRIVATE ${PROJECT_SOURCE_DIR}/src/service ${PROJECT_SOURCE_DIR}/src/shared)
target_link_libraries(wid-ctl PRIVATE Qt6::Network)
install(TARGETS wid-ctl RUNTIME DESTINATION .)
