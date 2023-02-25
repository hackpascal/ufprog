cmake_minimum_required(VERSION 3.0)

project(ufsnorprog)

set(ufsnorprog_common_src
	ufsnor-common.c
)

if(NOT (WIN32 OR MINGW))
	add_link_options(-Wl,--rpath=.,--disable-new-dtags)
endif()

add_executable(ufsnorprog ufsnorprog.c ${ufsnorprog_common_src})
target_compile_definitions(ufsnorprog PRIVATE UFP_VERSION=\"${UFPROG_VERSION_MAJOR}.${UFPROG_VERSION_MINOR}\")
target_link_libraries(ufsnorprog PRIVATE ufprog_spi_nor ufprog_spi ufprog_common)

add_executable(ufsnortest ufsnortest.c ${ufsnorprog_common_src})
target_compile_definitions(ufsnortest PRIVATE UFP_VERSION=\"${UFPROG_VERSION_MAJOR}.${UFPROG_VERSION_MINOR}\")
target_link_libraries(ufsnortest PRIVATE ufprog_spi_nor ufprog_spi ufprog_common)

include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_controller_SOURCE_DIR}/include)
include_directories(${ufprog_spi_SOURCE_DIR}/include)
include_directories(${ufprog_spi_nor_SOURCE_DIR}/include)

install(TARGETS ufsnorprog ufsnortest
	RUNTIME DESTINATION ${EXE_DIR}
)
