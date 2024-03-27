cmake_minimum_required(VERSION 3.13)

project(wch-controllers)

if(USE_VCPKG)
	find_package(hidapi CONFIG REQUIRED)
	set(HIDAPI_INC hidapi::include)
	set(HIDAPI_LIB hidapi::hidapi hidapi::winapi)
else()
	find_package(PkgConfig)

	if(WIN32 OR MINGW)
		pkg_check_modules(hidapi REQUIRED hidapi)
		set(HIDAPI_INC ${hidapi_INCLUDEDIR})
		set(HIDAPI_LIB ${hidapi_LIBRARIES})
	else()
		pkg_check_modules(hidapi REQUIRED hidapi-libusb)
		set(HIDAPI_INC ${hidapi-libusb_INCLUDEDIR})
		set(HIDAPI_LIB ${hidapi-libusb_LIBRARIES})
	endif()
endif()

include_directories(${LIBUSB_INC})
include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_controller_SOURCE_DIR}/include)

set(ch341_common_src
	ch341.c
	ch341-spi.c
)

set(ch347_common_src
	ch347.c
	ch347-spi.c
)

set(wch_libusb_files
	presets/ch341-libusb.json
	presets/ch347-libusb.json
	presets/ch347t-libusb.json
	presets/ch347f-libusb.json
	presets/ch347-hid.json
)

# CH341 libusb
add_library(ch341-libusb SHARED ch341-libusb.c ${ch341_common_src} ch341-libusb.def)
target_link_libraries(ch341-libusb PRIVATE ufprog_common ufprog_controller ${LIBUSB_LIB})
target_include_directories(ch341-libusb PRIVATE ${libusb_common_SOURCE_DIR} ${LIBUSB_INC})
target_compile_definitions(ch341-libusb PRIVATE UFP_MODULE_NAME=\"CH341-libusb\")
set_target_properties(ch341-libusb PROPERTIES PREFIX "" OUTPUT_NAME "ch341-libusb")

# CH347 libusb
add_library(ch347-libusb SHARED ch347-libusb.c ${ch347_common_src} ch347-libusb.def)
target_link_libraries(ch347-libusb PRIVATE ufprog_common ufprog_controller ${LIBUSB_LIB})
target_include_directories(ch347-libusb PRIVATE ${libusb_common_SOURCE_DIR} ${LIBUSB_INC})
target_compile_definitions(ch347-libusb PRIVATE UFP_MODULE_NAME=\"CH347-libusb\")
set_target_properties(ch347-libusb PROPERTIES PREFIX "" OUTPUT_NAME "ch347-libusb")

# CH347 HID
add_library(ch347-hid SHARED ch347-hid.c ${ch347_common_src} ch347-hid.def)
target_link_libraries(ch347-hid PRIVATE ufprog_common ufprog_controller ${HIDAPI_LIB})
target_include_directories(ch347-hid PRIVATE ${libusb_common_SOURCE_DIR} ${HIDAPI_INC})
target_compile_definitions(ch347-hid PRIVATE UFP_MODULE_NAME=\"CH347-HID\")
set_target_properties(ch347-hid PROPERTIES PREFIX "" OUTPUT_NAME "ch347-hid")

install(TARGETS ch341-libusb ch347-libusb ch347-hid
	RUNTIME DESTINATION ${CONTROLLER_DRIVER_DIR}
	LIBRARY DESTINATION ${CONTROLLER_DRIVER_DIR}
)

if(WIN32)
set(wch_dll_files
	presets/ch341.json
	presets/ch347.json
)

# CH341 DLL
add_library(ch341-dll SHARED ch34x-dll.c ch341-dll.c ${ch341_common_src} ch341-dll.def)
target_link_libraries(ch341-dll PRIVATE ufprog_common ufprog_controller)
target_compile_definitions(ch341-dll PRIVATE UFP_MODULE_NAME=\"CH341-DLL\")
set_target_properties(ch341-dll PROPERTIES PREFIX "" OUTPUT_NAME "ch341-dll")

# CH347 DLL
add_library(ch347-dll SHARED ch34x-dll.c ch347-dll.c ${ch347_common_src} ch347-dll.def)
target_link_libraries(ch347-dll PRIVATE ufprog_common ufprog_controller)
target_compile_definitions(ch347-dll PRIVATE UFP_MODULE_NAME=\"CH347-DLL\")
set_target_properties(ch347-dll PROPERTIES PREFIX "" OUTPUT_NAME "ch347-dll")

install(TARGETS ch341-dll ch347-dll
	RUNTIME DESTINATION ${CONTROLLER_DRIVER_DIR}
	LIBRARY DESTINATION ${CONTROLLER_DRIVER_DIR}
)
endif()

install(
	FILES ${wch_libusb_files} ${wch_dll_files}
	DESTINATION ${DEVICE_DIR}
)
