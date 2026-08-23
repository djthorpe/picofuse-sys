set(NAME "picofuse-sys-obj")

find_package(OpenSSL REQUIRED)

add_library(${NAME} OBJECT
    ../any/arena.c
    ../any/runloop.c
    ../any/debugf.c
    ../any/date.c
    ../any/djb2.c
    ../any/event.c
    ../any/mem.c
    ../openssl/hash.c
    ../openssl/random.c
    env.c
    memcmp.c
    memcpy.c
    memset.c
    timer.c
    ../posix/date.c
    ../posix/init.c
    ../posix/signal.c
    ../posix/halt.c
    ../posix/panicf.c
    ../printf/float.c
    ../printf/printf.c
    ../posix/stdio.c
    ../posix/sleep.c
    ../posix/strlen.c
    ../posix/timestamp.c
    ../pthread/cond.c
    ../pthread/mutex.c
    ../pthread/thread.c
    ../pthread/waitgroup.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${NAME} PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
)
