set(NAME "picofuse-net")

add_library(${NAME} STATIC
    ../stub/ntp.c
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${NAME} PUBLIC
    picofuse-sys
)

picofuse_add_module(${NAME} REQUIRES picofuse-sys)
