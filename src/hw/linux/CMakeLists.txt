set(NAME "picofuse-hw-obj")

find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_search_module(WPA_CLIENT QUIET libwpa_client wpa_client)
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
set(HAVE_WPA_CTRL OFF)

if (WPA_CTRL_INCLUDE_DIR AND (WPA_CTRL_LIBRARY OR WPA_CLIENT_FOUND))
    set(HAVE_WPA_CTRL ON)
endif()

if (HAVE_WPA_CTRL)
    message(STATUS "Found wpa_ctrl: enabling Linux Wi-Fi backend")
    set(WIFI_SOURCE wifi.c)
else()
    message(STATUS "wpa_ctrl not found: using Linux Wi-Fi stub backend")
endif()

add_library(${NAME} OBJECT
    ../stub/init.c
    ../stub/adc.c
    ../stub/gpio.c
    i2c.c
    ../stub/usb.c
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
