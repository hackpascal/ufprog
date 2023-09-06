// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Alliance Memory SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"
#include "vendor-etron.h"

/* Alliance Memory vendor flags */
#define AM_F_PP_OTP_PAGE_0				BIT(0)

static const struct spi_nand_part_flag_enum_info am_vendor_flag_info[] = {
	{ 0, "param-page-otp-page-0" },
};

static struct nand_otp_info am_otp = {
	.start_index = 1,
	.count = 63,
};

const struct nand_page_layout am_2k_64_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(7),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(32),
);

const struct nand_page_layout am_2k_128_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(17),
	ECC_PAGE_OOB_DATA_BYTES(18),
	ECC_PAGE_OOB_DATA_BYTES(18),
	ECC_PAGE_OOB_DATA_BYTES(18),
	ECC_PAGE_PARITY_BYTES(56),
);

static const struct spi_nand_flash_part am_parts[] = {
	SNAND_PART("AS5F31G04SND", SNAND_ID(SNAND_ID_DUMMY, 0x52, 0x25), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(AM_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&etron_2k_64_ecc_layout),
		   NAND_OTP_INFO(&am_otp),
	),

	SNAND_PART("AS5F32G04SNDB", SNAND_ID(SNAND_ID_DUMMY, 0x52, 0x41), &snand_memorg_2g_2k_64,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(AM_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&am_2k_64_ecc_layout),
		   NAND_OTP_INFO(&am_otp),
	),

	SNAND_PART("AS5F32G04SNDA", SNAND_ID(SNAND_ID_DUMMY, 0x52, 0x3a), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(AM_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&am_2k_128_ecc_layout),
		   NAND_OTP_INFO(&am_otp),
	),

	SNAND_PART("AS5F12G04SND", SNAND_ID(SNAND_ID_DUMMY, 0x52, 0x8e), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(AM_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(100),
		   SNAND_PAGE_LAYOUT(&etron_2k_128_ecc_layout),
		   NAND_OTP_INFO(&am_otp),
	),

	SNAND_PART("AS5F34G04SNDB", SNAND_ID(SNAND_ID_DUMMY, 0x52, 0x42), &snand_memorg_4g_2k_64,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(AM_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&am_2k_64_ecc_layout),
		   NAND_OTP_INFO(&am_otp),
	),

	SNAND_PART("AS5F34G04SNDA", SNAND_ID(SNAND_ID_DUMMY, 0x52, 0x3b), &snand_memorg_4g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(AM_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&am_2k_128_ecc_layout),
		   NAND_OTP_INFO(&am_otp),
	),

	SNAND_PART("AS5F14G04SND", SNAND_ID(SNAND_ID_DUMMY, 0x52, 0x8f), &snand_memorg_4g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(AM_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(100),
		   SNAND_PAGE_LAYOUT(&etron_2k_128_ecc_layout),
		   NAND_OTP_INFO(&am_otp),
	),

	SNAND_PART("AS5F38G04SND", SNAND_ID(SNAND_ID_DUMMY, 0x52, 0x2d), &snand_memorg_8g_4k_256,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(AM_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&etron_4k_256_ecc_layout),
		   NAND_OTP_INFO(&am_otp),
	),

	SNAND_PART("AS5F18G04SND", SNAND_ID(SNAND_ID_DUMMY, 0x52, 0x8d), &snand_memorg_8g_4k_256, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(AM_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(100),
		   SNAND_PAGE_LAYOUT(&etron_4k_256_ecc_layout),
		   NAND_OTP_INFO(&am_otp),
	),
};

static ufprog_status am_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	if (bp->p.vendor_flags & AM_F_PP_OTP_PAGE_0)
		spi_nand_probe_onfi_generic(snand, bp, 0, false);

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup am_fixups = {
	.pre_param_setup = am_part_fixup,
};

static const struct spi_nand_flash_part_ops am_part_ops = {
	.check_ecc = spi_nand_check_ecc_8bits_sr_2bits,
};

static ufprog_status am_pp_post_init(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bp->p.qe_type = QE_CR_BIT0;
	bp->p.ecc_type = ECC_CR_BIT4;
	bp->p.otp_en_type = OTP_CR_BIT6;

	bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4;
	bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4;

	return UFP_OK;
}

static const struct spi_nand_vendor_ops am_ops = {
	.pp_post_init = am_pp_post_init,
};

const struct spi_nand_vendor vendor_alliance_memory = {
	.mfr_id = SNAND_VENDOR_ALLIANCE_MEMORY,
	.id = "alliance-memory",
	.name = "AllianceMemory",
	.parts = am_parts,
	.nparts = ARRAY_SIZE(am_parts),
	.ops = &am_ops,
	.default_part_ops = &am_part_ops,
	.default_part_fixups = &am_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
	.vendor_flag_names = am_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(am_vendor_flag_info),
};
