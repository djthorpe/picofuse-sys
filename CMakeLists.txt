cmake_minimum_required(VERSION 3.20)

# Logic if building for a Pico board
if(DEFINED PICO_BOARD)
    # Test programs should make a static library, not an executable
    set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

    # Point to the shared boards header directory at the root of the objc
    # repository before including pico_sdk_init
    set(PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}/include/boards")

    # Load the Pico SDK toolchain before project() selects compilers.
    include(third_party/pico-sdk/pico_sdk_init.cmake)
endif()

project(picofuse C CXX ASM)

include(GNUInstallDirs)

option(PICOFUSE_USB "Enable USB support across all platforms" ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

if(DEFINED PICO_BOARD)
    pico_sdk_init()
endif()

message(STATUS "PICOFUSE_USB: ${PICOFUSE_USB}")

# Lowercase and uppercase system name
# SYSTEM_NAME_PICO // SYSTEM_NAME=pico
# SYSTEM_NAME_DARWIN // SYSTEM_NAME=darwin
# SYSTEM_NAME_LINUX // SYSTEM_NAME=linux
string(TOLOWER ${CMAKE_SYSTEM_NAME} SYSTEM_NAME_LOWER)
string(TOUPPER ${CMAKE_SYSTEM_NAME} SYSTEM_NAME_UPPER)
string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} PROCESSOR_NAME_LOWER)

if(DEFINED PICO_BOARD)
    string(TOLOWER "${PICO_BOARD}" PICOFUSE_BOARD_LOWER)
    string(REGEX REPLACE "[^a-z0-9_]+" "-" PICOFUSE_BOARD_LOWER "${PICOFUSE_BOARD_LOWER}")
    set(PICOFUSE_LIBRARY_BASENAME "picofuse-${PICOFUSE_BOARD_LOWER}")
    set(PICOFUSE_PC_FILE_BASENAME "picofuse-${PICOFUSE_BOARD_LOWER}")
else()
    if(DEFINED PICOFUSE_BOARD)
        set(PICOFUSE_BOARD_VALUE "${PICOFUSE_BOARD}")
    else()
        set(PICOFUSE_BOARD_VALUE "generic")
    endif()

    string(TOLOWER "${PICOFUSE_BOARD_VALUE}" PICOFUSE_BOARD_LOWER)
    string(REGEX REPLACE "[^a-z0-9_]+" "-" PICOFUSE_BOARD_LOWER "${PICOFUSE_BOARD_LOWER}")
    set(PICOFUSE_LIBRARY_BASENAME "picofuse-${SYSTEM_NAME_LOWER}-${PROCESSOR_NAME_LOWER}-${PICOFUSE_BOARD_LOWER}")
    set(PICOFUSE_PC_FILE_BASENAME "picofuse")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(PICOFUSE_LIBRARY_BASENAME "${PICOFUSE_LIBRARY_BASENAME}-debug")
    set(PICOFUSE_PC_FILE_BASENAME "${PICOFUSE_PC_FILE_BASENAME}-debug")
endif()

set(PICOFUSE_PC_FILE_NAME "${PICOFUSE_PC_FILE_BASENAME}.pc")

if(SYSTEM_NAME_LOWER STREQUAL "darwin")
    enable_language(OBJC)
endif()

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

if(DEFINED PROGRAM_VERSION)
    add_compile_options("-DPROGRAM_VERSION=\"${PROGRAM_VERSION}\"")
endif()

if(DEFINED PICO_BOARD)
    # Pico SDK host/stdio combinations can miss stdbool.h in disabled branches.
    add_compile_options("-include" "stdbool.h")
endif()

# Add source code 
add_subdirectory(src)

# Add sys examples
add_subdirectory(examples/sys/multicore)
add_subdirectory(examples/sys/queue)
add_subdirectory(examples/sys/runloop)

# Add Pico SDK-only repro examples
add_subdirectory(examples/picosdk)

# Add hid examples
add_subdirectory(examples/hid/runloop)

# Add hw examples
add_subdirectory(examples/hw/gpio)
add_subdirectory(examples/hw/uart)
add_subdirectory(examples/hw/i2c-scan)
add_subdirectory(examples/hw/pwm-fade)
add_subdirectory(examples/hw/led-blink)
add_subdirectory(examples/hw/infrared-rx)
add_subdirectory(examples/hw/watchdog)
add_subdirectory(examples/hw/usb-scan)
add_subdirectory(examples/hw/wifi-scan)
add_subdirectory(examples/hw/wifi-join)

# Add dev examples
add_subdirectory(examples/dev/bme680)
add_subdirectory(examples/dev/picoinky)
add_subdirectory(examples/dev/tca9555)

if(DEFINED PICO_BOARD AND PICO_BOARD STREQUAL "presto")
    add_subdirectory(examples/dev/presto-touch)
    add_subdirectory(examples/dev/presto-display)
endif()

# Tests
include(CTest)
add_subdirectory("test")
