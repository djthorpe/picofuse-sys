set(NAME "picofuse-net")

add_library(${NAME} STATIC
    ntp.c
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${NAME} PUBLIC
    picofuse-sys
)

# Check if we're building for a board with CYW43 support
if (PICO_CYW43_SUPPORTED)
    target_compile_definitions(${NAME} PRIVATE
        PICO_CYW43_SUPPORTED=1
    )
    target_include_directories(${NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/src/hw/pico
    )
    target_link_libraries(${NAME} PRIVATE
        pico_cyw43_arch_lwip_poll
        pico_lwip_sntp
    )
endif()

picofuse_add_module(${NAME} REQUIRES picofuse-sys)
