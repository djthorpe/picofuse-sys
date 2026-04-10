set(NAME "picofuse-example-hw-wifi")

add_executable(${NAME}
    main.c
)

target_link_libraries(${NAME} PRIVATE
    picofuse-sys
)

pico_enable_stdio_uart(${NAME} FALSE)
pico_enable_stdio_usb(${NAME} TRUE)
pico_add_extra_outputs(${NAME})
