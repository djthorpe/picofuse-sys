set(NAME "picofuse-hw-obj")

find_path(WPA_CTRL_INCLUDE_DIR
    NAMES wpa_ctrl.h
    PATH_SUFFIXES wpa_client
)

find_library(WPA_CTRL_LIBRARY
    NAMES wpa_client
)

set(WIFI_SOURCE ../stub/wifi.c)

if (WPA_CTRL_INCLUDE_DIR AND WPA_CTRL_LIBRARY)
    message(STATUS "Found wpa_ctrl: enabling Linux Wi-Fi backend")
    set(WIFI_SOURCE wifi.c)
endif()

add_library(${NAME} OBJECT
    ../stub/init.c
    ../stub/gpio.c
    ../stub/uart.c
    ${WIFI_SOURCE}
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

if (WPA_CTRL_INCLUDE_DIR AND WPA_CTRL_LIBRARY)
    target_include_directories(${NAME} PRIVATE
        ${WPA_CTRL_INCLUDE_DIR}
    )
    target_link_libraries(${NAME} PUBLIC
        ${WPA_CTRL_LIBRARY}
    )
endif()
