set(NAME "picofuse-dev-obj")

add_library(${NAME} OBJECT
    bme280/bme280.c
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${NAME} PUBLIC
    picofuse-hw-obj
    picofuse-sys-obj
)
