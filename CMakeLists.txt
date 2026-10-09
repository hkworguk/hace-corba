cmake_minimum_required(VERSION 3.16)
project(hace-corba C)

option(HACE_BUILD_TESTS "Build unit tests" ON)
option(HACE_BUILD_EXAMPLES "Build example programs" ON)

enable_testing()

add_subdirectory(runtime)

if(HACE_BUILD_EXAMPLES)
    add_subdirectory(examples/c)
endif()
