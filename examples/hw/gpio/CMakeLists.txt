set(NAME "picofuse-example-hw-gpio")

add_executable(${NAME}
    main.c
)

target_link_libraries(${NAME} PRIVATE
    picofuse-sys
)

pico_add_extra_outputs(${NAME})