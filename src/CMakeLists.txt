set(NAME "picofuse-sys")

add_subdirectory(hw/${SYSTEM_NAME_LOWER})
add_subdirectory(sys/${SYSTEM_NAME_LOWER})
add_subdirectory(dev)
add_subdirectory(hid)

add_library(${NAME} STATIC)

set_target_properties(${NAME} PROPERTIES
    OUTPUT_NAME "${PICOFUSE_LIBRARY_BASENAME}"
)

target_sources(${NAME} PRIVATE
    $<TARGET_OBJECTS:picofuse-hw-obj>
    $<TARGET_OBJECTS:picofuse-sys-obj>
    $<TARGET_OBJECTS:picofuse-dev-obj>
    $<TARGET_OBJECTS:picofuse-hid-obj>
)

target_link_libraries(${NAME} PUBLIC
    picofuse-hw-obj
    picofuse-sys-obj
    picofuse-dev-obj
    picofuse-hid-obj
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

if(DEFINED PROGRAM_VERSION)
    set(PICOFUSE_PC_VERSION "${PROGRAM_VERSION}")
else()
    set(PICOFUSE_PC_VERSION "0.0.0")
endif()

set(PICOFUSE_PC_LIB_NAME "${PICOFUSE_LIBRARY_BASENAME}")

# Resolve prefix relative to ${pcfiledir} for relocatable pkg-config files,
# including multi-component libdirs (e.g. lib/x86_64-linux-gnu).
set(PICOFUSE_PC_PREFIX_REL "..")
string(REPLACE "/" ";" PICOFUSE_INSTALL_LIBDIR_PARTS "${CMAKE_INSTALL_LIBDIR}")
foreach(PICOFUSE_LIBDIR_PART IN LISTS PICOFUSE_INSTALL_LIBDIR_PARTS)
    if(NOT PICOFUSE_LIBDIR_PART STREQUAL "")
        string(APPEND PICOFUSE_PC_PREFIX_REL "/..")
    endif()
endforeach()

configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/picofuse-sys.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PICOFUSE_PC_FILE_NAME}
    @ONLY
)

install(TARGETS ${NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/picofuse/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/picofuse
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PICOFUSE_PC_FILE_NAME}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
