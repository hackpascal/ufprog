/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Generic NAND parameter page definitions
 */
#pragma once

#ifndef _UFPROG_NAND_PARAM_PAGE_H_
#define _UFPROG_NAND_PARAM_PAGE_H_

#include <stdbool.h>
#include <ufprog/common.h>

EXTERN_C_BEGIN

struct nand_memorg;

#define PARAM_PAGE_MIN_COUNT				3

#define PP_SIGNATURE_OFFS				0
#define PP_SIGNATURE_LEN				4

#define PP_MANUF_OFFS					32
#define PP_MANUF_LEN					12

#define PP_MODEL_OFFS					44
#define PP_MODEL_LEN					20

#define PP_PAGE_SIZE_OFFS				80
#define PP_PAGE_SIZE_LEN				1

#define PP_SPARE_SIZE_OFFS				84
#define PP_SPARE_SIZE_LEN				2

#define PP_PAGES_PER_BLOCK_OFFS				92
#define PP_PAGES_PER_BLOCK_LEN				1

#define PP_BLOCKS_PER_LUN_OFFS				96
#define PP_BLOCKS_PER_LUN_LEN				1

#define PP_LUNS_PER_CE_OFFS				100
#define PP_LUNS_PER_CE_LEN				1

#define PP_ADDR_CYCLES_OFFS				101
#define PP_ADDR_CYCLES_LEN				1
#define PP_ADDR_CYCLES_MASK				0xf
#define PP_ADDR_CYCLES_COL_SHIFT			4
#define PP_ADDR_CYCLES_ROW_SHIFT			0

#define PP_BITS_PER_CELL_OFFS				102
#define PP_BITS_PER_CELL_LEN				1

#define PP_ECC_INFO_BITS_CORRECTABILITY_ROFFS		0
#define PP_ECC_INFO_BITS_CORRECTABILITY_LEN		1
#define PP_ECC_INFO_CODEWORD_SIZE_ROFFS			1
#define PP_ECC_INFO_CODEWORD_SIZE_LEN			1
#define PP_ECC_INFO_MAX_BAD_BLOCKS_PER_LUN_ROFFS	2
#define PP_ECC_INFO_MAX_BAD_BLOCKS_PER_LUN_LEN		2
#define PP_ECC_INFO_BLOCK_ENDURANCE_ROFFS		4
#define PP_ECC_INFO_BLOCK_ENDURANCE_LEN			2

#define PP_CRC_VAL_LEN					2
#define PP_CRC_BASE					0x4f4e

ufprog_bool UFPROG_API ufprog_pp_check_recover(void *pp, uint16_t crc_base, uint32_t len, uint32_t total_len,
					       uint32_t signature);

uint16_t UFPROG_API ufprog_pp_calc_crc(uint16_t crc, const void *pp, uint32_t len);

static inline uint8_t ufprog_pp_read_u8(const void *pp, uint32_t offs)
{
	const uint8_t *data = (const uint8_t *)pp + offs;

	if (!pp)
		return 0;

	return data[0];
}

static inline uint16_t ufprog_pp_read_u16(const void *pp, uint32_t offs)
{
	const uint8_t *data = (const uint8_t *)pp + offs;

	if (!pp)
		return 0;

	return data[0] | ((uint16_t)data[1] << 8);
}

static inline uint32_t ufprog_pp_read_u32(const void *pp, uint32_t offs)
{
	const uint8_t *data = (const uint8_t *)pp + offs;

	if (!pp)
		return 0;

	return data[0] | ((uint16_t)data[1] << 8) | ((uint16_t)data[2] << 16) | ((uint16_t)data[3] << 24);
}

uint32_t UFPROG_API ufprog_pp_read_str(const void *pp, char *buf, size_t size, uint32_t offs, uint32_t len);

ufprog_bool UFPROG_API ufprog_pp_resolve_memorg(const void *pp, struct nand_memorg *memorg);

EXTERN_C_END

#endif /* _UFPROG_NAND_PARAM_PAGE_H_ */
