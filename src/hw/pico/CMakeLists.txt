set(NAME "picofuse-hw")

set(PICOFUSE_PICO_USB_SOURCE ../stub/usb.c)
if(PICOFUSE_USB)
    set(PICOFUSE_PICO_USB_SOURCE usb.c)
endif()

add_library(${NAME} STATIC
    ../any/block.c
    infrared_rx.c
    adc.c
    lock.c
    flash.c
    gpio.c
    init.c
    i2c.c
    led.c
    pwm.c
    spi.c
    ${PICOFUSE_PICO_USB_SOURCE}
    uart.c
    watchdog.c
    wifi.c
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(${NAME} PUBLIC
    picofuse-sys
    pico_stdlib
    pico_flash
    hardware_flash
    hardware_gpio
    hardware_i2c
    hardware_pio
    hardware_uart
    hardware_spi
    hardware_adc
    hardware_pwm
    hardware_watchdog
)

pico_generate_pio_header(${NAME}
    ${CMAKE_CURRENT_LIST_DIR}/led_neopixel.pio
)

pico_generate_pio_header(${NAME}
    ${CMAKE_CURRENT_LIST_DIR}/infrared_rx.pio
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
    target_link_libraries(${NAME} PRIVATE
        pico_cyw43_arch_lwip_poll
    )
endif()

# See _hw_pico_init_anchor() in init.c: forces this module's real
# hw_init()/hw_exit()/hw_poll() to always be part of the link, so they
# correctly override picofuse-app's weak fallbacks of the same names
# regardless of link order.
target_link_options(${NAME} INTERFACE
    "LINKER:-u,_hw_pico_init_anchor"
)

picofuse_add_module(${NAME} REQUIRES picofuse-sys)
