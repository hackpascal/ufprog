cmake_minimum_required(VERSION 3.13)

project(os_win32)

add_subdirectory(newlib)

set(os_win32_src
	fsop.c
	console_log.c
	os.c
	entry.c
	asprintf.c
)

add_library(os_win32 STATIC ${os_win32_src})
target_link_libraries(os_win32 shell32 win32_newlib)

include_directories(${ufprog_common_SOURCE_DIR}/include)
