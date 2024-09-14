cmake_minimum_required(VERSION 3.13)

project(ufprog_spi_nand)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(ufprog_spi_nand_src
	spi-nand.c
	init.c
	ecc.c
	otp.c
	part.c
	vendor.c
	ext_id.c
)

set(ufprog_spi_nand_vendor_src
	vendor-alliance-memory.c
	vendor-ato.c
	vendor-corestorage.c
	vendor-dosilicon.c
	vendor-esmt.c
	vendor-etron.c
	vendor-fidelix.c
	vendor-foresee.c
	vendor-fudanmicro.c
	vendor-gigadevice.c
	vendor-heyangtek.c
	vendor-issi.c
	vendor-macronix.c
	vendor-micron.c
	vendor-mk.c
	vendor-paragon.c
	vendor-toshiba.c
	vendor-winbond.c
	vendor-xtx.c
	vendor-zetta.c
)

add_library(ufprog_spi_nand SHARED ${ufprog_spi_nand_src} ${ufprog_spi_nand_vendor_src} ufprog-spi-nand.def)
target_link_libraries(ufprog_spi_nand PRIVATE ufprog_common ufprog_spi ufprog_nand_core)
target_compile_definitions(ufprog_spi_nand PRIVATE UFP_MODULE_NAME=\"SPI-NAND\")
set_target_properties(ufprog_spi_nand PROPERTIES OUTPUT_NAME "ufprog-spi-nand")

include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_common_SOURCE_DIR}/internal)
include_directories(${ufprog_controller_SOURCE_DIR}/include)
include_directories(${ufprog_spi_SOURCE_DIR}/include)
include_directories(${ufprog_nand_core_SOURCE_DIR}/include)
include_directories(${ufprog_nand_core_SOURCE_DIR}/internal)
include_directories(${ufprog_spi_nand_SOURCE_DIR}/include)

if(WIN32 OR MINGW OR BUILD_PORTABLE)
	set_target_properties(ufprog_spi_nand PROPERTIES PREFIX "")
else()
	set_target_properties(ufprog_spi_nand PROPERTIES
		VERSION ${UFPROG_VERSION}
		SOVERSION 1
	)
endif()

install(TARGETS ufprog_spi_nand
	RUNTIME DESTINATION ${EXE_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${AR_DIR}
)

install(
	DIRECTORY ${ufprog_spi_nand_SOURCE_DIR}/include/
	DESTINATION include
	FILES_MATCHING PATTERN "*.h*"
)

install(
	FILES spi-nand-ids.json
	DESTINATION ${DATA_DIR}
)
