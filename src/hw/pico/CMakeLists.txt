set(NAME "picofuse-hw-obj")

set(PICOFUSE_PICO_USB_SOURCE ../stub/usb.c)
if(PICOFUSE_USB)
    set(PICOFUSE_PICO_USB_SOURCE usb.c)
endif()

add_library(${NAME} OBJECT
    adc.c
    gpio.c
    init.c
    i2c.c
    led.c
    pwm.c
    spi.c
    ${PICOFUSE_PICO_USB_SOURCE}
    uart.c
    wifi.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(${NAME} PUBLIC
    pico_stdlib
    hardware_gpio
    hardware_i2c
    hardware_uart
    hardware_spi
    hardware_adc
    hardware_pwm
)

if(PICOFUSE_USB)
    target_compile_definitions(${NAME} PRIVATE
        CFG_TUSB_CONFIG_FILE=\"host_tusb_config.h\"
    )
    target_link_libraries(${NAME} PRIVATE
        tinyusb_host
        tinyusb_board
    )
endif()

# Check if we're building for a board with CYW43 support
if (PICO_CYW43_SUPPORTED)
    message(STATUS "Board has CYW43 support - enabling CYW43 library")
    target_compile_definitions(${NAME} PRIVATE PICO_CYW43_SUPPORTED=1)
    target_link_libraries(${NAME} PUBLIC
        pico_cyw43_arch_lwip_poll
    )
endif()
