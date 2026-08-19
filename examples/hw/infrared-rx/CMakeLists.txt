set(NAME "picofuse-example-hw-infrared-rx")

add_executable(${NAME}
    main.c
    nec.c
    sony.c
)

target_link_libraries(${NAME} PRIVATE
    picofuse-sys
)

if(DEFINED PICO_BOARD)
    pico_set_printf_implementation(${NAME} none)
    pico_enable_stdio_uart(${NAME} FALSE)
    pico_enable_stdio_usb(${NAME} TRUE)
    pico_add_extra_outputs(${NAME})
endif()
