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

    target_link_libraries(${NAME} PRIVATE
        picofuse-sys
    )

    add_test(NAME ${NAME} COMMAND $<TARGET_FILE:${NAME}>)
endmacro()