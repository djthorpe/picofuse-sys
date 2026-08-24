set(NAME "picofuse-example-hw-gpio")

add_executable(${NAME}
    main.c
)

target_link_libraries(${NAME} PRIVATE
    picofuse-hw
    picofuse-sys
)

if(DEFINED PICO_BOARD)
    pico_set_printf_implementation(${NAME} none)
    pico_add_extra_outputs(${NAME})
endif()
