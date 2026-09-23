cmake_minimum_required(VERSION 3.16)
project(hace-corba C)

option(HACE_BUILD_TESTS "Build unit tests" ON)

enable_testing()

add_subdirectory(runtime)
