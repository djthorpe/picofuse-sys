
# add include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

# System tests
set(NAME "sys")
add_executable(${NAME}
    sys/test.c
)
target_link_libraries(${NAME} PRIVATE
    picofuse-sys
)
add_test(NAME ${NAME} COMMAND ${NAME})
