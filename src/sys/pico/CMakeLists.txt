set(NAME "picofuse-sys-obj")

add_library(${NAME} OBJECT
    halt.c
    init.c
    mem.c
    panicf.c
    printf.c
    sleep.c
    timestamp.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(${NAME} PUBLIC
    pico_stdlib
)
