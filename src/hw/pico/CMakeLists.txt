set(NAME "picofuse-hw-obj")

add_library(${NAME} OBJECT
    gpio.c
    uart.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(${NAME} PUBLIC
    hardware_gpio
    hardware_uart
    hardware_adc
)
