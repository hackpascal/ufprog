cmake_minimum_required(VERSION 3.13)

set(NAND_BBT_DRIVER_DIR ${PLUGIN_DIR}/bbt)
set(NAND_ECC_DRIVER_DIR ${PLUGIN_DIR}/ecc)
set(NAND_FTL_DRIVER_DIR ${PLUGIN_DIR}/ftl)

add_subdirectory(core)
add_subdirectory(bbt)
add_subdirectory(ecc)
add_subdirectory(ftl)
add_subdirectory(spi-nand)
