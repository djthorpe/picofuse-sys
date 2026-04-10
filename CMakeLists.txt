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

project(picofuse_sys C CXX ASM)

if(DEFINED PICO_BOARD)
    pico_sdk_init()
    add_subdirectory(src)
    add_subdirectory(examples/hw/gpio)
endif()
