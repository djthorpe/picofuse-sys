set(NAME "picofuse-example-picosdk-stdout-flood")

add_executable(${NAME}
    main.c
)

target_link_libraries(${NAME} PRIVATE
    pico_stdlib
    pico_multicore
)

if(DEFINED PICO_BOARD)
    pico_enable_stdio_uart(${NAME} FALSE)
    pico_enable_stdio_usb(${NAME} TRUE)
    pico_add_extra_outputs(${NAME})
endif()