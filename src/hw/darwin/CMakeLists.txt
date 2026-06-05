set(NAME "picofuse-hw-obj")

find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_search_module(LIBUSB QUIET libusb-1.0)
endif()

set(USB_SOURCE ../stub/usb.c)
set(HAVE_LIBUSB OFF)

if (LIBUSB_FOUND)
    set(HAVE_LIBUSB ON)
endif()

if (PICOFUSE_USB AND HAVE_LIBUSB)
    message(STATUS "Found libusb-1.0: enabling Darwin USB backend")
    set(USB_SOURCE ../libusb/usb.c)
else()
    if (PICOFUSE_USB)
        message(STATUS "libusb-1.0 not found: using Darwin USB stub backend")
    else()
        message(STATUS "PICOFUSE_USB is OFF: using Darwin USB stub backend")
    endif()
endif()

add_library(${NAME} OBJECT
    ../stub/init.c
    ../stub/adc.c
    ../stub/gpio.c
    ../stub/led.c
    ../stub/pwm.c
    ../stub/i2c.c
    ../stub/spi.c
    ${USB_SOURCE}
    ../stub/uart.c
    ../stub/wifi.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

if (PICOFUSE_USB AND HAVE_LIBUSB)
    find_package(Threads QUIET)

    target_include_directories(${NAME} PRIVATE
        ${LIBUSB_INCLUDE_DIRS}
    )
    target_compile_options(${NAME} PRIVATE
        ${LIBUSB_CFLAGS_OTHER}
    )
    target_link_directories(${NAME} PUBLIC
        ${LIBUSB_LIBRARY_DIRS}
    )
    target_link_libraries(${NAME} PUBLIC
        ${LIBUSB_LIBRARIES}
    )
    target_link_options(${NAME} PUBLIC
        ${LIBUSB_LDFLAGS_OTHER}
    )

    if (Threads_FOUND)
        target_link_libraries(${NAME} PUBLIC Threads::Threads)
    endif()
endif()
