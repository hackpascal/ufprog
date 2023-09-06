// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Dosilicon SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"
#include "vendor-micron.h"

static struct nand_otp_info dosilicon_otp = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 30,
};

const struct nand_page_layout dosilicon_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(63),
	ECC_PAGE_PARITY_BYTES(64),
);

static const struct spi_nand_flash_part_ops dosilicon_ecc_1bit_part_ops = {
	.check_ecc = spi_nand_check_ecc_1bit_per_step,
};

static const struct spi_nand_flash_part_ops dosilicon_ecc_8bits_part_ops = {
	.check_ecc = spi_nand_check_ecc_micron_8bits,
};

static const struct spi_nand_flash_part dosilicon_parts[] = {
	SNAND_PART("DS35Q12B", SNAND_ID(SNAND_ID_DUMMY, 0xe5, 0xf5), &snand_memorg_512m_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&dosilicon_ecc_layout),
		   NAND_OTP_INFO(&dosilicon_otp),
		   SNAND_OPS(&dosilicon_ecc_8bits_part_ops),
	),

	SNAND_PART("DS35M12B", SNAND_ID(SNAND_ID_DUMMY, 0xe5, 0xa5), &snand_memorg_512m_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(83),
		   SNAND_PAGE_LAYOUT(&dosilicon_ecc_layout),
		   NAND_OTP_INFO(&dosilicon_otp),
		   SNAND_OPS(&dosilicon_ecc_8bits_part_ops),
	),

	SNAND_PART("DS35Q1GA", SNAND_ID(SNAND_ID_DUMMY, 0xba, 0x71), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&dosilicon_otp),
		   SNAND_OPS(&dosilicon_ecc_1bit_part_ops),
	),

	SNAND_PART("DS35M1GA", SNAND_ID(SNAND_ID_DUMMY, 0xba, 0x21), &snand_memorg_1g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&dosilicon_otp),
		   SNAND_OPS(&dosilicon_ecc_1bit_part_ops),
	),

	SNAND_PART("DS35Q1GB", SNAND_ID(SNAND_ID_DUMMY, 0xe5, 0xf1), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&dosilicon_ecc_layout),
		   NAND_OTP_INFO(&dosilicon_otp),
		   SNAND_OPS(&dosilicon_ecc_8bits_part_ops),
	),

	SNAND_PART("DS35M1GB", SNAND_ID(SNAND_ID_DUMMY, 0xe5, 0xa1), &snand_memorg_1g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(83),
		   SNAND_PAGE_LAYOUT(&dosilicon_ecc_layout),
		   NAND_OTP_INFO(&dosilicon_otp),
		   SNAND_OPS(&dosilicon_ecc_8bits_part_ops),
	),

	SNAND_PART("DS35Q2GA", SNAND_ID(SNAND_ID_DUMMY, 0xba, 0x73), &snand_memorg_2g_2k_64,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&dosilicon_otp),
		   SNAND_OPS(&dosilicon_ecc_1bit_part_ops),
	),

	SNAND_PART("DS35M2GA", SNAND_ID(SNAND_ID_DUMMY, 0xba, 0x23), &snand_memorg_2g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&dosilicon_otp),
		   SNAND_OPS(&dosilicon_ecc_1bit_part_ops),
	),

	SNAND_PART("DS35Q2GB", SNAND_ID(SNAND_ID_DUMMY, 0xe5, 0xf2), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&dosilicon_ecc_layout),
		   NAND_OTP_INFO(&dosilicon_otp),
		   SNAND_OPS(&dosilicon_ecc_8bits_part_ops),
	),

	SNAND_PART("DS35M2GB", SNAND_ID(SNAND_ID_DUMMY, 0xe5, 0xa2), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(83),
		   SNAND_PAGE_LAYOUT(&dosilicon_ecc_layout),
		   NAND_OTP_INFO(&dosilicon_otp),
		   SNAND_OPS(&dosilicon_ecc_8bits_part_ops),
	),

	SNAND_PART("DS35Q4GM", SNAND_ID(SNAND_ID_DUMMY, 0xe5, 0xf4), &snand_memorg_4g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&dosilicon_ecc_layout),
		   NAND_OTP_INFO(&dosilicon_otp),
		   SNAND_OPS(&dosilicon_ecc_8bits_part_ops),
	),

	SNAND_PART("DS35M4GM", SNAND_ID(SNAND_ID_DUMMY, 0xe5, 0xa4), &snand_memorg_4g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(83),
		   SNAND_PAGE_LAYOUT(&dosilicon_ecc_layout),
		   NAND_OTP_INFO(&dosilicon_otp),
		   SNAND_OPS(&dosilicon_ecc_8bits_part_ops),
	),
};

static ufprog_status dosilicon_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup dosilicon_fixups = {
	.pre_param_setup = dosilicon_part_fixup,
};

static ufprog_status dosilicon_pp_post_init(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bp->p.qe_type = QE_CR_BIT0;
	bp->p.ecc_type = ECC_CR_BIT4;
	bp->p.otp_en_type = OTP_CR_BIT6;

	bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4;
	bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4;

	return UFP_OK;
}

static const struct spi_nand_vendor_ops dosilicon_ops = {
	.pp_post_init = dosilicon_pp_post_init,
};

const struct spi_nand_vendor vendor_dosilicon = {
	.mfr_id = SNAND_VENDOR_FIDELIX,
	.id = "dosilicon",
	.name = "Dosilicon",
	.parts = dosilicon_parts,
	.nparts = ARRAY_SIZE(dosilicon_parts),
	.ops = &dosilicon_ops,
	.default_part_fixups = &dosilicon_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
};
