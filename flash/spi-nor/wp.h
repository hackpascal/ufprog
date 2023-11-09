/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash write protection operations
 */
#pragma once

#ifndef _UFPROG_SPI_NOR_WP_H_
#define _UFPROG_SPI_NOR_WP_H_

#include <ufprog/bits.h>
#include <ufprog/sizes.h>
#include "regs.h"

#define SNOR_WPF_LOWER			BIT(0)	/* Lower part */
#define SNOR_WPF_CMP			BIT(1)	/* Complementary range */
#define SNOR_WPF_CMP_FULL		BIT(2)	/* Complementary range is full if original range is also full */

enum snor_wp_range_scale_type {
	SNOR_WPR_NONE,		/* No protection */
	SNOR_WPR_ALL,		/* Full protection */
	SNOR_WPR_LSHIFT,	/* Granularity left shift */
	SNOR_WPR_RSHIFT,	/* Chip size right shift */
	SNOR_WPR_MULTI,		/* Granularity multiplication */

	__MAX_SNOR_WPR_SCALE_TYPE
};

struct spi_nor_wp_range {
	uint32_t sr_val;

	enum snor_wp_range_scale_type type;
	uint32_t granularity;
	uint32_t scale;
	uint32_t flags;
};

#define SNOR_WP_NONE(_val)			{ .sr_val = (_val), .type = SNOR_WPR_NONE }
#define SNOR_WP_ALL(_val)			{ .sr_val = (_val), .type = SNOR_WPR_ALL }

#define SNOR_WP_BP_BLK(_val, _lshift, _flags)	\
	{ .sr_val = (_val), .scale = (_lshift), .granularity = SZ_64K, .type = SNOR_WPR_LSHIFT, .flags = (_flags) }

#define SNOR_WP_BP_LO(_val, _lshift)		SNOR_WP_BP_BLK(_val, _lshift, SNOR_WPF_LOWER)
#define SNOR_WP_BP_UP(_val, _lshift)		SNOR_WP_BP_BLK(_val, _lshift, 0)
#define SNOR_WP_BP_CMP_LO(_val, _lshift)	SNOR_WP_BP_BLK(_val, _lshift, SNOR_WPF_CMP)
#define SNOR_WP_BP_CMP_UP(_val, _lshift)	SNOR_WP_BP_BLK(_val, _lshift, SNOR_WPF_LOWER | SNOR_WPF_CMP)
#define SNOR_WP_BP_CMPF_LO(_val, _lshift)	SNOR_WP_BP_BLK(_val, _lshift, SNOR_WPF_CMP | SNOR_WPF_CMP_FULL)
#define SNOR_WP_BP_CMPF_UP(_val, _lshift)	SNOR_WP_BP_BLK(_val, _lshift, SNOR_WPF_LOWER | SNOR_WPF_CMP | \
									      SNOR_WPF_CMP_FULL)

#define SNOR_WP_BP_SEC(_val, _lshift, _flags)	\
	{ .sr_val = (_val), .scale = (_lshift), .granularity = SZ_4K, .type = SNOR_WPR_LSHIFT, .flags = (_flags) }

#define SNOR_WP_SP_LO(_val, _lshift)		SNOR_WP_BP_SEC(_val, _lshift, SNOR_WPF_LOWER)
#define SNOR_WP_SP_UP(_val, _lshift)		SNOR_WP_BP_SEC(_val, _lshift, 0)
#define SNOR_WP_SP_CMP_LO(_val, _lshift)	SNOR_WP_BP_SEC(_val, _lshift, SNOR_WPF_CMP)
#define SNOR_WP_SP_CMP_UP(_val, _lshift)	SNOR_WP_BP_SEC(_val, _lshift, SNOR_WPF_LOWER | SNOR_WPF_CMP)
#define SNOR_WP_SP_CMPF_LO(_val, _lshift)	SNOR_WP_BP_SEC(_val, _lshift, SNOR_WPF_CMP | SNOR_WPF_CMP_FULL)
#define SNOR_WP_SP_CMPF_UP(_val, _lshift)	SNOR_WP_BP_SEC(_val, _lshift, SNOR_WPF_LOWER | SNOR_WPF_CMP | \
									      SNOR_WPF_CMP_FULL)

#define SNOR_WP_BP_RATIO(_val, _lshift, _flags)	\
	{ .sr_val = (_val), .scale = (_lshift), .type = SNOR_WPR_RSHIFT, .flags = (_flags) }

#define SNOR_WP_RP_LO(_val, _rshift)		SNOR_WP_BP_RATIO(_val, _rshift, SNOR_WPF_LOWER)
#define SNOR_WP_RP_UP(_val, _rshift)		SNOR_WP_BP_RATIO(_val, _rshift, 0)
#define SNOR_WP_RP_CMP_LO(_val, _rshift)	SNOR_WP_BP_RATIO(_val, _rshift, SNOR_WPF_CMP)
#define SNOR_WP_RP_CMP_UP(_val, _rshift)	SNOR_WP_BP_RATIO(_val, _rshift, SNOR_WPF_LOWER | SNOR_WPF_CMP)
#define SNOR_WP_RP_CMPF_LO(_val, _rshift)	SNOR_WP_BP_RATIO(_val, _rshift, SNOR_WPF_CMP | SNOR_WPF_CMP_FULL)
#define SNOR_WP_RP_CMPF_UP(_val, _rshift)	SNOR_WP_BP_RATIO(_val, _rshift, SNOR_WPF_LOWER | SNOR_WPF_CMP | \
									      SNOR_WPF_CMP_FULL)

struct spi_nor_wp_info {
	const struct spi_nor_reg_access *access;

	uint32_t num;
	uint32_t sr_mask;
	struct spi_nor_wp_range ranges[];
};

#define SNOR_WP_BP(_access, _mask, ...)									\
	{ .access = (_access), .sr_mask = (_mask), .ranges = { __VA_ARGS__ },				\
	  .num = sizeof((struct spi_nor_wp_range[]){ __VA_ARGS__ }) / sizeof(struct spi_nor_wp_range) }

extern const struct spi_nor_wp_info wpr_2bp_all;

extern const struct spi_nor_wp_info wpr_2bp_up;
extern const struct spi_nor_wp_info wpr_2bp_up_ratio;
extern const struct spi_nor_wp_info wpr_2bp_lo;
extern const struct spi_nor_wp_info wpr_2bp_lo_ratio;

extern const struct spi_nor_wp_info wpr_2bp_tb;

extern const struct spi_nor_wp_info wpr_3bp_up;
extern const struct spi_nor_wp_info wpr_3bp_up_ratio;

extern const struct spi_nor_wp_info wpr_3bp_lo;
extern const struct spi_nor_wp_info wpr_3bp_lo_ratio;

extern const struct spi_nor_wp_info wpr_3bp_tb;
extern const struct spi_nor_wp_info wpr_3bp_tb_ratio;

extern const struct spi_nor_wp_info wpr_3bp_tb_sec;
extern const struct spi_nor_wp_info wpr_3bp_tb_sec_ratio;

extern const struct spi_nor_wp_info wpr_3bp_tb_sec_cmp;
extern const struct spi_nor_wp_info wpr_3bp_tb_sec_cmp_ratio;

extern const struct spi_nor_wp_info wpr_4bp_up;
extern const struct spi_nor_wp_info wpr_4bp_lo;

extern const struct spi_nor_wp_info wpr_4bp_tb;
extern const struct spi_nor_wp_info wpr_4bp_tb_cmp;

struct spi_nor_wp_info *wp_bp_info_copy(const struct spi_nor_wp_info *src);

#endif /* _UFPROG_SPI_NOR_WP_H_ */
