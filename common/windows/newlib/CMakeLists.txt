cmake_minimum_required(VERSION 3.13)

project(win32_newlib)

set(win32_newlib_src
	strcasestr.c
	strlcat.c
	strlcpy.c
	strndup.c
	wgetdelim.c
)

add_library(win32_newlib STATIC ${win32_newlib_src})

include_directories(${ufprog_common_SOURCE_DIR}/include)
