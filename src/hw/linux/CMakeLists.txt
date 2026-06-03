set(NAME "picofuse-hw-obj")

find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_search_module(WPA_CLIENT QUIET libwpa_client wpa_client)
    pkg_search_module(LIBUSB QUIET libusb-1.0)
endif()

find_path(WPA_CTRL_INCLUDE_DIR
    NAMES wpa_ctrl.h
    HINTS ${WPA_CLIENT_INCLUDE_DIRS}
    PATH_SUFFIXES wpa_client wpa_supplicant
)

find_library(WPA_CTRL_LIBRARY
    NAMES wpa_client libwpa_client
    HINTS ${WPA_CLIENT_LIBRARY_DIRS}
)

set(WIFI_SOURCE ../stub/wifi.c)
set(USB_SOURCE ../stub/usb.c)
set(HAVE_WPA_CTRL OFF)
set(HAVE_LIBUSB OFF)

if (WPA_CTRL_INCLUDE_DIR AND (WPA_CTRL_LIBRARY OR WPA_CLIENT_FOUND))
    set(HAVE_WPA_CTRL ON)
endif()

if (LIBUSB_FOUND)
    set(HAVE_LIBUSB ON)
endif()

if (HAVE_WPA_CTRL)
    message(STATUS "Found wpa_ctrl: enabling Linux Wi-Fi backend")
    set(WIFI_SOURCE wifi.c)
else()
    message(STATUS "wpa_ctrl not found: using Linux Wi-Fi stub backend")
endif()

if (HAVE_LIBUSB)
    message(STATUS "Found libusb-1.0: enabling Linux USB backend")
    set(USB_SOURCE ../libusb/usb.c)
else()
    message(STATUS "libusb-1.0 not found: using Linux USB stub backend")
endif()

add_library(${NAME} OBJECT
    ../stub/init.c
    ../stub/adc.c
    ../stub/gpio.c
    i2c.c
    spi.c
    ${USB_SOURCE}
    ../stub/uart.c
    ${WIFI_SOURCE}
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

if (HAVE_WPA_CTRL)
    target_include_directories(${NAME} PRIVATE
        ${WPA_CTRL_INCLUDE_DIR}
        ${WPA_CLIENT_INCLUDE_DIRS}
    )

    if (WPA_CTRL_LIBRARY)
        target_link_libraries(${NAME} PUBLIC
            ${WPA_CTRL_LIBRARY}
        )
    endif()

    if (WPA_CLIENT_LIBRARIES)
        target_link_libraries(${NAME} PUBLIC
            ${WPA_CLIENT_LIBRARIES}
        )
    endif()
endif()

if (HAVE_LIBUSB)
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
