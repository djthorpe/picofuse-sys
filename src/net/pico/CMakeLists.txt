set(NAME "picofuse-net-obj")

add_library(${NAME} OBJECT
    mqtt.c
    net.c
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${NAME} PUBLIC
    pico_lwip_mqtt
)
