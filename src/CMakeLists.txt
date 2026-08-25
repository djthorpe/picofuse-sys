add_subdirectory(fs/${SYSTEM_NAME_LOWER})
add_subdirectory(hw/${SYSTEM_NAME_LOWER})
add_subdirectory(sys/${SYSTEM_NAME_LOWER})
add_subdirectory(dev/${SYSTEM_NAME_LOWER})
add_subdirectory(net/${SYSTEM_NAME_LOWER})
add_subdirectory(hid)
add_subdirectory(pix)
add_subdirectory(app)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/picofuse/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/picofuse
)

install(FILES ${PROJECT_SOURCE_DIR}/cmake/picofuse.cmake
    DESTINATION ${CMAKE_INSTALL_DATADIR}/picofuse/cmake
)
