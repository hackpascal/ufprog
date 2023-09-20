/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash write protection operations
 */
#pragma once

#ifndef _UFPROG_SPI_NOR_WP_H_
#define _UFPROG_SPI_NOR_WP_H_

#include "regs.h"

enum snor_wp_range_type {
	SNOR_WPR_BLOCK,
	SNOR_WPR_SECTOR,
	SNOR_WPR_RATIO,
	SNOR_WPR_BLOCK_MULTI,
	SNOR_WPR_SECTOR_MULTI,

	__MAX_SNOR_WPR_TYPR
};

struct spi_nor_wp_range {
	uint16_t sr_mask;
	uint16_t sr_val;

	enum snor_wp_range_type type;
	bool lower;
	bool cmp;
	bool whole_cmp;

	union {
		int shift;
		int multi;
	};
};

#define SNOR_WP_BP_BLK(_mask, _val, _lower, _cmp, _lshift)						\
	{ .sr_mask = (_mask), .sr_val = (_val), .lower = (_lower), .cmp = (_cmp), .shift = (_lshift),	\
	  .type = SNOR_WPR_BLOCK }

#define SNOR_WP_BP_LO(_mask, _val, _lshift)		SNOR_WP_BP_BLK(_mask, _val, true, false, _lshift)
#define SNOR_WP_BP_UP(_mask, _val, _lshift)		SNOR_WP_BP_BLK(_mask, _val, false, false, _lshift)
#define SNOR_WP_BP_CMP_LO(_mask, _val, _lshift)		SNOR_WP_BP_BLK(_mask, _val, false, true, _lshift)
#define SNOR_WP_BP_CMP_UP(_mask, _val, _lshift)		SNOR_WP_BP_BLK(_mask, _val, true, true, _lshift)

#define SNOR_WP_BP_SEC(_mask, _val, _lower, _cmp, _lshift)						\
	{ .sr_mask = (_mask), .sr_val = (_val), .lower = (_lower), .cmp = (_cmp), .shift = (_lshift),	\
	  .type = SNOR_WPR_SECTOR }

#define SNOR_WP_SP_LO(_mask, _val, _lshift)		SNOR_WP_BP_SEC(_mask, _val, true, false, _lshift)
#define SNOR_WP_SP_UP(_mask, _val, _lshift)		SNOR_WP_BP_SEC(_mask, _val, false, false, _lshift)
#define SNOR_WP_SP_CMP_LO(_mask, _val, _lshift)		SNOR_WP_BP_SEC(_mask, _val, false, true, _lshift)
#define SNOR_WP_SP_CMP_UP(_mask, _val, _lshift)		SNOR_WP_BP_SEC(_mask, _val, true, true, _lshift)

#define SNOR_WP_BP_RATIO(_mask, _val, _lower, _cmp, _rshift)						\
	{ .sr_mask = (_mask), .sr_val = (_val), .lower = (_lower), .cmp = (_cmp), .shift = (_rshift),	\
	  .type = SNOR_WPR_RATIO }

#define SNOR_WP_RP_LO(_mask, _val, _rshift)		SNOR_WP_BP_RATIO(_mask, _val, true, false, _rshift)
#define SNOR_WP_RP_UP(_mask, _val, _rshift)		SNOR_WP_BP_RATIO(_mask, _val, false, false, _rshift)
#define SNOR_WP_RP_CMP_LO(_mask, _val, _rshift)		SNOR_WP_BP_RATIO(_mask, _val, false, true, _rshift)
#define SNOR_WP_RP_CMP_UP(_mask, _val, _rshift)		SNOR_WP_BP_RATIO(_mask, _val, true, true, _rshift)

struct spi_nor_wp_info {
	const struct spi_nor_reg_access *access;

	uint32_t num;
	struct spi_nor_wp_range ranges[];
};

#define SNOR_WP_BP(_access, ...)									\
	{ .access = (_access), .ranges = { __VA_ARGS__ },						\
	  .num = sizeof((struct spi_nor_wp_range[]){ __VA_ARGS__ }) / sizeof(struct spi_nor_wp_range) }

extern const struct spi_nor_wp_info wpr_2bp_all;

extern const struct spi_nor_wp_info wpr_2bp_tb;

extern const struct spi_nor_wp_info wpr_3bp_tb;
extern const struct spi_nor_wp_info wpr_3bp_tb_ratio;

extern const struct spi_nor_wp_info wpr_3bp_tb_sec;
extern const struct spi_nor_wp_info wpr_3bp_tb_sec_ratio;

extern const struct spi_nor_wp_info wpr_3bp_tb_sec_cmp;
extern const struct spi_nor_wp_info wpr_3bp_tb_sec_cmp_ratio;

extern const struct spi_nor_wp_info wpr_4bp_tb;
extern const struct spi_nor_wp_info wpr_4bp_tb_cmp;

struct spi_nor_wp_info *wp_bp_info_copy(const struct spi_nor_wp_info *src);

#endif /* _UFPROG_SPI_NOR_WP_H_ */
