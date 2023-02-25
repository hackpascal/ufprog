cmake_minimum_required(VERSION 3.13)

project(ufprog_spi)

set(ufprog_spi_src
	spi.c
)

add_library(ufprog_spi SHARED ${ufprog_spi_src} ufprog-spi.def)
target_link_libraries(ufprog_spi PRIVATE ufprog_common ufprog_controller)
target_compile_definitions(ufprog_spi PRIVATE UFP_MODULE_NAME=\"SPI\")
set_target_properties(ufprog_spi PROPERTIES OUTPUT_NAME "ufprog-spi")

include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_controller_SOURCE_DIR}/include)
include_directories(${ufprog_spi_SOURCE_DIR}/include)

if(WIN32 OR MINGW OR BUILD_PORTABLE)
	set_target_properties(ufprog_spi PROPERTIES PREFIX "")
else()
	set_target_properties(ufprog_spi PROPERTIES
		VERSION ${UFPROG_VERSION}
		SOVERSION 1
	)
endif()

install(TARGETS ufprog_spi
	RUNTIME DESTINATION ${EXE_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${AR_DIR}
)

install(
	DIRECTORY ${ufprog_spi_SOURCE_DIR}/include/
	DESTINATION include
	FILES_MATCHING PATTERN "*.h*"
)
