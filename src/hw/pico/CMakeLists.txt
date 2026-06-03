set(NAME "picofuse-hw-obj")

add_library(${NAME} OBJECT
    adc.c
    gpio.c
    init.c
    i2c.c
    uart.c
    wifi.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(${NAME} PUBLIC
    hardware_gpio
    hardware_i2c
    hardware_uart
    hardware_adc
)

# Check if we're building for a board with CYW43 support
if (PICO_CYW43_SUPPORTED)
    message(STATUS "Board has CYW43 support - enabling CYW43 library")
    target_compile_definitions(${NAME} PRIVATE PICO_CYW43_SUPPORTED=1)
    target_link_libraries(${NAME} PUBLIC
        pico_cyw43_arch_lwip_poll
    )
endif()
