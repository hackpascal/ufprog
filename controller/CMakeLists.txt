cmake_minimum_required(VERSION 3.13)

set(CONTROLLER_DRIVER_DIR ${PLUGIN_DIR}/controller)

add_subdirectory(core)
add_subdirectory(ftdi)
add_subdirectory(wch)
add_subdirectory(serprog)
