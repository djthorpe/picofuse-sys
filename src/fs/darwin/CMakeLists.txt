set(NAME "picofuse-fs-obj")

add_library(${NAME} OBJECT
    ../any/pool.c
    ../any/volume.c
    ../any/file.c
    ../stub/vol_mem.c
    ../stub/vol_file.c
    ../stub/vol_flash.c
    ../posix/path.c
    ../posix/volume.c
    ../posix/file.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
