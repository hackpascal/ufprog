/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash vendor definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NAND_VENDOR_H_
#define _UFPROG_SPI_NAND_VENDOR_H_

#include "part.h"
#include "ext_id.h"

#define SNAND_VENDOR_HEYANGTEK_01		0x01
#define SNAND_VENDOR_XTX			0x0b
#define SNAND_VENDOR_MICRON			0x2c
#define SNAND_VENDOR_ALLIANCE_MEMORY		0x52
#define SNAND_VENDOR_CORESTORAGE		0x6b
#define SNAND_VENDOR_TOSHIBA			0x98
#define SNAND_VENDOR_ATO			0x9b
#define SNAND_VENDOR_ISSI			0x9d
#define SNAND_VENDOR_PARAGON			0xa1
#define SNAND_VENDOR_ZETTA			0xba
#define SNAND_VENDOR_MACRONIX			0xc2
#define SNAND_VENDOR_GIGADEVICE			0xc8
#define SNAND_VENDOR_HEYANGTEK			0xc9
#define SNAND_VENDOR_FORESEE			0xcd
#define SNAND_VENDOR_ETRON			0xd5
#define SNAND_VENDOR_FIDELIX			0xe5
#define SNAND_VENDOR_WINBOND			0xef

struct spi_nand_vendor_ops {
	ufprog_status (*init)(void);
	ufprog_status (*pp_post_init)(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp);
};

struct spi_nand_vendor {
	uint8_t mfr_id;
	const char *id;
	const char *name;

	const struct spi_nand_flash_part *parts;
	uint32_t nparts;

	const struct spi_nand_vendor_ops *ops;
	const struct spi_nand_flash_part_ops *default_part_ops;
	const struct spi_nand_flash_part_fixup *default_part_fixups;
	const struct nand_flash_otp_ops *default_part_otp_ops;

	const struct spi_nand_part_flag_enum_info *vendor_flag_names;
	uint32_t num_vendor_flag_names;
};

struct spi_nand_vendor_part {
	const struct spi_nand_vendor *vendor;
	const struct spi_nand_flash_part *part;
};

extern const struct spi_nand_vendor vendor_alliance_memory;
extern const struct spi_nand_vendor vendor_ato;
extern const struct spi_nand_vendor vendor_corestorage;
extern const struct spi_nand_vendor vendor_dosilicon;
extern const struct spi_nand_vendor vendor_esmt;
extern const struct spi_nand_vendor vendor_etron;
extern const struct spi_nand_vendor vendor_fidelix;
extern const struct spi_nand_vendor vendor_foresee;
extern const struct spi_nand_vendor vendor_fudanmicro;
extern const struct spi_nand_vendor vendor_gigadevice;
extern const struct spi_nand_vendor vendor_heyangtek_01;
extern const struct spi_nand_vendor vendor_heyangtek;
extern const struct spi_nand_vendor vendor_issi;
extern const struct spi_nand_vendor vendor_macronix;
extern const struct spi_nand_vendor vendor_micron;
extern const struct spi_nand_vendor vendor_mk;
extern const struct spi_nand_vendor vendor_paragon;
extern const struct spi_nand_vendor vendor_toshiba;
extern const struct spi_nand_vendor vendor_winbond;
extern const struct spi_nand_vendor vendor_xtx;
extern const struct spi_nand_vendor vendor_zetta;

ufprog_status spi_nand_vendors_init(void);

const struct spi_nand_vendor *spi_nand_find_vendor(uint8_t mfr_id);
const struct spi_nand_vendor *spi_nand_find_vendor_by_id(const char *id);

bool spi_nand_find_vendor_part(enum spi_nand_id_type type, const uint8_t *id, struct spi_nand_vendor_part *retvp);
bool spi_nand_find_vendor_part_by_name(const char *model, struct spi_nand_vendor_part *retvp);

const struct spi_nand_flash_part *spi_nand_vendor_find_part_by_name(const char *model,
								    const struct spi_nand_vendor *vendor);

uint32_t spi_nand_vendor_list_parts(const struct spi_nand_vendor *vendor, const char *match_part,
				   const struct spi_nand_id *match_id, struct spi_nand_probe_part *list, bool no_meta);
uint32_t spi_nand_list_parts(const char *vendorid, const char *match_part, const struct spi_nand_id *match_id,
			     struct spi_nand_probe_part *list);

bool spi_nand_set_ext_vendor_capacity(uint32_t n);
struct spi_nand_vendor *spi_nand_alloc_ext_vendor(void);
bool spi_nand_is_ext_vendor(const struct spi_nand_vendor *vendor);

typedef void (*spi_nand_reset_ext_vendor_cb)(struct spi_nand_vendor *vendor);
void spi_nand_reset_ext_vendors(spi_nand_reset_ext_vendor_cb cb);

#endif /* _UFPROG_SPI_NAND_VENDOR_H_ */
