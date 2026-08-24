set(NAME "picofuse-pix")

add_library(${NAME} STATIC
    jpeg.c
    pix.c
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_include_directories(${NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/nanojpeg
)

target_link_libraries(${NAME} PUBLIC
    picofuse-sys
)

picofuse_add_module(${NAME} REQUIRES picofuse-sys)
