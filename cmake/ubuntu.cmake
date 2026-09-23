# Toolchain file for native builds on Ubuntu Linux.
#
# This is a native (non-cross) build, so it mostly pins compiler standard
# and warning flags rather than compiler paths. It exists as a named
# toolchain file so the build invocation is uniform across targets
# (compare cmake/integrity178.cmake).

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(HACE_TARGET_PLATFORM "linux" CACHE STRING "hace-corba target platform")

set(HACE_C_WARNING_FLAGS
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes
    -Wmissing-prototypes
    CACHE STRING "Warning flags applied to hace-corba C targets")
