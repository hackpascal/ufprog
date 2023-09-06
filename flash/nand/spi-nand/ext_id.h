/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND external flash table processing
 */
#pragma once

#ifndef _UFPROG_SPI_NAND_EXT_ID_H_
#define _UFPROG_SPI_NAND_EXT_ID_H_

#include <ufprog/common.h>

#define SPI_NAND_EXT_PART_FREE_RD_OPCODES		BIT(0)
#define SPI_NAND_EXT_PART_FREE_PL_OPCODES		BIT(1)
#define SPI_NAND_EXT_PART_FREE_PAGE_LAYOUT		BIT(2)
#define SPI_NAND_EXT_PART_FREE_MEMORG			BIT(3)

struct spi_nand_part_flag_enum_info {
	uint32_t val;
	const char *name;
};

ufprog_status spi_nand_load_ext_id_list(void);

#endif /* _UFPROG_SPI_NAND_EXT_ID_H_ */
