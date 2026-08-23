set(NAME "picofuse-pix-obj")

add_library(${NAME} OBJECT
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
    picofuse-sys-obj
)
