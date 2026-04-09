cmake_minimum_required(VERSION 3.20)

project(picofuse_sys C CXX ASM)

# Logic if building for a Pico board
if(DEFINED PICO_BOARD)
    # Point to the shared boards header directory at the root of the objc
    # repository before including pico_sdk_init
    set(PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include/boards")

    # Include Pico SDK modules (only when targeting a Pico board)
    include(third_party/pico-sdk/pico_sdk_init.cmake)

    # Test programs should make a static library, not an executable
    set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

    pico_sdk_init()

    add_subdirectory(src/hw/pico)
endif()