# Finish configuring a picofuse module static library target: sets its
# OUTPUT_NAME, generates + installs its pkg-config file, and installs the
# library itself. Call once per module (picofuse-sys, picofuse-hw, ...)
# after add_library(<NAME> STATIC ...) and its own target_link_libraries
# calls.
#
#   picofuse_add_module(<NAME> [REQUIRES <picofuse-module>...])
#
# REQUIRES lists this module's own picofuse-* PUBLIC dependencies (e.g.
# picofuse-hid REQUIRES picofuse-hw), used for the pkg-config Requires:
# field. Expects PICOFUSE_LIBRARY_SUFFIX and the pkg-config template
# variables (PICOFUSE_PC_VERSION, PICOFUSE_PC_PREFIX_REL, ...) to already be
# set by the top-level CMakeLists.txt before any module subdirectory calling
# this is added.
function(picofuse_add_module NAME)
    cmake_parse_arguments(_ARG "" "" "REQUIRES" ${ARGN})

    string(REPLACE "picofuse-" "" _MODULE "${NAME}")
    set(_LIBNAME "picofuse-${_MODULE}-${PICOFUSE_LIBRARY_SUFFIX}")

    set_target_properties(${NAME} PROPERTIES
        OUTPUT_NAME "${_LIBNAME}"
    )

    set(PICOFUSE_PC_NAME "picofuse-${_MODULE}-${PICOFUSE_LIBRARY_SUFFIX}")
    set(PICOFUSE_PC_LIB_NAME "${_LIBNAME}")
    set(PICOFUSE_PC_REQUIRES "")
    foreach(_REQ ${_ARG_REQUIRES})
        string(REPLACE "picofuse-" "" _REQ_MODULE "${_REQ}")
        string(APPEND PICOFUSE_PC_REQUIRES " picofuse-${_REQ_MODULE}-${PICOFUSE_LIBRARY_SUFFIX}")
    endforeach()

    set(_PC_FILE_NAME "picofuse-${_MODULE}-${PICOFUSE_LIBRARY_SUFFIX}.pc")
    configure_file(
        ${PROJECT_SOURCE_DIR}/cmake/picofuse-module.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/${_PC_FILE_NAME}
        @ONLY
    )

    install(TARGETS ${NAME}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${_PC_FILE_NAME}
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
endfunction()

# Shared build logic for Raspberry Pi microcontrollers without Pico SDK path
# dependencies.
#
# Supports two modes:
# - Compatibility mode: picofuse_add_executable(<existing_target>)
# - Full mode:
#   picofuse_add_executable(
#     NAME <target>
#     C <file.c>...
#     ASM <file.s>...
#     LDSCRIPT <linker-script.ld>
#     BOOT2 <boot2.o|boot2.s|boot2.S|relative-path>
#   )
#
# Optional caller-provided variables for external/installed builds:
# - PICOFUSE_INCLUDE_DIRS: additional include roots passed as -I
# - PICOFUSE_LIBRARY_DIRS: additional library roots passed as -L
# - PICOFUSE_LIBRARIES: libraries or link inputs appended to the final link
# - PICOFUSE_LINK_OPTIONS: extra raw linker/compiler flags appended as-is

function(_picofuse_init_defaults)
    if(DEFINED PICOFUSE__DEFAULTS_READY)
        return()
    endif()

    # Installed layout is expected as:
    #   <prefix>/share/picofuse/cmake/picofuse.cmake
    #   <prefix>/share/picofuse/<board>/flash.ld
    #   <prefix>/share/picofuse/<board>/start.s
    get_filename_component(_PICOFUSE_MODULE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

    if(NOT DEFINED PICOFUSE_BOARD AND DEFINED PICOFUSE_BOARD_LOWER)
        set(PICOFUSE_BOARD "${PICOFUSE_BOARD_LOWER}" PARENT_SCOPE)
        set(PICOFUSE_BOARD "${PICOFUSE_BOARD_LOWER}")
    endif()

    if(NOT DEFINED PICOFUSE_CHIP_DIR)
        if(DEFINED PICOFUSE_FAMILY)
            set(PICOFUSE_CHIP_DIR "${PICOFUSE_FAMILY}" PARENT_SCOPE)
            set(PICOFUSE_CHIP_DIR "${PICOFUSE_FAMILY}")
        else()
            set(PICOFUSE_CHIP_DIR "rp2040" PARENT_SCOPE)
            set(PICOFUSE_CHIP_DIR "rp2040")
        endif()
    endif()

    if(NOT DEFINED PICOFUSE_FAMILY)
        set(PICOFUSE_FAMILY "${PICOFUSE_CHIP_DIR}" PARENT_SCOPE)
        set(PICOFUSE_FAMILY "${PICOFUSE_CHIP_DIR}")
    endif()

    if(NOT DEFINED PICOFUSE_CPU)
        if(DEFINED CMAKE_SYSTEM_PROCESSOR)
            set(PICOFUSE_CPU "${CMAKE_SYSTEM_PROCESSOR}" PARENT_SCOPE)
            set(PICOFUSE_CPU "${CMAKE_SYSTEM_PROCESSOR}")
        elseif(PICOFUSE_CHIP_DIR STREQUAL "rp2350")
            set(PICOFUSE_CPU "cortex-m33" PARENT_SCOPE)
            set(PICOFUSE_CPU "cortex-m33")
        else()
            set(PICOFUSE_CPU "cortex-m0plus" PARENT_SCOPE)
            set(PICOFUSE_CPU "cortex-m0plus")
        endif()
    endif()

    if(NOT DEFINED PICOFUSE_ARMGNU AND DEFINED CMAKE_C_COMPILER)
        get_filename_component(_PICOFUSE_CC_NAME "${CMAKE_C_COMPILER}" NAME)
        if(_PICOFUSE_CC_NAME MATCHES "^([A-Za-z0-9_+.-]+)-gcc$")
            set(PICOFUSE_ARMGNU "${CMAKE_MATCH_1}" PARENT_SCOPE)
            set(PICOFUSE_ARMGNU "${CMAKE_MATCH_1}")
        endif()
    endif()

    if(NOT DEFINED PICOFUSE_SRAM_ENTRY)
        set(PICOFUSE_SRAM_ENTRY "0x20000000" PARENT_SCOPE)
        set(PICOFUSE_SRAM_ENTRY "0x20000000")
    endif()
    if(NOT DEFINED PICOFUSE_SRAM_LOAD)
        set(PICOFUSE_SRAM_LOAD "0x20000000" PARENT_SCOPE)
        set(PICOFUSE_SRAM_LOAD "0x20000000")
    endif()
    if(NOT DEFINED PICOFUSE_FLASH_LOAD)
        set(PICOFUSE_FLASH_LOAD "0x10000000" PARENT_SCOPE)
        set(PICOFUSE_FLASH_LOAD "0x10000000")
    endif()
    if(NOT DEFINED PICOFUSE_FLASH_ENTRY)
        if(PICOFUSE_CHIP_DIR STREQUAL "rp2040")
            set(PICOFUSE_FLASH_ENTRY "0x10000100" PARENT_SCOPE)
            set(PICOFUSE_FLASH_ENTRY "0x10000100")
        else()
            set(PICOFUSE_FLASH_ENTRY "0x10000000" PARENT_SCOPE)
            set(PICOFUSE_FLASH_ENTRY "0x10000000")
        endif()
    endif()

    if(DEFINED PICOFUSE_BOARD)
        set(_PICOFUSE_BOARD_DIR "${_PICOFUSE_MODULE_ROOT}/${PICOFUSE_BOARD}")

        if(NOT DEFINED PICOFUSE_LDSCRIPT AND EXISTS "${_PICOFUSE_BOARD_DIR}/flash.ld")
            set(PICOFUSE_LDSCRIPT "${_PICOFUSE_BOARD_DIR}/flash.ld" PARENT_SCOPE)
            set(PICOFUSE_LDSCRIPT "${_PICOFUSE_BOARD_DIR}/flash.ld")
        endif()

        if(NOT DEFINED PICOFUSE_STARTUP AND EXISTS "${_PICOFUSE_BOARD_DIR}/start.s")
            set(PICOFUSE_STARTUP "${_PICOFUSE_BOARD_DIR}/start.s" PARENT_SCOPE)
            set(PICOFUSE_STARTUP "${_PICOFUSE_BOARD_DIR}/start.s")
        endif()

        if(NOT DEFINED PICOFUSE_BOOT2_OBJECT AND EXISTS "${_PICOFUSE_BOARD_DIR}/boot2.o")
            set(PICOFUSE_BOOT2_OBJECT "${_PICOFUSE_BOARD_DIR}/boot2.o" PARENT_SCOPE)
            set(PICOFUSE_BOOT2_OBJECT "${_PICOFUSE_BOARD_DIR}/boot2.o")
        endif()
    endif()

    set(PICOFUSE__DEFAULTS_READY TRUE PARENT_SCOPE)
    set(PICOFUSE__DEFAULTS_READY TRUE)
endfunction()

function(_picofuse_init_toolchain)
    if(DEFINED PICOFUSE__TOOLCHAIN_READY)
        return()
    endif()

    _picofuse_init_defaults()

    if(NOT DEFINED PICOFUSE_ARMGNU)
        message(FATAL_ERROR "picofuse_add_executable: PICOFUSE_ARMGNU is not set and could not be inferred")
    endif()

    find_program(PICOFUSE_AS ${PICOFUSE_ARMGNU}-as REQUIRED)
    find_program(PICOFUSE_GCC ${PICOFUSE_ARMGNU}-gcc REQUIRED)
    find_program(PICOFUSE_LD ${PICOFUSE_ARMGNU}-gcc REQUIRED)
    find_program(PICOFUSE_OBJDUMP ${PICOFUSE_ARMGNU}-objdump REQUIRED)
    find_program(PICOFUSE_OBJCOPY ${PICOFUSE_ARMGNU}-objcopy REQUIRED)
    find_program(PICOFUSE_PICOTOOL picotool REQUIRED)

    set(PICOFUSE__TOOLCHAIN_READY TRUE PARENT_SCOPE)
    set(PICOFUSE__TOOLCHAIN_READY TRUE)
endfunction()

function(_picofuse_resolve_boot2 BOOT2_ARG OUTPUT_VAR)
    if(NOT BOOT2_ARG)
        set(${OUTPUT_VAR} "" PARENT_SCOPE)
        return()
    endif()

    if(IS_ABSOLUTE "${BOOT2_ARG}")
        set(_BOOT2_SOURCE "${BOOT2_ARG}")
    else()
        set(_BOOT2_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${BOOT2_ARG}")
    endif()

    if(NOT EXISTS "${_BOOT2_SOURCE}")
        message(FATAL_ERROR "picofuse_add_executable: BOOT2 file not found: ${_BOOT2_SOURCE}")
    endif()

    get_filename_component(_BOOT2_EXT "${_BOOT2_SOURCE}" EXT)
    if(_BOOT2_EXT STREQUAL ".o")
        set(${OUTPUT_VAR} "${_BOOT2_SOURCE}" PARENT_SCOPE)
        return()
    endif()

    if(NOT _BOOT2_EXT STREQUAL ".s" AND NOT _BOOT2_EXT STREQUAL ".S")
        message(FATAL_ERROR "picofuse_add_executable: BOOT2 must be .o, .s, or .S")
    endif()

    get_filename_component(_BOOT2_BASE "${_BOOT2_SOURCE}" NAME_WE)
    set(_BOOT2_OBJ "${CMAKE_CURRENT_BINARY_DIR}/boot2/${_BOOT2_BASE}.o")
    add_custom_command(
        OUTPUT ${_BOOT2_OBJ}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/boot2"
        COMMAND ${PICOFUSE_GCC} -x assembler-with-cpp -c --warn --fatal-warnings -mcpu=${PICOFUSE_CPU} -mthumb -g "${_BOOT2_SOURCE}" -o "${_BOOT2_OBJ}"
        DEPENDS "${_BOOT2_SOURCE}"
        COMMENT "AS boot2 ${_BOOT2_SOURCE}"
    )

    set(${OUTPUT_VAR} "${_BOOT2_OBJ}" PARENT_SCOPE)
endfunction()

function(picofuse_add_executable)
    # Compatibility mode: add map output to an existing target.
    if(ARGC EQUAL 1)
        set(_TARGET "${ARGV0}")
        if(NOT TARGET ${_TARGET})
            message(FATAL_ERROR "picofuse_add_executable: unknown target '${_TARGET}'")
        endif()
        if(NOT DEFINED PICOFUSE_BOARD AND DEFINED PICOFUSE_BOARD_LOWER)
            set(PICOFUSE_BOARD "${PICOFUSE_BOARD_LOWER}")
        endif()
        if(NOT DEFINED PICOFUSE_BOARD)
            return()
        endif()
        target_link_options(${_TARGET} PRIVATE
            "LINKER:-Map=$<TARGET_FILE_DIR:${_TARGET}>/${PICOFUSE_BOARD}-$<TARGET_FILE_BASE_NAME:${_TARGET}>.elf.map"
        )
        return()
    endif()

    cmake_parse_arguments(_ARG "" "NAME;LDSCRIPT;BOOT2" "ASM;C" ${ARGN})

    if(NOT _ARG_NAME)
        message(FATAL_ERROR "picofuse_add_executable: NAME is required")
    endif()
    if(NOT _ARG_C AND NOT _ARG_ASM)
        message(FATAL_ERROR "picofuse_add_executable: at least one C or ASM source is required")
    endif()

    _picofuse_init_toolchain()

    set(_PICOFUSE_AFLAGS -Wa,--warn,--fatal-warnings -mcpu=${PICOFUSE_CPU} -g)
    if(DEFINED PICOFUSE_INCLUDE_DIRS)
        foreach(_INCLUDE_DIR ${PICOFUSE_INCLUDE_DIRS})
            list(APPEND _PICOFUSE_AFLAGS -I${_INCLUDE_DIR})
        endforeach()
    endif()
    set(_PICOFUSE_CFLAGS
        -mcpu=${PICOFUSE_CPU}
        -mthumb
        -nostartfiles
        -ffreestanding
        -g
        -O0
        -I${CMAKE_SOURCE_DIR}/include
    )
    if(DEFINED PICOFUSE_INCLUDE_DIRS)
        foreach(_INCLUDE_DIR ${PICOFUSE_INCLUDE_DIRS})
            list(APPEND _PICOFUSE_CFLAGS -I${_INCLUDE_DIR})
        endforeach()
    endif()

    if(DEFINED PICOFUSE_LED_PIN)
        list(APPEND _PICOFUSE_CFLAGS -DLED_PIN=${PICOFUSE_LED_PIN})
    endif()
    if(DEFINED PICOFUSE_USER_SW_PIN)
        list(APPEND _PICOFUSE_CFLAGS -DUSER_SW_PIN=${PICOFUSE_USER_SW_PIN})
    endif()
    if(DEFINED PICOFUSE_XOSC_HZ)
        list(APPEND _PICOFUSE_CFLAGS -DPICOFUSE_XOSC_HZ=${PICOFUSE_XOSC_HZ})
    endif()
    if(DEFINED PICOFUSE_SYS_CLK_HZ)
        list(APPEND _PICOFUSE_CFLAGS -DPICOFUSE_SYS_CLK_HZ=${PICOFUSE_SYS_CLK_HZ})
    endif()
    if(DEFINED PICOFUSE_IO_BANK0_IRQ)
        list(APPEND _PICOFUSE_CFLAGS -DPICOFUSE_IO_BANK0_IRQ=${PICOFUSE_IO_BANK0_IRQ})
    endif()
    if(DEFINED PICOFUSE_NUM_GPIOS)
        list(APPEND _PICOFUSE_CFLAGS -DPICOFUSE_NUM_GPIOS=${PICOFUSE_NUM_GPIOS})
    endif()

    set(_PICOFUSE_LDFLAGS -mcpu=${PICOFUSE_CPU} -mthumb -nostdlib)
    if(DEFINED PICOFUSE_LIBRARY_DIRS)
        foreach(_LIBRARY_DIR ${PICOFUSE_LIBRARY_DIRS})
            list(APPEND _PICOFUSE_LDFLAGS -L${_LIBRARY_DIR})
        endforeach()
    endif()
    if(DEFINED PICOFUSE_LINK_OPTIONS)
        list(APPEND _PICOFUSE_LDFLAGS ${PICOFUSE_LINK_OPTIONS})
    endif()

    # LDSCRIPT: explicit (relative to caller) > configured default.
    if(_ARG_LDSCRIPT)
        set(_LDSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/${_ARG_LDSCRIPT}")
    elseif(DEFINED PICOFUSE_LDSCRIPT)
        set(_LDSCRIPT "${PICOFUSE_LDSCRIPT}")
    else()
        message(FATAL_ERROR "picofuse_add_executable: LDSCRIPT is required (or set PICOFUSE_LDSCRIPT)")
    endif()

    # BOOT2: explicit > configured default object/path.
    if(NOT _ARG_BOOT2 AND DEFINED PICOFUSE_BOOT2_OBJECT)
        set(_ARG_BOOT2 ${PICOFUSE_BOOT2_OBJECT})
    endif()

    set(_NAME ${_ARG_NAME})
    set(_OBJECTS)

    if(_ARG_BOOT2)
        _picofuse_resolve_boot2(${_ARG_BOOT2} _BOOT2_OBJ)
        list(APPEND _OBJECTS "${_BOOT2_OBJ}")
        set(_ENTRY ${PICOFUSE_FLASH_ENTRY})
        set(_LOAD_ADDR ${PICOFUSE_FLASH_LOAD})
    else()
        set(_ENTRY ${PICOFUSE_SRAM_ENTRY})
        set(_LOAD_ADDR ${PICOFUSE_SRAM_LOAD})
    endif()

    # Board startup is prepended when provided.
    if(DEFINED PICOFUSE_STARTUP)
        get_filename_component(_STARTUP_BASE ${PICOFUSE_STARTUP} NAME_WE)
        set(_STARTUP_OBJ ${CMAKE_CURRENT_BINARY_DIR}/${_STARTUP_BASE}.o)
        add_custom_command(
            OUTPUT ${_STARTUP_OBJ}
            COMMAND ${PICOFUSE_GCC} -x assembler-with-cpp -c ${_PICOFUSE_AFLAGS} -mthumb ${PICOFUSE_STARTUP} -o ${_STARTUP_OBJ}
            DEPENDS ${PICOFUSE_STARTUP}
            COMMENT "AS (startup) ${_STARTUP_BASE}.s"
        )
        list(APPEND _OBJECTS ${_STARTUP_OBJ})
    endif()

    foreach(_SRC ${_ARG_ASM})
        get_filename_component(_BASE ${_SRC} NAME_WE)
        set(_OBJ ${CMAKE_CURRENT_BINARY_DIR}/${_BASE}.o)
        add_custom_command(
            OUTPUT ${_OBJ}
            COMMAND ${PICOFUSE_GCC} -x assembler-with-cpp -c ${_PICOFUSE_AFLAGS} -mthumb ${CMAKE_CURRENT_SOURCE_DIR}/${_SRC} -o ${_OBJ}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_SRC}
            COMMENT "AS ${_SRC}"
        )
        list(APPEND _OBJECTS ${_OBJ})
    endforeach()

    foreach(_SRC ${PICOFUSE_CHIP_SOURCES})
        get_filename_component(_BASE ${_SRC} NAME_WE)
        set(_OBJ ${CMAKE_CURRENT_BINARY_DIR}/chip/${_BASE}.o)
        add_custom_command(
            OUTPUT ${_OBJ}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/chip"
            COMMAND ${PICOFUSE_GCC} ${_PICOFUSE_CFLAGS} -c ${_SRC} -o ${_OBJ}
            DEPENDS ${_SRC}
            COMMENT "CC (chip) ${_BASE}.c"
        )
        list(APPEND _OBJECTS ${_OBJ})
    endforeach()

    foreach(_SRC ${_ARG_C})
        get_filename_component(_BASE ${_SRC} NAME_WE)
        set(_OBJ ${CMAKE_CURRENT_BINARY_DIR}/${_BASE}.o)
        add_custom_command(
            OUTPUT ${_OBJ}
            COMMAND ${PICOFUSE_GCC} ${_PICOFUSE_CFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/${_SRC} -o ${_OBJ}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_SRC}
            COMMENT "CC ${_SRC}"
        )
        list(APPEND _OBJECTS ${_OBJ})
    endforeach()

    set(_ELF ${CMAKE_CURRENT_BINARY_DIR}/${_NAME}.elf)
    set(_LIST ${CMAKE_CURRENT_BINARY_DIR}/${_NAME}.list)
    set(_BIN ${CMAKE_CURRENT_BINARY_DIR}/${_NAME}.bin)
    set(_UF2 ${CMAKE_CURRENT_BINARY_DIR}/${_NAME}.uf2)

    add_custom_command(
        OUTPUT ${_ELF}
        COMMAND ${PICOFUSE_LD} ${_PICOFUSE_LDFLAGS}
                -Wl,--entry=${_ENTRY} -T ${_LDSCRIPT}
                ${_OBJECTS} ${PICOFUSE_LIBRARIES} -lc -lm -lgcc -lnosys -o ${_ELF}
        DEPENDS ${_OBJECTS} ${_LDSCRIPT}
        COMMENT "LD ${_NAME}.elf"
    )

    add_custom_command(
        OUTPUT ${_LIST}
        COMMAND ${PICOFUSE_OBJDUMP} -D ${_ELF} > ${_LIST}
        DEPENDS ${_ELF}
        COMMENT "OBJDUMP ${_NAME}.list"
    )

    add_custom_command(
        OUTPUT ${_BIN}
        COMMAND ${PICOFUSE_OBJCOPY} -O binary ${_ELF} ${_BIN}
        DEPENDS ${_ELF}
        COMMENT "OBJCOPY ${_NAME}.bin"
    )

    add_custom_command(
        OUTPUT ${_UF2}
        COMMAND ${PICOFUSE_PICOTOOL} uf2 convert ${_BIN} ${_UF2}
                -o ${_LOAD_ADDR} --family ${PICOFUSE_FAMILY}
        DEPENDS ${_BIN}
        COMMENT "UF2 ${_NAME}.uf2"
    )

    add_custom_target(${_NAME} ALL DEPENDS ${_UF2} ${_LIST})
endfunction()
