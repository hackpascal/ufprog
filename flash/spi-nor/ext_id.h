/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR external flash table processing
 */
#pragma once

#ifndef _UFPROG_SPI_NOR_EXT_ID_H_
#define _UFPROG_SPI_NOR_EXT_ID_H_

#include <ufprog/common.h>

struct spi_nor_part_flag_enum_info {
	uint32_t val;
	const char *name;
};

ufprog_status spi_nor_load_ext_id_list(void);

#endif /* _UFPROG_SPI_NOR_EXT_ID_H_ */
