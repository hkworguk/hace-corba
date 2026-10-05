if(NOT DEFINED HACE_TARGET_PLATFORM)
    set(HACE_TARGET_PLATFORM "linux")
endif()

set(HACE_CORE_SOURCES
    src/cdr/cdr.c
    src/giop/giop.c
    src/uiop/uiop.c
    src/hal/hal_pool.c
)

if(HACE_TARGET_PLATFORM STREQUAL "linux")
    list(APPEND HACE_CORE_SOURCES src/hal/linux/hal_linux.c)
elseif(HACE_TARGET_PLATFORM STREQUAL "integrity178")
    list(APPEND HACE_CORE_SOURCES src/hal/integrity178/hal_integrity178.c)
else()
    message(FATAL_ERROR "Unknown HACE_TARGET_PLATFORM: ${HACE_TARGET_PLATFORM}")
endif()

add_library(hace_corba_core STATIC ${HACE_CORE_SOURCES})

target_include_directories(hace_corba_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(DEFINED HACE_C_WARNING_FLAGS)
    target_compile_options(hace_corba_core PRIVATE ${HACE_C_WARNING_FLAGS})
endif()

if(HACE_TARGET_PLATFORM STREQUAL "linux")
    find_package(Threads REQUIRED)
    target_link_libraries(hace_corba_core PUBLIC Threads::Threads)
endif()

if(HACE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
