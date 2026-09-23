# Placeholder toolchain file for Green Hills INTEGRITY-178 tuMP.
#
# This cannot be completed without the licensed Green Hills MULTI
# toolchain (compiler/linker paths, target BSP flags, tuMP partition
# configuration) — see runtime/src/hal/integrity178/README.md for what's
# blocked and why. Configuring CMake with this file today stops with a
# clear error rather than silently misconfiguring a build.

message(FATAL_ERROR
    "cmake/integrity178.cmake is a placeholder: it needs real Green "
    "Hills MULTI compiler/linker paths and INTEGRITY-178 tuMP BSP "
    "settings, which require a licensed Green Hills SDK this project "
    "does not have access to. See "
    "runtime/src/hal/integrity178/README.md.")

set(CMAKE_SYSTEM_NAME Generic)
set(HACE_TARGET_PLATFORM "integrity178" CACHE STRING "hace-corba target platform")

# When GHS SDK access is available, fill in at minimum:
#   set(CMAKE_C_COMPILER <path to ccintarm/ccppc/etc.>)
#   set(CMAKE_C_COMPILER_TARGET <INTEGRITY-178 tuMP target triple>)
#   set(CMAKE_EXE_LINKER_FLAGS ...)   # tuMP partition/BSP link flags
# and remove the FATAL_ERROR above.
