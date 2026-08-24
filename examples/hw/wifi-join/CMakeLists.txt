set(NAME "picofuse-example-hw-wifi-join")

add_executable(${NAME}
    main.c
)

target_link_libraries(${NAME} PRIVATE
    picofuse-hw
    picofuse-sys
)

# Wi-Fi credentials from environment (export WIFI_SSID and WIFI_PASSWORD)
set(WIFI_SSID "$ENV{WIFI_SSID}")
set(WIFI_PASSWORD "$ENV{WIFI_PASSWORD}")

# Add Wi-Fi credentials as compile definitions
target_compile_definitions(${NAME} PRIVATE
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PASSWORD="${WIFI_PASSWORD}"
)

if(DEFINED PICO_BOARD)
    pico_set_printf_implementation(${NAME} none)
    pico_enable_stdio_uart(${NAME} FALSE)
    pico_enable_stdio_usb(${NAME} TRUE)
    pico_add_extra_outputs(${NAME})
endif()
