set(NAME "picofuse-sys")

add_library(${NAME} STATIC
    ../any/arena.c
    ../any/runloop.c
    ../any/debugf.c
    ../any/date.c
    ../any/djb2.c
    ../any/event.c
    ../any/mem.c
    ../any/memcmp.c
    cond.c
    env.c
    date.c
    hash.c
    halt.c
    init.c
    memcpy.c
    memset.c
    mutex.c
    panicf.c
    ../printf/float.c
    ../printf/printf.c
    random.c
    sleep.c
    stdio.c
    strlen.c
    thread.c
    timer.c
    timestamp.c
    waitgroup.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(${NAME} PUBLIC
    pico_bootrom
    pico_mbedtls
    pico_multicore
    pico_rand
    pico_sync
    pico_stdlib
    pico_standard_link
    hardware_sync
)

picofuse_add_module(${NAME})
