cmake_minimum_required(VERSION 3.13)

project(ftdi-controllers)

include_directories(${LIBUSB_INC})
include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_controller_SOURCE_DIR}/include)

set(ft4222h_common_src
	ft4222h.c
	ft4222h-spi.c
)

set(mpsse_common_src
	mpsse.c
	mpsse-spi.c
)

set(presets_libusb_files
	presets/ft2232h-libusb.json
	presets/ft4222h-libusb.json
	presets/ft4232h-libusb.json
)

# FT4222H libusb
add_library(ft4222h-libusb SHARED ftdi-libusb.c ft4222h-libusb.c ${ft4222h_common_src} ft4222h-libusb.def)
target_link_libraries(ft4222h-libusb PRIVATE ufprog_common ufprog_controller ${LIBUSB_LIB})
target_include_directories(ft4222h-libusb PRIVATE ${libusb_common_SOURCE_DIR})
target_compile_definitions(ft4222h-libusb PRIVATE UFP_MODULE_NAME=\"FT4222H-libusb\")
set_target_properties(ft4222h-libusb PROPERTIES PREFIX "" OUTPUT_NAME "ft4222h-libusb")

# MPSSE (FT232H/FT2232/FT4232) libusb
add_library(mpsse-libusb SHARED ftdi-libusb.c mpsse-libusb.c ${mpsse_common_src} mpsse-libusb.def)
target_link_libraries(mpsse-libusb PRIVATE ufprog_common ufprog_controller ${LIBUSB_LIB})
target_include_directories(mpsse-libusb PRIVATE ${libusb_common_SOURCE_DIR})
target_compile_definitions(mpsse-libusb PRIVATE UFP_MODULE_NAME=\"MPSSE-libusb\")
set_target_properties(mpsse-libusb PROPERTIES PREFIX "" OUTPUT_NAME "mpsse-libusb")

install(TARGETS ft4222h-libusb mpsse-libusb
	RUNTIME DESTINATION ${CONTROLLER_DRIVER_DIR}
	LIBRARY DESTINATION ${CONTROLLER_DRIVER_DIR}
)

if(WIN32)
set(presets_d2xx_files
	presets/ft2232h.json
	presets/ft4222h.json
	presets/ft4232h.json
)

# FT4222H D2XX
add_library(ft4222h-d2xx SHARED d2xx.c ft4222h-d2xx.c ${ft4222h_common_src} ft4222h-d2xx.def)
target_link_libraries(ft4222h-d2xx PRIVATE ufprog_common ufprog_controller)
target_compile_definitions(ft4222h-d2xx PRIVATE UFP_MODULE_NAME=\"FT4222H-D2XX\")
set_target_properties(ft4222h-d2xx PROPERTIES PREFIX "" OUTPUT_NAME "ft4222h-d2xx")

# MPSSE (FT232H/FT2232/FT4232) D2XX
add_library(mpsse-d2xx SHARED d2xx.c mpsse-d2xx.c ${mpsse_common_src} mpsse-d2xx.def)
target_link_libraries(mpsse-d2xx PRIVATE ufprog_common ufprog_controller)
target_compile_definitions(mpsse-d2xx PRIVATE UFP_MODULE_NAME=\"MPSSE-D2XX\")
set_target_properties(mpsse-d2xx PROPERTIES PREFIX "" OUTPUT_NAME "mpsse-d2xx")

install(TARGETS ft4222h-d2xx mpsse-d2xx
	RUNTIME DESTINATION ${CONTROLLER_DRIVER_DIR}
	LIBRARY DESTINATION ${CONTROLLER_DRIVER_DIR}
)
endif()

install(
	FILES ${presets_libusb_files} ${presets_d2xx_files}
	DESTINATION ${DEVICE_DIR}
)
