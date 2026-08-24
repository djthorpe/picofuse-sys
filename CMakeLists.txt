cmake_minimum_required(VERSION 3.20)

# Logic if building for a Pico board
if(DEFINED PICO_BOARD)
    set(PICO_MBEDTLS_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/src/sys/pico/mbedtls_config.h")

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
if(EXISTS "${PROJECT_SOURCE_DIR}/cmake/picofuse.cmake")
    include(picofuse)
else()
    include(PicofusePico)
endif()

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

# PICOFUSE_LIBRARY_SUFFIX identifies this particular build (board, or
# system/processor/board for native builds), shared by every module's
# library OUTPUT_NAME and .pc filename: picofuse-<module>-${SUFFIX}[.pc].
if(DEFINED PICO_BOARD)
    string(TOLOWER "${PICO_BOARD}" PICOFUSE_BOARD_LOWER)
    string(REGEX REPLACE "[^a-z0-9_]+" "-" PICOFUSE_BOARD_LOWER "${PICOFUSE_BOARD_LOWER}")
    set(PICOFUSE_LIBRARY_SUFFIX "${PICOFUSE_BOARD_LOWER}")
else()
    if(DEFINED PICOFUSE_BOARD)
        set(PICOFUSE_BOARD_VALUE "${PICOFUSE_BOARD}")
    else()
        set(PICOFUSE_BOARD_VALUE "generic")
    endif()

    string(TOLOWER "${PICOFUSE_BOARD_VALUE}" PICOFUSE_BOARD_LOWER)
    string(REGEX REPLACE "[^a-z0-9_]+" "-" PICOFUSE_BOARD_LOWER "${PICOFUSE_BOARD_LOWER}")
    set(PICOFUSE_LIBRARY_SUFFIX "${SYSTEM_NAME_LOWER}-${PROCESSOR_NAME_LOWER}-${PICOFUSE_BOARD_LOWER}")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(PICOFUSE_LIBRARY_SUFFIX "${PICOFUSE_LIBRARY_SUFFIX}-debug")
endif()

if(DEFINED PROGRAM_VERSION)
    set(PICOFUSE_PC_VERSION "${PROGRAM_VERSION}")
else()
    set(PICOFUSE_PC_VERSION "0.0.0")
endif()

# Default pkg-config board metadata fields to empty for non-Pico builds.
set(PICOFUSE_ARMGNU "")
set(PICOFUSE_CPU "")
set(PICOFUSE_CHIP_DIR "")
set(PICOFUSE_FAMILY "")
set(PICOFUSE_SRAM_ENTRY "")
set(PICOFUSE_SRAM_LOAD "")
set(PICOFUSE_FLASH_ENTRY "")
set(PICOFUSE_FLASH_LOAD "")

if(DEFINED PICO_BOARD)
    # Toolchain prefix: prefer Pico SDK triple, then derive from compiler name.
    if(DEFINED PICO_DEFAULT_GCC_TRIPLE)
        set(PICOFUSE_ARMGNU "${PICO_DEFAULT_GCC_TRIPLE}")
    elseif(DEFINED CMAKE_C_COMPILER)
        get_filename_component(PICOFUSE_C_COMPILER_NAME "${CMAKE_C_COMPILER}" NAME)
        if(PICOFUSE_C_COMPILER_NAME MATCHES "^([A-Za-z0-9_+.-]+)-gcc$")
            set(PICOFUSE_ARMGNU "${CMAKE_MATCH_1}")
        endif()
    endif()

    if(DEFINED CMAKE_SYSTEM_PROCESSOR)
        set(PICOFUSE_CPU "${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    if(DEFINED PICO_CHIP)
        set(PICOFUSE_CHIP_DIR "${PICO_CHIP}")
        set(PICOFUSE_FAMILY "${PICO_CHIP}")
    endif()

    # Address defaults for post-build tooling; overridable from cache/CLI.
    if(PICOFUSE_SRAM_ENTRY STREQUAL "")
        set(PICOFUSE_SRAM_ENTRY "0x20000000")
    endif()
    if(PICOFUSE_SRAM_LOAD STREQUAL "")
        set(PICOFUSE_SRAM_LOAD "0x20000000")
    endif()
    if(PICOFUSE_FLASH_LOAD STREQUAL "")
        set(PICOFUSE_FLASH_LOAD "0x10000000")
    endif()
    if(PICOFUSE_FLASH_ENTRY STREQUAL "")
        if(DEFINED PICO_CHIP AND PICO_CHIP STREQUAL "rp2040")
            set(PICOFUSE_FLASH_ENTRY "0x10000100")
        else()
            set(PICOFUSE_FLASH_ENTRY "0x10000000")
        endif()
    endif()
endif()

# Resolve prefix relative to ${pcfiledir} for relocatable pkg-config files,
# including multi-component libdirs (e.g. lib/x86_64-linux-gnu).
set(PICOFUSE_PC_PREFIX_REL "..")
string(REPLACE "/" ";" PICOFUSE_INSTALL_LIBDIR_PARTS "${CMAKE_INSTALL_LIBDIR}")
foreach(PICOFUSE_LIBDIR_PART IN LISTS PICOFUSE_INSTALL_LIBDIR_PARTS)
    if(NOT PICOFUSE_LIBDIR_PART STREQUAL "")
        string(APPEND PICOFUSE_PC_PREFIX_REL "/..")
    endif()
endforeach()

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
add_subdirectory(examples/sys/helloworld)
add_subdirectory(examples/sys/multicore)
add_subdirectory(examples/sys/queue)
add_subdirectory(examples/sys/runloop)

# Add hid examples
add_subdirectory(examples/hid/runloop)
add_subdirectory(examples/hid/evdev)

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
add_subdirectory(examples/dev/framebuffer)

# Add app examples
add_subdirectory(examples/app)

# Add presto examples if building for the presto board
if(DEFINED PICO_BOARD AND PICO_BOARD STREQUAL "presto")
    add_subdirectory(examples/dev/presto-touch)
    add_subdirectory(examples/dev/presto-display)
endif()

# Tests
include(CTest)
add_subdirectory("test")
