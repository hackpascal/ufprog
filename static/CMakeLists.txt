cmake_minimum_required(VERSION 3.13)

project(ufprog_static)

install(
	DIRECTORY ${ufprog_static_SOURCE_DIR}/examples/
	DESTINATION ${DATA_DIR}/examples
)
