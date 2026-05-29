set(NAME "picofuse-net-obj")

find_library(MOSQUITTO_LIB mosquitto)
find_path(MOSQUITTO_INCLUDE_DIR mosquitto.h)

if(MOSQUITTO_LIB AND MOSQUITTO_INCLUDE_DIR)
    message(STATUS "Found libmosquitto: ${MOSQUITTO_LIB}")
    add_library(${NAME} OBJECT
        ../mosquitto/mqtt.c
        ../mosquitto/net.c
    )
    target_include_directories(${NAME} PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${MOSQUITTO_INCLUDE_DIR}
    )
    target_link_libraries(${NAME} PUBLIC ${MOSQUITTO_LIB})
else()
    message(STATUS "libmosquitto not found, using stub")
    add_library(${NAME} OBJECT
        ../stub/mqtt.c
        ../stub/net.c
    )
    target_include_directories(${NAME} PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    )
endif()
