set(NAME "picofuse-dev-obj")

add_library(${NAME} OBJECT
    bme680/bme680.c
    bme280/bme280.c
    ft6236/ft6236.c
    uc8151/uc8151.c
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${NAME} PUBLIC
    picofuse-hw-obj
    picofuse-sys-obj
)
