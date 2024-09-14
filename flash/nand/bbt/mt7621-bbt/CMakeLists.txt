cmake_minimum_required(VERSION 3.13)

project(nand_bbt_mt7621)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(nand_bbt_mt7621 SHARED mt7621-bbt.c mt7621-bbt.def)
target_link_libraries(nand_bbt_mt7621 PRIVATE ufprog_common ufprog_nand_core)
target_compile_definitions(nand_bbt_mt7621 PRIVATE UFP_MODULE_NAME=\"MT7621-BBT\")
set_target_properties(nand_bbt_mt7621 PROPERTIES OUTPUT_NAME "mt7621")

include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_nand_core_SOURCE_DIR}/include)

if(WIN32 OR MINGW OR BUILD_PORTABLE)
	set_target_properties(nand_bbt_mt7621 PROPERTIES PREFIX "")
else()
	set_target_properties(nand_bbt_mt7621 PROPERTIES
		VERSION ${UFPROG_VERSION}
		SOVERSION 1
	)
endif()

install(TARGETS nand_bbt_mt7621
	RUNTIME DESTINATION ${NAND_BBT_DRIVER_DIR}
	LIBRARY DESTINATION ${NAND_BBT_DRIVER_DIR}
)
