set(NAME "picofuse-app-obj")

add_library(${NAME} OBJECT
    app.c
    hid.c
    hw.c
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${NAME} PUBLIC
    picofuse-sys-obj
)
