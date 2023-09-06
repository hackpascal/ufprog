/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Micron SPI-NAND flash definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NAND_VENDOR_MICRON_H_
#define _UFPROG_SPI_NAND_VENDOR_MICRON_H_

#include <stdbool.h>
#include "core.h"

#define SPI_NAND_MICRON_CR_CFG_MASK			0xc2
#define SPI_NAND_MICRON_CR_CFG_OTP_PROTECT		0xc0
#define SPI_NAND_MICRON_CR_CFG_NOR_READ			0x82

extern const struct nand_page_layout mt_2k_ecc_8bits_layout;

ufprog_status spi_nand_micron_cfg_enabled(struct spi_nand *snand, uint8_t cfg, uint32_t check_size, uint8_t *buf,
					  bool compromise, bool *retenabled);
ufprog_status spi_nand_micron_enable_cfg(struct spi_nand *snand, uint8_t cfg);

ufprog_status spi_nand_otp_control_micron(struct spi_nand *snand, bool enable);
ufprog_status spi_nand_select_die_micron(struct spi_nand *snand, uint32_t dieidx);
ufprog_status spi_nand_check_ecc_micron_8bits(struct spi_nand *snand);

ufprog_status micron_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp);

ufprog_status micron_nor_read_enable(struct spi_nand *snand);
ufprog_status micron_nor_read_enabled(struct spi_nand *snand, ufprog_bool *retenabled);

#endif /* _UFPROG_SPI_NAND_VENDOR_MICRON_H_ */
