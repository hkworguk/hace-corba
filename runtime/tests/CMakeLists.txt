add_executable(test_cdr test_cdr.c)
target_link_libraries(test_cdr PRIVATE hace_corba_core)
target_include_directories(test_cdr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME test_cdr COMMAND test_cdr)

add_executable(test_giop test_giop.c)
target_link_libraries(test_giop PRIVATE hace_corba_core)
target_include_directories(test_giop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME test_giop COMMAND test_giop)

if(HACE_TARGET_PLATFORM STREQUAL "linux")
    add_executable(test_hal test_hal.c)
    target_link_libraries(test_hal PRIVATE hace_corba_core)
    target_include_directories(test_hal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    add_test(NAME test_hal COMMAND test_hal)
    # Guards against a regression hanging forever (e.g. the SO_RCVTIMEO
    # "0 means block forever" pitfall this test suite was written to
    # catch) instead of silently stalling CI.
    set_tests_properties(test_hal PROPERTIES TIMEOUT 30)

    add_executable(test_uiop test_uiop.c)
    target_link_libraries(test_uiop PRIVATE hace_corba_core)
    target_include_directories(test_uiop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    add_test(NAME test_uiop COMMAND test_uiop)
    set_tests_properties(test_uiop PROPERTIES TIMEOUT 30)
endif()
