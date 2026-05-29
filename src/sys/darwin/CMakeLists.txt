set(NAME "picofuse-sys-obj")

find_package(OpenSSL REQUIRED)

add_library(${NAME} OBJECT
    ../any/arena.c
    ../any/djb2.c
    ../any/event.c
    ../any/mem.c
    ../openssl/hash.c
    random.c
    ../posix/init.c
    ../posix/halt.c
    ../posix/panicf.c
    ../printf/float.c
    ../printf/printf.c
    ../posix/stdio.c
    ../posix/sleep.c
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
