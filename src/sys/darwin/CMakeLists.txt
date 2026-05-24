set(NAME "picofuse-sys-obj")

add_library(${NAME} OBJECT
    ../posix/init.c
    ../posix/halt.c
    ../posix/panicf.c
    ../posix/printf.c
    random.c
    ../posix/sleep.c
    ../posix/timestamp.c
    ../pthread/mutex.c
    ../pthread/thread.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    pthread
)
