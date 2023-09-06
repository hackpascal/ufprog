/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash ECC status reading operations
 */
#pragma once

#ifndef _UFPROG_SPI_NAND_ECC_H_
#define _UFPROG_SPI_NAND_ECC_H_

extern const struct nand_page_layout ecc_2k_64_1bit_layout;

ufprog_status spi_nand_check_dummy(struct spi_nand *snand);

ufprog_status spi_nand_check_extended_ecc_bfr_4b(struct spi_nand *snand);
ufprog_status spi_nand_check_extended_ecc_bfr_8b(struct spi_nand *snand);

ufprog_status spi_nand_check_ecc_1bit_per_step(struct spi_nand *snand);
ufprog_status spi_nand_check_ecc_8bits_sr_2bits(struct spi_nand *snand);

#endif /* _UFPROG_SPI_NAND_ECC_H_ */
