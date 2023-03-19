/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash SFDP internal definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NOR_SFDP_INTERNAL_H_
#define _UFPROG_SPI_NOR_SFDP_INTERNAL_H_

#include <ufprog/endian.h>
#include <ufprog/spi-nor-sfdp.h>

struct spi_nor_vendor;
struct spi_nor_flash_part_blank;

struct spi_nor_sfdp {
	void *data;
	uint32_t size;

	struct sfdp_param_header *bfpt_hdr;
	uint32_t *bfpt;
	uint32_t bfpt_dw_num;

	struct sfdp_param_header *a4bit_hdr;
	uint32_t *a4bit;
	uint32_t a4bit_dw_num;

	struct sfdp_param_header *smpt_hdr;
	uint32_t *smpt;
	uint32_t smpt_dw_num;

	struct sfdp_param_header *vendor_hdr;
	uint32_t *vendor;
	uint32_t vendor_dw_num;
};

static inline uint32_t sfdp_dw(const uint32_t *dw, uint32_t idx)
{
	return le32toh(dw[idx - 1]);
}

ufprog_status spi_nor_read_sfdp(struct spi_nor *snor, uint8_t buswidth, uint32_t addr, uint32_t len, void *data);

bool spi_nor_probe_sfdp(struct spi_nor *snor, const struct spi_nor_vendor *vendor, struct spi_nor_flash_part_blank *bp);
bool spi_nor_parse_sfdp_smpt(struct spi_nor *snor);
bool spi_nor_locate_sfdp_vendor(struct spi_nor *snor, uint8_t mfr_id, bool match_jedec_msb);

#endif /* _UFPROG_SPI_NOR_SFDP_INTERNAL_H_ */
