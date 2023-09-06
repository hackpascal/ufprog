// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Fudan Microelectronics SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"

static struct nand_otp_info fudanmicro_otp = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 25,
};

static const struct nand_page_layout fudanmicro_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(63),
);

static const struct spi_nand_flash_part fudanmicro_parts[] = {
	SNAND_PART("FM25S01A", SNAND_ID(SNAND_ID_DUMMY, 0xa1, 0xe4), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(104), SNAND_DUAL_MAX_SPEED_MHZ(40), SNAND_QUAD_MAX_SPEED_MHZ(40),
		   SNAND_PAGE_LAYOUT(&fudanmicro_ecc_layout),
		   NAND_OTP_INFO(&fudanmicro_otp),
	),
};

static ufprog_status fudanmicro_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup fudanmicro_fixups = {
	.pre_param_setup = fudanmicro_part_fixup,
};

static const struct spi_nand_flash_part_ops fudanmicro_part_ops = {
	.check_ecc = spi_nand_check_ecc_1bit_per_step,
};

const struct spi_nand_vendor vendor_fudanmicro = {
	.mfr_id = SNAND_VENDOR_PARAGON,
	.id = "fudanmicro",
	.name = "FudanMicro",
	.parts = fudanmicro_parts,
	.nparts = ARRAY_SIZE(fudanmicro_parts),
	.default_part_ops = &fudanmicro_part_ops,
	.default_part_fixups = &fudanmicro_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
};
