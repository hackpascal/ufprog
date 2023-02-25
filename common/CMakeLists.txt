cmake_minimum_required(VERSION 3.13)

project(ufprog_common)

if(USE_VCPKG)
find_package(json-c CONFIG REQUIRED)
set(JSON_C_LIB json-c::json-c json-c::json-c-static)
else()
find_package(PkgConfig)

pkg_check_modules(json-c REQUIRED json-c>=0.13)
set(JSON_C_INC ${json-c_INCLUDEDIR})
set(JSON_C_LIB ${json-c_LIBRARIES})
endif()

if(WIN32 OR MINGW)
	add_subdirectory(windows)
	set(OSDEF os_win32)
else()
	add_subdirectory(linux)
	set(OSDEF os_linux)
endif()

set(ufprog_common_src
	config.c
	log.c
	dirs.c
	lookup_table.c
	crc32.c
	misc.c
	init.c
	cmdarg.c
	bitops.c
	hexdump.c
	progbar.c
	buffdiff.c
)

add_library(ufprog_common SHARED ${ufprog_common_src} ufprog-common.def)
target_link_libraries(ufprog_common PRIVATE ${JSON_C_LIB} ${OSDEF})
set_target_properties(ufprog_common PROPERTIES OUTPUT_NAME "ufprog-common")

include_directories(${JSON_C_INC})
include_directories(${ufprog_common_SOURCE_DIR}/include)

if(WIN32 OR MINGW OR BUILD_PORTABLE)
	set_target_properties(ufprog_common PROPERTIES PREFIX "")
	target_compile_definitions(ufprog_common PRIVATE BUILD_PORTABLE)
else()
	set_target_properties(ufprog_common PROPERTIES
		VERSION ${UFPROG_VERSION}
		SOVERSION 1
	)
endif()

install(TARGETS ufprog_common
	RUNTIME DESTINATION ${EXE_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${AR_DIR}
)

install(
	DIRECTORY ${ufprog_common_SOURCE_DIR}/include/
	DESTINATION include
	FILES_MATCHING PATTERN "*.h*"
)
