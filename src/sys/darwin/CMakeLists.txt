set(NAME "picofuse-sys-obj")

add_library(${NAME} OBJECT
    ../posix/init.c
    ../posix/halt.c
    ../posix/panicf.c
    ../posix/printf.c
    ../posix/sleep.c
    ../posix/timestamp.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
