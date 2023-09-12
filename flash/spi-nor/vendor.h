/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash vendor definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NOR_VENDOR_H_
#define _UFPROG_SPI_NOR_VENDOR_H_

#include "part.h"
#include "ext_id.h"

#define SNOR_VENDOR_EON				0x1c
#define SNOR_VENDOR_ESMT			0x8c
#define SNOR_VENDOR_GIGADEVICE			0xc8
#define SNOR_VENDOR_INTEL			0x98
#define SNOR_VENDOR_ISSI			0x9d
#define SNOR_VENDOR_SST				0xbf
#define SNOR_VENDOR_WINBOND			0xef

struct json_object;

struct spi_nor_vendor_ops {
	ufprog_status(*init)(void);
};

struct spi_nor_vendor {
	uint8_t mfr_id;
	const char *id;
	const char *name;

	const struct spi_nor_flash_part *parts;
	uint32_t nparts;

	const struct spi_nor_vendor_ops *ops;
	const struct spi_nor_flash_part_ops *default_part_ops;
	const struct spi_nor_flash_part_fixup *default_part_fixups;

	const struct spi_nor_part_flag_enum_info *vendor_flag_names;
	uint32_t num_vendor_flag_names;
};

struct spi_nor_vendor_part {
	const struct spi_nor_vendor *vendor_init;
	const struct spi_nor_vendor *vendor;
	const struct spi_nor_flash_part *part;
};

extern const struct spi_nor_vendor vendor_eon;
extern const struct spi_nor_vendor vendor_esmt;
extern const struct spi_nor_vendor vendor_gigadevice;
extern const struct spi_nor_vendor vendor_intel;
extern const struct spi_nor_vendor vendor_issi;
extern const struct spi_nor_vendor vendor_issi_pmc;
extern const struct spi_nor_vendor vendor_sst;
extern const struct spi_nor_vendor vendor_winbond;

ufprog_status spi_nor_vendors_init(void);

const struct spi_nor_vendor *spi_nor_find_vendor(uint8_t mfr_id);
const struct spi_nor_vendor *spi_nor_find_vendor_by_id(const char *id);

bool spi_nor_find_vendor_part(const uint8_t *id, struct spi_nor_vendor_part *retvp);
bool spi_nor_find_vendor_part_by_name(const char *model, struct spi_nor_vendor_part *retvp);

const struct spi_nor_flash_part *spi_nor_vendor_find_part_by_name(const char *model,
								  const struct spi_nor_vendor *vendor);

uint32_t spi_nor_vendor_list_parts(const struct spi_nor_vendor *vendor, const char *match_part,
				   const struct spi_nor_id *match_id, struct spi_nor_probe_part *list, bool no_meta);
uint32_t spi_nor_list_parts(const char *vendorid, const char *match_part, const struct spi_nor_id *match_id,
			    struct spi_nor_probe_part *list);

bool spi_nor_set_ext_vendor_capacity(uint32_t n);
struct spi_nor_vendor *spi_nor_alloc_ext_vendor(void);
bool spi_nor_is_ext_vendor(const struct spi_nor_vendor *vendor);

typedef void (*spi_nor_reset_ext_vendor_cb)(struct spi_nor_vendor *vendor);
void spi_nor_reset_ext_vendors(spi_nor_reset_ext_vendor_cb cb);


#endif /* _UFPROG_SPI_NOR_VENDOR_H_ */
