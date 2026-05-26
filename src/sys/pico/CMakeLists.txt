set(NAME "picofuse-sys-obj")

add_library(${NAME} OBJECT
    cond.c
    hash.c
    halt.c
    init.c
    mem.c
    mutex.c
    panicf.c
    ../printf/float.c
    ../printf/printf.c
    random.c
    sleep.c
    stdio.c
    thread.c
    timestamp.c
    waitgroup.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(${NAME} PUBLIC
    pico_mbedtls
    pico_multicore
    pico_rand
    pico_stdlib
)
