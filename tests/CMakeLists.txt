# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_AUTOMOC ON)

foreach(name camera motion velocitytracker parking navigation placementmemory)
    add_executable(test_${name} test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE infinitdesktop_core Qt6::Test)
    add_test(NAME test_${name} COMMAND test_${name})
endforeach()

if(WIN32)
    # Unit tests need no window system.
    set_tests_properties(test_camera test_motion test_velocitytracker test_parking test_navigation test_placementmemory
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
