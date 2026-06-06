set(NAME "picofuse-dev-obj")

add_library(${NAME} OBJECT
    bme680/bme680.c
    bme680/hid.c
    bme280/bme280.c
    ft6236/ft6236.c
    ft6236/presto_touch.c
    st7701/st7701.c
    tca9555/pimoroni_pad.c
    tca9555/tca9555.c
    uc8151/uc8151.c
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${NAME} PUBLIC
    picofuse-hw-obj
    picofuse-sys-obj
)

if(DEFINED PICO_BOARD)
    pico_generate_pio_header(${NAME}
        ${CMAKE_CURRENT_LIST_DIR}/st7701/st7701.pio
    )
endif()
