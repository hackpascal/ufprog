/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Generic NAND flash internal definitions
 */
#pragma once

#ifndef _UFPROG_NAND_CORE_H_
#define _UFPROG_NAND_CORE_H_

#include <stdbool.h>
#include <ufprog/nand.h>

EXTERN_C_BEGIN

#define NAND_ID(...) { .id = { __VA_ARGS__ }, .len = sizeof((uint8_t[]){ __VA_ARGS__ }) }

#define NAND_ECC_INFO(_ss, _sps) { .step_size = (_ss), .strength_per_step = (_sps) }
#define NAND_ECC_REQ	NAND_ECC_INFO

#define NAND_MEMORG(_ps, _ss, _ppb, _bpl, _lpc, _nc, _ppl) \
	{ .page_size = (_ps), .oob_size = (_ss), .pages_per_block = (_ppb), \
	  .blocks_per_lun = (_bpl), .luns_per_cs = (_lpc), .num_chips = (_nc), \
	  .planes_per_lun = (_ppl) }

#define NAND_MEMORG_1P(_ps, _ss, _ppb, _bpl, _lpc, _nc) \
	NAND_MEMORG(_ps, _ss, _ppb, _bpl, _lpc, _nc, 1)

#define NAND_MEMORG_2P(_ps, _ss, _ppb, _bpl, _lpc, _nc) \
	NAND_MEMORG(_ps, _ss, _ppb, _bpl, _lpc, _nc, 2)

#define NAND_NOPS(_nops)			.nops = (_nops)

struct nand_otp_info {
	uint32_t start_index;
	uint32_t count;
};

#define NAND_OTP_INFO(_info)			.otp = (_info)

struct nand_flash_otp_ops {
	ufprog_status (*read)(struct nand_chip *nand, uint32_t index, uint32_t column, uint32_t len, void *data);
	ufprog_status (*write)(struct nand_chip *nand, uint32_t index, uint32_t column, uint32_t len, const void *data);
	ufprog_status (*lock)(struct nand_chip *nand);
	ufprog_status (*locked)(struct nand_chip *nand, ufprog_bool *retlocked);
};

#define NAND_OTP_OPS(_ops)			.otp_ops = (_ops)

struct nand_chip {
	const char *model;
	const char *vendor;

	uint16_t bus_width;
	uint16_t bits_per_cell;
	uint32_t nops;
	struct nand_id id;
	struct nand_memorg memorg;
	struct nand_ecc_config ecc_req;
	const struct nand_otp_info *otp;

	struct ufprog_nand_ecc_chip *default_ecc;
	struct nand_bbm_config default_bbm_config;

	ufprog_status (*select_die)(struct nand_chip *nand, uint32_t ce, uint32_t lun);

	ufprog_status (*read_page)(struct nand_chip *nand, uint32_t page, uint32_t column, uint32_t len,
				   void *buf);
	ufprog_status (*read_pages)(struct nand_chip *nand, uint32_t page, uint32_t count, void *buf,
				    uint32_t flags, uint32_t *retcount);
	ufprog_status (*write_page)(struct nand_chip *nand, uint32_t page, uint32_t column, uint32_t len,
				    const void *buf);
	ufprog_status (*write_pages)(struct nand_chip *nand, uint32_t page, uint32_t count, const void *buf,
				    bool ignore_error, uint32_t *retcount);
	ufprog_status (*erase_block)(struct nand_chip *nand, uint32_t page);

	ufprog_status (*read_uid)(struct nand_chip *nand, void *data, uint32_t *retlen);

	const struct nand_flash_otp_ops *otp_ops;

	/* provided by controller */
	void *page_cache[2];

	/* dynamic fields */
	struct ufprog_nand_ecc_chip *ecc;
	struct nand_bbm_config bbm_config;
	uint32_t ecc_steps;

	/* private fields */
	struct nand_memaux_info maux;
};

void UFPROG_API ufprog_nand_update_param(struct nand_chip *nand);

void UFPROG_API ufprog_nand_print_ecc_result(struct nand_chip *nand, uint32_t page);

EXTERN_C_END

#endif /* _UFPROG_NAND_CORE_H_ */
