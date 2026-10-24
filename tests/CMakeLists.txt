# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_AUTOMOC ON)

set(core_tests camera motion velocitytracker parking navigation placementmemory groups areas)
foreach(name IN LISTS core_tests)
    add_executable(test_${name} test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE infinitdesktop_core Qt6::Test)
    add_test(NAME test_${name} COMMAND test_${name})
endforeach()

if(WIN32)
    # The pure parts of the canvas service.
    set(service_tests shortcuts inputrouter service)
    foreach(name IN LISTS service_tests)
        add_executable(test_${name} test_${name}.cpp)
        target_link_libraries(test_${name} PRIVATE wid_service Qt6::Test)
        add_test(NAME test_${name} COMMAND test_${name})
    endforeach()
    # Unit tests need no window system.
    foreach(name IN LISTS core_tests service_tests)
        set_tests_properties(test_${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endforeach()
endif()
