add_executable(test_cdr test_cdr.c)
target_link_libraries(test_cdr PRIVATE hace_corba_core)
target_include_directories(test_cdr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME test_cdr COMMAND test_cdr)

if(HACE_TARGET_PLATFORM STREQUAL "linux")
    add_executable(test_hal test_hal.c)
    target_link_libraries(test_hal PRIVATE hace_corba_core)
    target_include_directories(test_hal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    add_test(NAME test_hal COMMAND test_hal)
    # Guards against a regression hanging forever (e.g. the SO_RCVTIMEO
    # "0 means block forever" pitfall this test suite was written to
    # catch) instead of silently stalling CI.
    set_tests_properties(test_hal PROPERTIES TIMEOUT 30)
endif()
