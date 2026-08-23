set(NAME "picofuse-dev-obj")

find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_search_module(LIBUSB QUIET libusb-1.0)
    pkg_search_module(LIBFTDI QUIET libftdi1)
endif()

set(HAVE_LIBUSB OFF)
if (LIBUSB_FOUND)
    set(HAVE_LIBUSB ON)
endif()

set(HAVE_LIBFTDI OFF)
if (PICOFUSE_USB AND HAVE_LIBUSB AND LIBFTDI_FOUND)
    set(HAVE_LIBFTDI ON)
endif()

set(LIBFTDI_SOURCES ../ftdi/stub.c)
if (HAVE_LIBFTDI)
    message(STATUS "Found libftdi1: enabling FT232H support")
    set(LIBFTDI_SOURCES ../ftdi/ftdi.c)
elseif (PICOFUSE_USB AND HAVE_LIBUSB)
    message(STATUS "libftdi1 not found: FT232H support disabled")
endif()

add_library(${NAME} OBJECT
    ../bme680/bme680.c
    ../bme680/hid.c
    ../bme280/bme280.c
    ../framebuffer/framebuffer.c
    ../ft6236/ft6236.c
    ../ft6236/presto_touch.c
    ../st7701/st7701.c
    ../tca9555/pimoroni_pad.c
    ../tca9555/tca9555.c
    ../uc8151/uc8151.c
    ${LIBFTDI_SOURCES}
)

target_include_directories(${NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${NAME} PUBLIC
    picofuse-hw-obj
    picofuse-sys-obj
)

if (HAVE_LIBFTDI)
    target_include_directories(${NAME} PRIVATE
        ${LIBFTDI_INCLUDE_DIRS}
        ${LIBUSB_INCLUDE_DIRS}
    )
    target_compile_options(${NAME} PRIVATE
        ${LIBFTDI_CFLAGS_OTHER}
        ${LIBUSB_CFLAGS_OTHER}
    )
    target_link_directories(${NAME} PUBLIC
        ${LIBFTDI_LIBRARY_DIRS}
        ${LIBUSB_LIBRARY_DIRS}
    )
    target_link_libraries(${NAME} PUBLIC
        ${LIBFTDI_LIBRARIES}
        ${LIBUSB_LIBRARIES}
    )
    target_link_options(${NAME} PUBLIC
        ${LIBFTDI_LDFLAGS_OTHER}
        ${LIBUSB_LDFLAGS_OTHER}
    )
endif()
