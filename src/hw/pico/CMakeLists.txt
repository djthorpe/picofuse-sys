set(NAME "picofuse-hw")

add_library(${NAME} STATIC
    gpio.c
    uart.c
)
target_include_directories(${NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../../../include
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(${NAME} 
    hardware_gpio
    hardware_uart
)
