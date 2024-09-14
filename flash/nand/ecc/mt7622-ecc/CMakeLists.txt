cmake_minimum_required(VERSION 3.13)

project(nand_ecc_mt7622)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(nand_ecc_mt7622 SHARED mt7622-ecc.c mt7622-ecc.def)
target_link_libraries(nand_ecc_mt7622 PRIVATE ufprog_common ufprog_nand_core)
target_compile_definitions(nand_ecc_mt7622 PRIVATE UFP_MODULE_NAME=\"MT7622-ECC\")
set_target_properties(nand_ecc_mt7622 PROPERTIES OUTPUT_NAME "mt7622")

include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_nand_core_SOURCE_DIR}/include)

if(WIN32 OR MINGW OR BUILD_PORTABLE)
	set_target_properties(nand_ecc_mt7622 PROPERTIES PREFIX "")
else()
	set_target_properties(nand_ecc_mt7622 PROPERTIES
		VERSION ${UFPROG_VERSION}
		SOVERSION 1
	)
endif()

install(TARGETS nand_ecc_mt7622
	RUNTIME DESTINATION ${NAND_ECC_DRIVER_DIR}
	LIBRARY DESTINATION ${NAND_ECC_DRIVER_DIR}
)

install(
	FILES presets/mt7629.json
	DESTINATION ${NAND_ECC_DRIVER_DIR}
)
