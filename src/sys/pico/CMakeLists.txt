set(NAME "picofuse-sys-obj")

add_library(${NAME} OBJECT
    halt.c
    mem.c
    panicf.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)
