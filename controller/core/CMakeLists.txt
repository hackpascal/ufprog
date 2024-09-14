cmake_minimum_required(VERSION 3.13)

project(ufprog_controller)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(ufprog_controller_src
	driver.c
	device.c
	libusb.c
	init.c
)

add_library(ufprog_controller SHARED ${ufprog_controller_src} ufprog-controller.def)
target_link_libraries(ufprog_controller PRIVATE ufprog_common ${LIBUSB_LIB})
set_target_properties(ufprog_controller PROPERTIES OUTPUT_NAME "ufprog-controller")

include_directories(${LIBUSB_INC})
include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_common_SOURCE_DIR}/internal)
include_directories(${ufprog_controller_SOURCE_DIR}/include)

if(WIN32 OR MINGW OR BUILD_PORTABLE)
	set_target_properties(ufprog_controller PROPERTIES PREFIX "")
else()
	set_target_properties(ufprog_controller PROPERTIES
		VERSION ${UFPROG_VERSION}
		SOVERSION 1
	)
endif()

install(TARGETS ufprog_controller
	RUNTIME DESTINATION ${EXE_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${AR_DIR}
)

install(
	DIRECTORY ${ufprog_controller_SOURCE_DIR}/include/
	DESTINATION include
	FILES_MATCHING PATTERN "*.h*"
)
