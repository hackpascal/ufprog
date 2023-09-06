cmake_minimum_required(VERSION 3.13)

project(nand_ftl_nmbm)

if(MSVC)
	add_compile_options(
		/wd4018 /wd4242 /wd4244 /wd4389 /wd4701
	)
endif()

add_library(nand_ftl_nmbm SHARED nmbm.c nmbm-core.c nmbm.def)
target_link_libraries(nand_ftl_nmbm PRIVATE ufprog_common ufprog_nand_core)
target_compile_definitions(nand_ftl_nmbm PRIVATE UFP_MODULE_NAME=\"NMBM\")
set_target_properties(nand_ftl_nmbm PROPERTIES OUTPUT_NAME "nmbm")

include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_nand_core_SOURCE_DIR}/include)
include_directories(${nand_ftl_nmbm_SOURCE_DIR}/include)

if(WIN32 OR MINGW OR BUILD_PORTABLE)
	set_target_properties(nand_ftl_nmbm PROPERTIES PREFIX "")
else()
	set_target_properties(nand_ftl_nmbm PROPERTIES
		VERSION ${UFPROG_VERSION}
		SOVERSION 1
	)
endif()

install(TARGETS nand_ftl_nmbm
	RUNTIME DESTINATION ${NAND_FTL_DRIVER_DIR}
	LIBRARY DESTINATION ${NAND_FTL_DRIVER_DIR}
)

install(
	FILES presets/nmbm-rw.json
	DESTINATION ${NAND_FTL_DRIVER_DIR}
)
