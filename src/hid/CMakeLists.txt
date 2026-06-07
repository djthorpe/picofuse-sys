set(NAME "picofuse-hid-obj")

add_library(${NAME} OBJECT
    any/device.c
    any/event.c
    any/gpio.c
    any/keycode.c
    any/signal.c
    any/timer.c
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${NAME} PUBLIC
    picofuse-sys-obj
)
