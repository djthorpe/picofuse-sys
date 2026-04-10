set(NAME "picofuse-sys")

add_subdirectory(hw/pico)
add_subdirectory(sys/pico)

add_library(${NAME} STATIC)

target_sources(${NAME} PRIVATE
    $<TARGET_OBJECTS:picofuse-hw-obj>
    $<TARGET_OBJECTS:picofuse-sys-obj>
)

target_link_libraries(${NAME} PUBLIC
    picofuse-hw-obj
    picofuse-sys-obj
    pico_stdlib
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
