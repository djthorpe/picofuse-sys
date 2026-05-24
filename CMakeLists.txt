cmake_minimum_required(VERSION 3.20)

# Logic if building for a Pico board
if(DEFINED PICO_BOARD)
    # Test programs should make a static library, not an executable
    set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

    # Point to the shared boards header directory at the root of the objc
    # repository before including pico_sdk_init
    set(PICO_BOARD_HEADER_DIRS "${PROJECT_SOURCE_DIR}/include/boards")

    # Load the Pico SDK toolchain before project() selects compilers.
    include(third_party/pico-sdk/pico_sdk_init.cmake)
endif()

project(picofuse C CXX ASM)

if(DEFINED PICO_BOARD)
    pico_sdk_init()
endif()

# Lowercase and uppercase system name
# SYSTEM_NAME_PICO // SYSTEM_NAME=pico
# SYSTEM_NAME_DARWIN // SYSTEM_NAME=darwin
# SYSTEM_NAME_LINUX // SYSTEM_NAME=linux
string(TOLOWER ${CMAKE_SYSTEM_NAME} SYSTEM_NAME_LOWER)
string(TOUPPER ${CMAKE_SYSTEM_NAME} SYSTEM_NAME_UPPER)
string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} PROCESSOR_NAME_LOWER)

# Compile options
add_compile_options(
    "-Wall"
    "-Wextra"
    "$<$<CONFIG:Debug>:-DDEBUG>"
    "$<$<CONFIG:Release>:-O3>"
    "-DSYSTEM_NAME_${SYSTEM_NAME_UPPER}"
    "-DSYSTEM_NAME=\"${SYSTEM_NAME_LOWER}\""
    "-DSYSTEM_PROCESSOR=\"${PROCESSOR_NAME_LOWER}\""
)

# Add source code and examples
add_subdirectory(src)
add_subdirectory(examples/hw/gpio)
add_subdirectory(examples/hw/uart)
add_subdirectory(examples/hw/wifi)

# Tests
include(CTest)
add_subdirectory("test")
