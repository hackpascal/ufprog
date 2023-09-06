// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * MK Founder SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"
#include "vendor-etron.h"

/* MK Founder vendor flags */
#define MK_F_PP_OTP_PAGE_0				BIT(0)

static const struct spi_nand_part_flag_enum_info mk_vendor_flag_info[] = {
	{ 0, "param-page-otp-page-0" },
};

static struct nand_otp_info mk_otp = {
	.start_index = 1,
	.count = 63,
};

static const struct spi_nand_flash_part mk_parts[] = {
	SNAND_PART("MKSV1GIL", SNAND_ID(SNAND_ID_DUMMY, 0xd5, 0x26), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(MK_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&etron_2k_64_ecc_layout),
		   NAND_OTP_INFO(&mk_otp),
	),

	SNAND_PART("MKSV2GIL", SNAND_ID(SNAND_ID_DUMMY, 0xd5, 0x27), &snand_memorg_2g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(MK_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&etron_2k_64_ecc_layout),
		   NAND_OTP_INFO(&mk_otp),
	),

	SNAND_PART("MKSV4GIL", SNAND_ID(SNAND_ID_DUMMY, 0xd5, 0x33), &snand_memorg_4g_4k_256,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(MK_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&etron_4k_256_ecc_layout),
		   NAND_OTP_INFO(&mk_otp),
	),

	SNAND_PART("MKSV8GIL", SNAND_ID(SNAND_ID_DUMMY, 0xd5, 0x34), &snand_memorg_8g_4k_256,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(MK_F_PP_OTP_PAGE_0),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&etron_4k_256_ecc_layout),
		   NAND_OTP_INFO(&mk_otp),
	),
};

static ufprog_status mk_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	if (bp->p.vendor_flags & MK_F_PP_OTP_PAGE_0)
		spi_nand_probe_onfi_generic(snand, bp, 0, false);

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup mk_fixups = {
	.pre_param_setup = mk_part_fixup,
};

static const struct spi_nand_flash_part_ops mk_part_ops = {
	.check_ecc = spi_nand_check_ecc_8bits_sr_2bits,
};

static ufprog_status mk_pp_post_init(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bp->p.qe_type = QE_CR_BIT0;
	bp->p.ecc_type = ECC_CR_BIT4;
	bp->p.otp_en_type = OTP_CR_BIT6;

	bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4;
	bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4;

	return UFP_OK;
}

static const struct spi_nand_vendor_ops mk_ops = {
	.pp_post_init = mk_pp_post_init,
};

const struct spi_nand_vendor vendor_mk = {
	.mfr_id = SNAND_VENDOR_ETRON,
	.id = "mk",
	.name = "MK",
	.parts = mk_parts,
	.nparts = ARRAY_SIZE(mk_parts),
	.ops = &mk_ops,
	.default_part_ops = &mk_part_ops,
	.default_part_fixups = &mk_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
	.vendor_flag_names = mk_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(mk_vendor_flag_info),
};
