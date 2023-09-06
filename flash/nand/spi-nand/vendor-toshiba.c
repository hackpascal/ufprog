// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Toshiba/Kioxia SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"

/* Toshiba configuration bits */
#define SPI_NAND_CONFIG_TOSHIBA_HSE			BIT(1)

static const struct nand_page_layout toshiba_ecc_2k_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(4096),
	ECC_PAGE_MARKER_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(62),
	ECC_PAGE_PARITY_BYTES(64),
);

static const struct nand_page_layout toshiba_ecc_4k_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(4096),
	ECC_PAGE_MARKER_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(126),
	ECC_PAGE_PARITY_BYTES(128),
);

static const struct spi_nand_flash_part toshiba_parts[] = {
	SNAND_PART("TC58CVG0S3HRAIG", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xc2), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_2k_layout)
	),

	SNAND_PART("TC58CYG0S3HRAIG", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xb2), &snand_memorg_1g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_2k_layout)
	),

	SNAND_PART("TC58CVG0S3HRAIJ", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xe2, 0x40), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_2k_layout)
	),

	SNAND_PART("TC58CYG0S3HRAIJ", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xd2, 0x40), &snand_memorg_1g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_2k_layout)
	),

	SNAND_PART("TC58CVG1S3HRAIG", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xcb), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_2k_layout)
	),

	SNAND_PART("TC58CYG1S3HRAIG", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xbb), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_2k_layout)
	),

	SNAND_PART("TC58CVG1S3HRAIJ", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xeb, 0x40), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_2k_layout)
	),

	SNAND_PART("TC58CYG1S3HRAIJ", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xdb, 0x40), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_2k_layout)
	),

	SNAND_PART("TC58CVG2S0HRAIG", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xcd), &snand_memorg_4g_4k_256,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_4k_layout)
	),

	SNAND_PART("TC58CYG2S0HRAIG", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xbd), &snand_memorg_4g_4k_256, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_4k_layout)
	),

	SNAND_PART("TC58CVG2S0HRAIJ", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xed, 0x51), &snand_memorg_4g_4k_256,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_4k_layout)
	),

	SNAND_PART("TC58CYG2S0HRAIJ", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xdd, 0x51), &snand_memorg_4g_4k_256, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_4k_layout)
	),

	SNAND_PART("TH58CVG3S0HRAIJ", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xe4, 0x51), &snand_memorg_8g_4k_256,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_4k_layout)
	),

	SNAND_PART("TH58CYG3S0HRAIJ", SNAND_ID(SNAND_ID_DUMMY, 0x98, 0xd4, 0x51), &snand_memorg_8g_4k_256, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&toshiba_ecc_4k_layout)
	),
};

static ufprog_status toshiba_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	return UFP_OK;
}

static ufprog_status toshiba_part_setup_ecc_mark(struct spi_nand *snand)
{
	snand->ecc.bbm_config.flags |= ECC_F_BBM_MARK_WHOLE_PAGE;

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup toshiba_fixups = {
	.pre_param_setup = toshiba_part_fixup,
	.pre_chip_setup = toshiba_part_setup_ecc_mark,
};

static ufprog_status toshiba_setup_chip(struct spi_nand *snand)
{
	return spi_nand_update_config(snand, 0, SPI_NAND_CONFIG_TOSHIBA_HSE);
}

static const struct spi_nand_flash_part_ops toshiba_part_ops = {
	.chip_setup = toshiba_setup_chip,
};

static ufprog_status toshiba_pp_post_init(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bp->p.qe_type = QE_CR_BIT0;
	bp->p.ecc_type = ECC_CR_BIT4;
	bp->p.otp_en_type = OTP_CR_BIT6;

	bp->p.ecc_req.step_size = 512;
	bp->p.ecc_req.strength_per_step = 8;

	bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4;
	bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4;

	bp->p.flags |= SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B;

	return UFP_OK;
}

static const struct spi_nand_vendor_ops toshiba_ops = {
	.pp_post_init = toshiba_pp_post_init,
};

const struct spi_nand_vendor vendor_toshiba = {
	.mfr_id = SNAND_VENDOR_TOSHIBA,
	.id = "toshiba",
	.name = "Toshiba",
	.parts = toshiba_parts,
	.nparts = ARRAY_SIZE(toshiba_parts),
	.ops = &toshiba_ops,
	.default_part_ops = &toshiba_part_ops,
	.default_part_fixups = &toshiba_fixups,
};
