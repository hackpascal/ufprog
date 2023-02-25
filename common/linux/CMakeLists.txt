cmake_minimum_required(VERSION 3.13)

project(os_linux)

set(os_linux_src
	fsop.c
	os.c
)

add_library(os_linux STATIC ${os_linux_src})

target_compile_definitions(os_linux PRIVATE INSTALL_PREFIX=\"${CMAKE_INSTALL_PREFIX}\")
target_link_libraries(os_linux dl)
add_link_options(-pthread)

include_directories(${ufprog_common_SOURCE_DIR}/include)

