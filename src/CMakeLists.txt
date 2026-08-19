set(NAME "picofuse-sys")

add_subdirectory(fs/${SYSTEM_NAME_LOWER})
add_subdirectory(hw/${SYSTEM_NAME_LOWER})
add_subdirectory(sys/${SYSTEM_NAME_LOWER})

add_library(${NAME} STATIC)

target_sources(${NAME} PRIVATE
    $<TARGET_OBJECTS:picofuse-fs-obj>
    $<TARGET_OBJECTS:picofuse-hw-obj>
    $<TARGET_OBJECTS:picofuse-sys-obj>
)

target_link_libraries(${NAME} PUBLIC
    picofuse-fs-obj
    picofuse-hw-obj
    picofuse-sys-obj
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
