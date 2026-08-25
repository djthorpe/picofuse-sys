set(NAME "picofuse-example-app")

add_executable(${NAME}
    main.c
)

target_link_libraries(${NAME} PRIVATE
    picofuse-app
    picofuse-hid
    picofuse-hw
    picofuse-net
    picofuse-sys
)

# Wi-Fi credentials from environment (export WIFI_SSID and WIFI_PASSWORD).
# Optional here (unlike examples/hw/wifi-join): the app only attempts to
# join a network if APP_FLAG_WIFI registered successfully AND a non-empty
# SSID was provided.
set(WIFI_SSID "$ENV{WIFI_SSID}")
set(WIFI_PASSWORD "$ENV{WIFI_PASSWORD}")

target_compile_definitions(${NAME} PRIVATE
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PASSWORD="${WIFI_PASSWORD}"
)

if(DEFINED PICO_BOARD)
    pico_set_printf_implementation(${NAME} none)
    pico_enable_stdio_uart(${NAME} TRUE)
    pico_enable_stdio_usb(${NAME} FALSE)
    pico_add_extra_outputs(${NAME})
endif()
