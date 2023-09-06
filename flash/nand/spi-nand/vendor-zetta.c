// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Zetta SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"

static struct nand_otp_info zetta_otp = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 30,
};

static struct nand_otp_info zetta_otp_4 = {
	.start_index = 0,
	.count = 4,
};

const struct nand_page_layout zd35q1gc_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(2),
	ECC_PAGE_PARITY_BYTES(13),
	ECC_PAGE_OOB_DATA_BYTES(3),
	ECC_PAGE_PARITY_BYTES(13),
	ECC_PAGE_OOB_DATA_BYTES(3),
	ECC_PAGE_PARITY_BYTES(13),
	ECC_PAGE_OOB_DATA_BYTES(3),
	ECC_PAGE_PARITY_BYTES(13),
);

static const struct spi_nand_flash_part_ops zd35q1gc_part_ops = {
	.check_ecc = spi_nand_check_ecc_8bits_sr_2bits,
};

static const struct spi_nand_flash_part zetta_parts[] = {
	SNAND_PART("ZD35Q1G", SNAND_ID(SNAND_ID_DUMMY, 0xba, 0x71), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(0, 0),
		   SNAND_FLAGS(SNAND_F_META | SNAND_F_NO_OP),
	),

	SNAND_PART("ZD35Q1GA", SNAND_ID(SNAND_ID_DUMMY, 0xba, 0x71), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&zetta_otp),
	),

	SNAND_PART("ZD35Q1GC", SNAND_ID(SNAND_ID_DUMMY, 0xba, 0x71), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&zd35q1gc_ecc_layout),
		   NAND_OTP_INFO(&zetta_otp_4),
		   SNAND_OPS(&zd35q1gc_part_ops),
	),

	SNAND_PART("ZD35M1GA", SNAND_ID(SNAND_ID_DUMMY, 0xba, 0x21), &snand_memorg_1g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(90),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&zetta_otp),
	),

	SNAND_PART("ZD35Q2GA", SNAND_ID(SNAND_ID_DUMMY, 0xba, 0x72), &snand_memorg_2g_2k_64,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&zetta_otp),
	),

	SNAND_PART("ZD35M2GA", SNAND_ID(SNAND_ID_DUMMY, 0xba, 0x22), &snand_memorg_2g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(90),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&zetta_otp),
	),
};

static ufprog_status zetta_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup zetta_fixups = {
	.pre_param_setup = zetta_part_fixup,
};

static const struct spi_nand_flash_part_ops zetta_part_ops = {
	.check_ecc = spi_nand_check_ecc_1bit_per_step,
};

static ufprog_status zetta_pp_post_init(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bp->p.qe_type = QE_CR_BIT0;
	bp->p.ecc_type = ECC_CR_BIT4;
	bp->p.otp_en_type = OTP_CR_BIT6;

	bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4;
	bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4;

	return UFP_OK;
}

static const struct spi_nand_vendor_ops zetta_ops = {
	.pp_post_init = zetta_pp_post_init,
};

const struct spi_nand_vendor vendor_zetta = {
	.mfr_id = SNAND_VENDOR_ZETTA,
	.id = "zetta",
	.name = "Zetta",
	.parts = zetta_parts,
	.nparts = ARRAY_SIZE(zetta_parts),
	.ops = &zetta_ops,
	.default_part_ops = &zetta_part_ops,
	.default_part_fixups = &zetta_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
};
