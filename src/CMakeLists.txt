set(NAME "picofuse-sys")

add_subdirectory(hw/${SYSTEM_NAME_LOWER})
add_subdirectory(sys/${SYSTEM_NAME_LOWER})
add_subdirectory(dev)

add_library(${NAME} STATIC)

target_sources(${NAME} PRIVATE
    $<TARGET_OBJECTS:picofuse-hw-obj>
    $<TARGET_OBJECTS:picofuse-sys-obj>
    $<TARGET_OBJECTS:picofuse-dev-obj>
)

target_link_libraries(${NAME} PUBLIC
    picofuse-hw-obj
    picofuse-sys-obj
    picofuse-dev-obj
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
