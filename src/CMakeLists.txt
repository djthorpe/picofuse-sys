add_subdirectory(fs/${SYSTEM_NAME_LOWER})
add_subdirectory(hw/${SYSTEM_NAME_LOWER})
add_subdirectory(sys/${SYSTEM_NAME_LOWER})
add_subdirectory(dev/${SYSTEM_NAME_LOWER})
add_subdirectory(hid)
add_subdirectory(pix)
add_subdirectory(app)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/picofuse/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/picofuse
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
