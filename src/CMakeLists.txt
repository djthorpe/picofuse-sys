set(NAME "picofuse-sys")

add_subdirectory(hw/${SYSTEM_NAME_LOWER})
add_subdirectory(sys/${SYSTEM_NAME_LOWER})
add_subdirectory(dev)
add_subdirectory(hid)

add_library(${NAME} STATIC)

set_target_properties(${NAME} PROPERTIES
    OUTPUT_NAME "${PICOFUSE_LIBRARY_BASENAME}"
)

target_sources(${NAME} PRIVATE
    $<TARGET_OBJECTS:picofuse-hw-obj>
    $<TARGET_OBJECTS:picofuse-sys-obj>
    $<TARGET_OBJECTS:picofuse-dev-obj>
    $<TARGET_OBJECTS:picofuse-hid-obj>
)

target_link_libraries(${NAME} PUBLIC
    picofuse-hw-obj
    picofuse-sys-obj
    picofuse-dev-obj
    picofuse-hid-obj
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

if(DEFINED PROGRAM_VERSION)
    set(PICOFUSE_PC_VERSION "${PROGRAM_VERSION}")
else()
    set(PICOFUSE_PC_VERSION "0.0.0")
endif()

set(PICOFUSE_PC_LIB_NAME "${PICOFUSE_LIBRARY_BASENAME}")

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

configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/picofuse-sys.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PICOFUSE_PC_FILE_NAME}
    @ONLY
)

install(TARGETS ${NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/picofuse/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/picofuse
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PICOFUSE_PC_FILE_NAME}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

install(FILES ${PROJECT_SOURCE_DIR}/cmake/picofuse.cmake
    DESTINATION ${CMAKE_INSTALL_DATADIR}/picofuse/cmake
)

if(DEFINED PICO_BOARD)
    # Install board linker/startup assets into share/picofuse/<board>.
    # Allow board overlays to override these paths if needed.
    set(PICOFUSE_BOARD_ASSET_DIR "${CMAKE_INSTALL_DATADIR}/picofuse/${PICOFUSE_BOARD_LOWER}")

    if(DEFINED PICOFUSE_LDSCRIPT AND EXISTS "${PICOFUSE_LDSCRIPT}")
        set(PICOFUSE_FLASH_LD_SOURCE "${PICOFUSE_LDSCRIPT}")
    elseif(DEFINED PICO_SDK_PATH AND DEFINED PICO_CHIP AND EXISTS "${PICO_SDK_PATH}/src/rp2_common/pico_crt0/${PICO_CHIP}/memmap_default.ld")
        set(PICOFUSE_FLASH_LD_SOURCE "${PICO_SDK_PATH}/src/rp2_common/pico_crt0/${PICO_CHIP}/memmap_default.ld")
    endif()

    if(DEFINED PICOFUSE_STARTUP AND EXISTS "${PICOFUSE_STARTUP}")
        set(PICOFUSE_START_S_SOURCE "${PICOFUSE_STARTUP}")
    elseif(DEFINED PICO_SDK_PATH AND EXISTS "${PICO_SDK_PATH}/src/rp2_common/pico_crt0/crt0.S")
        set(PICOFUSE_START_S_SOURCE "${PICO_SDK_PATH}/src/rp2_common/pico_crt0/crt0.S")
    endif()

    if(DEFINED PICOFUSE_FLASH_LD_SOURCE)
        install(FILES "${PICOFUSE_FLASH_LD_SOURCE}"
            DESTINATION "${PICOFUSE_BOARD_ASSET_DIR}"
            RENAME "flash.ld"
        )

        install(FILES "${CMAKE_BINARY_DIR}/pico_flash_region.ld"
            DESTINATION "${PICOFUSE_BOARD_ASSET_DIR}"
            OPTIONAL
        )
    else()
        message(WARNING "Could not resolve flash linker script source for board '${PICO_BOARD}'")
    endif()

    if(DEFINED PICOFUSE_START_S_SOURCE)
        install(FILES "${PICOFUSE_START_S_SOURCE}"
            DESTINATION "${PICOFUSE_BOARD_ASSET_DIR}"
            RENAME "start.s"
        )
    else()
        message(WARNING "Could not resolve startup source file for board '${PICO_BOARD}'")
    endif()

    install(FILES "${CMAKE_BINARY_DIR}/bin/picofuse-example-hw-infrared-rx.elf.map"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/picofuse/${PICOFUSE_BOARD_LOWER}/maps"
        RENAME "${PICOFUSE_BOARD_LOWER}.elf.map"
        OPTIONAL
    )
endif()
