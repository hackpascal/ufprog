cmake_minimum_required(VERSION 3.13)

project(ufprog_nand_core)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(ufprog_nand_core_src
	init.c
	nand.c
	ecc-driver.c
	ecc.c
	bbt-driver.c
	bbt.c
	bbt-ram.c
	ftl-driver.c
	ftl.c
	ftl-basic.c
	param-page.c
	onfi.c
)

add_library(ufprog_nand_core SHARED ${ufprog_nand_core_src} ufprog-nand-core.def)
target_link_libraries(ufprog_nand_core PRIVATE ufprog_common)
target_compile_definitions(ufprog_nand_core PRIVATE UFP_MODULE_NAME=\"NAND-CORE\")
set_target_properties(ufprog_nand_core PROPERTIES OUTPUT_NAME "ufprog-nand-core")

include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_common_SOURCE_DIR}/internal)
include_directories(${ufprog_nand_core_SOURCE_DIR}/include)

if(WIN32 OR MINGW OR BUILD_PORTABLE)
	set_target_properties(ufprog_nand_core PROPERTIES PREFIX "")
else()
	set_target_properties(ufprog_nand_core PROPERTIES
		VERSION ${UFPROG_VERSION}
		SOVERSION 1
	)
endif()

install(TARGETS ufprog_nand_core
	RUNTIME DESTINATION ${EXE_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${AR_DIR}
)

install(
	DIRECTORY ${ufprog_nand_core_SOURCE_DIR}/include/
	DESTINATION include
	FILES_MATCHING PATTERN "*.h*"
)
