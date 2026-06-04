set(NAME "picofuse-sys")

add_subdirectory(hw/${SYSTEM_NAME_LOWER})
add_subdirectory(sys/${SYSTEM_NAME_LOWER})
add_subdirectory(dev)

add_library(${NAME} STATIC)

set_target_properties(${NAME} PROPERTIES
    OUTPUT_NAME "${PICOFUSE_LIBRARY_BASENAME}"
)

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

if(DEFINED PROGRAM_VERSION)
    set(PICOFUSE_PC_VERSION "${PROGRAM_VERSION}")
else()
    set(PICOFUSE_PC_VERSION "0.0.0")
endif()

set(PICOFUSE_PC_LIB_NAME "${PICOFUSE_LIBRARY_BASENAME}")

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
