// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Fidelix SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"

static struct nand_otp_info fm_otp = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 30,
};

static const struct spi_nand_flash_part fidelix_parts[] = {
	SNAND_PART("FM35Q1GA", SNAND_ID(SNAND_ID_DUMMY, 0xe5, 0x71), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(100),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&fm_otp),
	),

	SNAND_PART("FM35M1GA", SNAND_ID(SNAND_ID_DUMMY, 0xe5, 0x21), &snand_memorg_1g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(83),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&fm_otp),
	),
};

static ufprog_status fidelix_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup fidelix_fixups = {
	.pre_param_setup = fidelix_part_fixup,
};

static const struct spi_nand_flash_part_ops fidelix_part_ops = {
	.check_ecc = spi_nand_check_ecc_1bit_per_step,
};

static ufprog_status fidelix_pp_post_init(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bp->p.qe_type = QE_CR_BIT0;
	bp->p.ecc_type = ECC_CR_BIT4;
	bp->p.otp_en_type = OTP_CR_BIT6;

	bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4;
	bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4;

	bp->p.ecc_req.step_size = 512;
	bp->p.ecc_req.strength_per_step = 1;

	return UFP_OK;
}

static const struct spi_nand_vendor_ops fidelix_ops = {
	.pp_post_init = fidelix_pp_post_init,
};

const struct spi_nand_vendor vendor_fidelix = {
	.mfr_id = SNAND_VENDOR_FIDELIX,
	.id = "fidelix",
	.name = "Fidelix",
	.parts = fidelix_parts,
	.nparts = ARRAY_SIZE(fidelix_parts),
	.ops = &fidelix_ops,
	.default_part_ops = &fidelix_part_ops,
	.default_part_fixups = &fidelix_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
};
