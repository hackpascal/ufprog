cmake_minimum_required(VERSION 3.13)

project(serprog)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(${ufprog_common_SOURCE_DIR}/include)
include_directories(${ufprog_controller_SOURCE_DIR}/include)

add_library(serprog SHARED serprog.c serprog-spi.c serprog.def)
target_link_libraries(serprog PRIVATE ufprog_common ufprog_controller)
target_compile_definitions(serprog PRIVATE UFP_MODULE_NAME=\"serprog\")
set_target_properties(serprog PROPERTIES PREFIX "" OUTPUT_NAME "serprog")

install(TARGETS serprog
	RUNTIME DESTINATION ${CONTROLLER_DRIVER_DIR}
	LIBRARY DESTINATION ${CONTROLLER_DRIVER_DIR}
)
