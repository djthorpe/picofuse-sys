set(NAME "picofuse-example-dev-framebuffer")

add_executable(${NAME}
    main.c
)

target_link_libraries(${NAME} PRIVATE
    picofuse-dev
    picofuse-hw
    picofuse-pix
    picofuse-sys
)

if(DEFINED PICO_BOARD)
    pico_set_printf_implementation(${NAME} none)
    pico_enable_stdio_uart(${NAME} FALSE)
    pico_enable_stdio_usb(${NAME} TRUE)
    pico_add_extra_outputs(${NAME})
endif()
