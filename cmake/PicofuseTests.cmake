macro(picofuse_add_sys_test NAME)
    set(TEST_SOURCES)
    foreach(SOURCE_FILE ${ARGN})
        list(APPEND TEST_SOURCES "sys/${NAME}/${SOURCE_FILE}")
    endforeach()

    add_executable(${NAME}
        ${TEST_SOURCES}
    )

    target_compile_definitions(${NAME} PRIVATE
        PICOFUSE_TEST_NAME="${NAME}"
    )

    target_include_directories(${NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/test/include
    )

    target_link_libraries(${NAME} PRIVATE
        picofuse-sys
    )

    if(DEFINED PICO_BOARD)
        pico_enable_stdio_uart(${NAME} FALSE)
        pico_enable_stdio_usb(${NAME} TRUE)
        pico_add_extra_outputs(${NAME})
    endif()

    add_test(NAME ${NAME} COMMAND $<TARGET_FILE:${NAME}>)
endmacro()