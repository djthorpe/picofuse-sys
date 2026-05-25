set(NAME "picofuse-sys-obj")

add_library(${NAME} OBJECT
    cond.c
    halt.c
    init.c
    mem.c
    mutex.c
    panicf.c
    printf.c
    random.c
    sleep.c
    thread.c
    timestamp.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(${NAME} PUBLIC
    pico_multicore
    pico_rand
    pico_stdlib
)
