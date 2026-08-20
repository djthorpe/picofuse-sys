set(NAME "picofuse-fs-obj")

add_library(${NAME} OBJECT
    ../any/pool.c
    ../any/volume.c
    ../any/file.c
    ../littlefs/mem.c
    ../littlefs/flash.c
    ../littlefs/ops.c
    ../littlefs/path.c
    ../stub/vol_file.c
    ../stub/vol_path.c
    ${PROJECT_SOURCE_DIR}/third_party/littlefs/lfs.c
    ${PROJECT_SOURCE_DIR}/third_party/littlefs/lfs_util.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(${NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/littlefs
)
