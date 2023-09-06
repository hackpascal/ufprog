// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ATO Solutions SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"

static struct nand_otp_info ato_otp = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 8,
};

static const struct nand_page_layout ato_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(7),
	ECC_PAGE_PARITY_BYTES(8),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(8),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(8),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(8),
);

static const struct spi_nand_flash_part ato_parts[] = {
	SNAND_PART("ATO25D1GA", SNAND_ID(SNAND_ID_DUMMY, 0x9b, 0x12), &snand_memorg_1g_2k_64,
		   NAND_ECC_INFO(512, 1),
		   SNAND_QE_CR_BIT0, SNAND_ECC_ALWAYS_ON, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_PL_OPCODES(default_pl_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ato_layout),
		   NAND_OTP_INFO(&ato_otp),
		   NAND_NOPS(4),
	),
};

static const struct spi_nand_flash_part_ops ato_part_ops = {
	.check_ecc = spi_nand_check_dummy,
};

const struct spi_nand_vendor vendor_ato = {
	.mfr_id = SNAND_VENDOR_ATO,
	.id = "ato",
	.name = "ATO Solutions",
	.parts = ato_parts,
	.nparts = ARRAY_SIZE(ato_parts),
	.default_part_ops = &ato_part_ops,
	.default_part_otp_ops = &spi_nand_otp_ops,
};
