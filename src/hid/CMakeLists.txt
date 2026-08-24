set(NAME "picofuse-hid")

if(SYSTEM_NAME_LOWER STREQUAL "linux")
    set(EVDEV_SOURCE linux/evdev.c)
else()
    set(EVDEV_SOURCE stub/evdev.c)
endif()

add_library(${NAME} STATIC
    any/device.c
    any/event.c
    any/gpio.c
    any/keycode.c
    any/signal.c
    any/timer.c
    ${EVDEV_SOURCE}
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${NAME} PUBLIC
    picofuse-hw
    picofuse-sys
)

picofuse_add_module(${NAME} REQUIRES picofuse-hw)
