cmake_minimum_required(VERSION 3.13)

project(os_win32)

set(os_win32_src
	fsop.c
	console_log.c
	os.c
	entry.c
	asprintf.c
)

set(win32_newlib_src
	newlib/strcasestr.c
	newlib/strlcat.c
	newlib/strlcpy.c
	newlib/strndup.c
	newlib/wgetdelim.c
)

add_library(os_win32 STATIC ${os_win32_src} ${win32_newlib_src})
target_link_libraries(os_win32 shell32)

include_directories(${ufprog_common_SOURCE_DIR}/include)
