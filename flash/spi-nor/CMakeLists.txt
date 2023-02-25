cmake_minimum_required(VERSION 3.13)

project(ufprog_spi_nor)

set(ufprog_spi_nor_src
	spi-nor.c
	init.c
	part.c
	sfdp.c
	otp.c
	wp.c
	regs.c
	vendor.c
	ext_id.c
)

set(ufprog_spi_nor_vendor_src
	vendor-gigadevice.c
	vendor-intel.c
	vendor-issi.c
	vendor-sst.c
	vendor-winbond.c
)

add_library(ufprog_spi_nor SHARED ${ufprog_spi_nor_src} ${ufprog_spi_nor_vendor_src} ufprog-spi-nor.def)
target_link_libraries(ufprog_spi_nor PRIVATE ufprog_common ufprog_spi)
target_compile_definitions(ufprog_spi_nor PRIVATE UFP_MODULE_NAME=\"SPI-NOR\")
set_target_properties(ufprog_spi_nor PROPERTIES OUTPUT_NAME "ufprog-spi-nor")

include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_controller_SOURCE_DIR}/include)
include_directories(${ufprog_spi_SOURCE_DIR}/include)
include_directories(${ufprog_spi_nor_SOURCE_DIR}/include)

if(WIN32 OR MINGW OR BUILD_PORTABLE)
	set_target_properties(ufprog_spi_nor PROPERTIES PREFIX "")
else()
	set_target_properties(ufprog_spi_nor PROPERTIES
		VERSION ${UFPROG_VERSION}
		SOVERSION 1
	)
endif()

install(TARGETS ufprog_spi_nor
	RUNTIME DESTINATION ${EXE_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${AR_DIR}
)

install(
	DIRECTORY ${ufprog_spi_nor_SOURCE_DIR}/include/
	DESTINATION include
	FILES_MATCHING PATTERN "*.h*"
)

install(
	FILES spi-nor-ids.json
	DESTINATION ${DATA_DIR}
)
