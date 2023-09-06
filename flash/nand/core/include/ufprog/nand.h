/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Generic NAND flash support
 */
#pragma once

#ifndef _UFPROG_NAND_H_
#define _UFPROG_NAND_H_

#include <stdint.h>
#include <ufprog/common.h>
#include <ufprog/ecc.h>

EXTERN_C_BEGIN

#define NAND_ID_MAX_LEN				8
#define NAND_VENDOR_MODEL_LEN			128

#define NAND_OTP_PAGE_UID			0
#define NAND_OTP_PAGE_PARAM_PAGE		1
#define NAND_OTP_PAGE_OTP			2

#define NAND_DEFAULT_UID_LEN			16
#define NAND_DEFAULT_UID_REPEATS		16

#define NAND_READ_F_IGNORE_IO_ERROR		BIT(0)
#define NAND_READ_F_IGNORE_ECC_ERROR		BIT(1)

#define PAGE_FILL_F_FILL_NON_DATA_FF		BIT(0)
#define PAGE_FILL_F_FILL_OOB			BIT(1)
#define PAGE_FILL_F_FILL_UNPROTECTED_OOB	BIT(2)
#define PAGE_FILL_F_FILL_UNUSED			BIT(3)
#define PAGE_FILL_F_FILL_ECC_PARITY		BIT(4)
#define PAGE_FILL_F_SRC_SKIP_NON_DATA		BIT(5)

struct nand_chip;

struct nand_id {
	uint8_t id[NAND_ID_MAX_LEN];
	uint32_t len;
};

struct nand_memorg {
	uint32_t num_chips;
	uint32_t luns_per_cs;
	uint32_t blocks_per_lun;
	uint32_t pages_per_block;
	uint32_t page_size;
	uint32_t oob_size;

	/* Not in calculation */
	uint32_t planes_per_lun;
};

struct nand_memaux_info {
	uint64_t size;
	uint32_t chip_shift;
	uint64_t chip_mask;
	uint32_t lun_shift;
	uint64_t lun_mask;
	uint32_t block_size;
	uint32_t oob_block_size;
	uint32_t block_shift;
	uint32_t block_mask;
	uint32_t block_count;
	uint32_t oob_page_size;
	uint32_t page_shift;
	uint32_t page_mask;
	uint32_t page_count;
	uint32_t pages_per_block_shift;
	uint32_t pages_per_block_mask;
};

struct nand_info {
	const char *model;
	const char *vendor;

	uint16_t bus_width;
	uint16_t bits_per_cell;
	uint32_t nops;
	uint32_t uid_length;
	uint32_t otp_pages;
	struct nand_id id;
	struct nand_ecc_config ecc_req;
	struct nand_memorg memorg;
	struct nand_memaux_info maux;
};

int UFPROG_API ufprog_nand_check_buf_bitflips(const void *buf, size_t len, uint32_t bitflips,
					      uint32_t bitflips_threshold);
int UFPROG_API ufprog_nand_check_buf_bitflips_by_bits(const void *buf, size_t bits, uint32_t bitflips,
						      uint32_t bitflips_threshold);

void UFPROG_API ufprog_nand_compute_id_len(struct nand_id *id);

const char *UFPROG_API ufprog_nand_cell_type(struct nand_chip *nand);
ufprog_status UFPROG_API ufprog_nand_info(struct nand_chip *nand, struct nand_info *retinfo);

ufprog_status UFPROG_API ufprog_nand_select_die(struct nand_chip *nand, uint32_t ce, uint32_t lun);

ufprog_status UFPROG_API ufprog_nand_read_page(struct nand_chip *nand, uint32_t page, void *buf, ufprog_bool raw);
ufprog_status UFPROG_API ufprog_nand_read_pages(struct nand_chip *nand, uint32_t page, uint32_t count, void *buf,
						ufprog_bool raw, uint32_t flags, uint32_t *retcount);
ufprog_status UFPROG_API ufprog_nand_write_page(struct nand_chip *nand, uint32_t page, const void *buf,
						ufprog_bool raw);
ufprog_status UFPROG_API ufprog_nand_write_pages(struct nand_chip *nand, uint32_t page, uint32_t count, const void *buf,
						 ufprog_bool raw, ufprog_bool ignore_error, uint32_t *retcount);
ufprog_status UFPROG_API ufprog_nand_erase_block(struct nand_chip *nand, uint32_t page);

ufprog_status UFPROG_API ufprog_nand_read_uid(struct nand_chip *nand, void *data, uint32_t *retlen);

ufprog_status UFPROG_API ufprog_nand_otp_read(struct nand_chip *nand, uint32_t index, void *buf, ufprog_bool raw);
ufprog_status UFPROG_API ufprog_nand_otp_write(struct nand_chip *nand, uint32_t index, const void *buf,
					       ufprog_bool raw);
ufprog_status UFPROG_API ufprog_nand_otp_lock(struct nand_chip *nand);
ufprog_status UFPROG_API ufprog_nand_otp_locked(struct nand_chip *nand, ufprog_bool *retlocked);

ufprog_status UFPROG_API ufprog_nand_convert_page_format(struct nand_chip *nand, const void *buf, void *out,
							 ufprog_bool from_canonical);

ufprog_status UFPROG_API ufprog_nand_check_bbm(struct nand_chip *nand, const struct nand_bbm_config *bbmcfg,
					       uint32_t page);
ufprog_status UFPROG_API ufprog_nand_write_bbm(struct nand_chip *nand, const struct nand_bbm_config *bbmcfg,
					       uint32_t page);

ufprog_status UFPROG_API ufprog_nand_checkbad(struct nand_chip *nand, const struct nand_bbm_config *bbmcfg,
					      uint32_t block);
ufprog_status UFPROG_API ufprog_nand_markbad(struct nand_chip *nand, const struct nand_bbm_config *bbmcfg,
					     uint32_t block);

ufprog_bool UFPROG_API ufprog_nand_bbm_add_page(struct nand_chip *nand, struct nand_bbm_page_cfg *cfg,
						uint32_t page);
ufprog_bool UFPROG_API ufprog_nand_bbm_add_check_pos(struct nand_chip *nand, struct nand_bbm_check_cfg *cfg,
						     uint32_t pos);
ufprog_bool UFPROG_API ufprog_nand_bbm_add_mark_pos(struct nand_chip *nand, struct nand_bbm_mark_cfg *cfg,
						    uint32_t pos);

ufprog_status UFPROG_API ufprog_nand_set_ecc(struct nand_chip *nand, struct ufprog_nand_ecc_chip *ecc);
struct ufprog_nand_ecc_chip *UFPROG_API ufprog_nand_get_ecc(struct nand_chip *nand);
struct ufprog_nand_ecc_chip *UFPROG_API ufprog_nand_default_ecc(struct nand_chip *nand);
ufprog_status UFPROG_API ufprog_nand_get_bbm_config(struct nand_chip *nand, struct nand_bbm_config *ret_bbmcfg);

ufprog_status UFPROG_API ufprog_nand_generate_page_layout(struct nand_chip *nand, struct nand_page_layout **out_layout);
void UFPROG_API ufprog_nand_free_page_layout(struct nand_page_layout *layout);
uint32_t UFPROG_API ufprog_nand_page_layout_to_map(const struct nand_page_layout *layout, uint8_t *map);
uint32_t UFPROG_API ufprog_nand_fill_page_by_layout(const struct nand_page_layout *layout, void *dst, const void *src,
						    uint32_t count, uint32_t flags);

ufprog_status UFPROG_API ufprog_nand_torture_block(struct nand_chip *nand, uint32_t block);

static inline uint64_t nand_flash_compute_chip_blocks(const struct nand_memorg *memorg)
{
	return (uint64_t)memorg->luns_per_cs * (uint64_t)memorg->blocks_per_lun;
}

static inline uint64_t nand_flash_compute_blocks(const struct nand_memorg *memorg)
{
	return (uint64_t)memorg->num_chips * nand_flash_compute_chip_blocks(memorg);
}

static inline uint64_t nand_flash_compute_chip_size(const struct nand_memorg *memorg)
{
	return nand_flash_compute_chip_blocks(memorg) * (uint64_t)memorg->pages_per_block * (uint64_t)memorg->page_size;
}

static inline uint64_t nand_flash_compute_size(const struct nand_memorg *memorg)
{
	return nand_flash_compute_blocks(memorg) * (uint64_t)memorg->pages_per_block * (uint64_t)memorg->page_size;
}

EXTERN_C_END

#endif /* _UFPROG_NAND_H_ */
