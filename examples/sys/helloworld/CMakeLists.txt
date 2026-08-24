set(NAME "helloworld")

add_executable(${NAME}
    main.c
)

target_link_libraries(${NAME} PRIVATE
    picofuse-sys
)

if(DEFINED PICO_BOARD)
    pico_set_printf_implementation(${NAME} none)
    pico_enable_stdio_uart(${NAME} TRUE)
    pico_enable_stdio_usb(${NAME} FALSE)
    pico_add_extra_outputs(${NAME})
endif()
