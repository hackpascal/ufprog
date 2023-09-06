cmake_minimum_required(VERSION 3.0)

project(ufsnandprog)

set(ufsnandprog_common_src
	ufsnand-common.c
)

if(NOT (WIN32 OR MINGW))
	add_link_options(-Wl,--rpath=.,--disable-new-dtags)
endif()

add_executable(ufsnandprog ufsnandprog.c ${ufsnandprog_common_src})
target_compile_definitions(ufsnandprog PRIVATE UFP_VERSION=\"${UFPROG_VERSION_MAJOR}.${UFPROG_VERSION_MINOR}\")
target_link_libraries(ufsnandprog PRIVATE ufprog_spi_nand ufprog_nand_core ufprog_spi ufprog_common)

add_executable(ufsnandtest ufsnandtest.c ${ufsnandprog_common_src})
target_compile_definitions(ufsnandtest PRIVATE UFP_VERSION=\"${UFPROG_VERSION_MAJOR}.${UFPROG_VERSION_MINOR}\")
target_link_libraries(ufsnandtest PRIVATE ufprog_spi_nand ufprog_nand_core ufprog_spi ufprog_common)

include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_controller_SOURCE_DIR}/include)
include_directories(${ufprog_spi_SOURCE_DIR}/include)
include_directories(${ufprog_nand_core_SOURCE_DIR}/include)
include_directories(${ufprog_spi_nand_SOURCE_DIR}/include)

install(TARGETS ufsnandprog ufsnandtest
	RUNTIME DESTINATION ${EXE_DIR}
)
