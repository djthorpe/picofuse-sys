set(NAME "picofuse-sys-obj")

add_library(${NAME} OBJECT
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
