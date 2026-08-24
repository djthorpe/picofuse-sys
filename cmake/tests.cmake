macro(picofuse_add_sys_test NAME)
    set(TEST_SOURCES)
    foreach(SOURCE_FILE ${ARGN})
        list(APPEND TEST_SOURCES "sys/${NAME}/${SOURCE_FILE}")
    endforeach()

    add_executable(${NAME}
        ${TEST_SOURCES}
    )

    target_include_directories(${NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/test/include
    )

    # test/include/test.h's TestMain() calls hw_init()/hw_exit()
    # unconditionally, so every test target needs picofuse-hw regardless of
    # category.
    target_link_libraries(${NAME} PRIVATE
        picofuse-hw
        picofuse-sys
    )

    if(DEFINED PICO_BOARD)
        pico_set_printf_implementation(${NAME} none)
        pico_enable_stdio_uart(${NAME} FALSE)
        pico_enable_stdio_usb(${NAME} TRUE)
        pico_add_extra_outputs(${NAME})
    endif()

    add_test(NAME ${NAME} COMMAND $<TARGET_FILE:${NAME}>)
endmacro()

macro(picofuse_add_fs_test NAME)
    set(TEST_SOURCES)
    foreach(SOURCE_FILE ${ARGN})
        list(APPEND TEST_SOURCES "fs/${NAME}/${SOURCE_FILE}")
    endforeach()

    add_executable(${NAME}
        ${TEST_SOURCES}
    )

    target_include_directories(${NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/test/include
    )

    # test/include/test.h's TestMain() calls hw_init()/hw_exit()
    # unconditionally; picofuse-fs only pulls in picofuse-hw itself on the
    # pico platform variant, so add it explicitly here too.
    target_link_libraries(${NAME} PRIVATE
        picofuse-fs
        picofuse-hw
        picofuse-sys
    )

    if(DEFINED PICO_BOARD)
        pico_set_printf_implementation(${NAME} none)
        pico_enable_stdio_uart(${NAME} FALSE)
        pico_enable_stdio_usb(${NAME} TRUE)
        pico_add_extra_outputs(${NAME})
    endif()

    add_test(NAME ${NAME} COMMAND $<TARGET_FILE:${NAME}>)
endmacro()

macro(picofuse_add_hw_test NAME)
    set(TEST_SOURCES)
    foreach(SOURCE_FILE ${ARGN})
        list(APPEND TEST_SOURCES "hw/${NAME}/${SOURCE_FILE}")
    endforeach()

    add_executable(${NAME}
        ${TEST_SOURCES}
    )

    target_include_directories(${NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/test/include
    )

    target_link_libraries(${NAME} PRIVATE
        picofuse-hw
        picofuse-sys
    )

    if(DEFINED PICO_BOARD)
        pico_set_printf_implementation(${NAME} none)
        pico_enable_stdio_uart(${NAME} FALSE)
        pico_enable_stdio_usb(${NAME} TRUE)
        pico_add_extra_outputs(${NAME})
    endif()

    add_test(NAME ${NAME} COMMAND $<TARGET_FILE:${NAME}>)
endmacro()

macro(picofuse_add_hid_test NAME)
    set(TEST_SOURCES)
    foreach(SOURCE_FILE ${ARGN})
        list(APPEND TEST_SOURCES "hid/${NAME}/${SOURCE_FILE}")
    endforeach()

    add_executable(${NAME}
        ${TEST_SOURCES}
    )

    target_include_directories(${NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/test/include
    )

    target_link_libraries(${NAME} PRIVATE
        picofuse-hid
        picofuse-sys
    )

    if(DEFINED PICO_BOARD)
        pico_set_printf_implementation(${NAME} none)
        pico_enable_stdio_uart(${NAME} FALSE)
        pico_enable_stdio_usb(${NAME} TRUE)
        pico_add_extra_outputs(${NAME})
    endif()

    add_test(NAME ${NAME} COMMAND $<TARGET_FILE:${NAME}>)
endmacro()