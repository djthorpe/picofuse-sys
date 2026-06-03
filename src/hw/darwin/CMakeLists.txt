set(NAME "picofuse-hw-obj")

add_library(${NAME} OBJECT
    ../stub/init.c
    ../stub/gpio.c
    ../stub/i2c.c
    ../stub/uart.c
    ../stub/wifi.c
)
target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
