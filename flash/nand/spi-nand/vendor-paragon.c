// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Paragon SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"

/* Paragon read UID opcode */
#define SNAND_CMD_PARAGON_READ_UID			0x4b
#define PARAGON_UID_LEN					8

static struct nand_otp_info paragon_otp = {
	.start_index = 0,
	.count = 8,
};

static const struct nand_page_layout paragon_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_FREE_BYTES(3),
	ECC_PAGE_OOB_DATA_BYTES(2),
	ECC_PAGE_PARITY_BYTES(13),
	ECC_PAGE_OOB_DATA_BYTES(2),
	ECC_PAGE_PARITY_BYTES(13),
	ECC_PAGE_OOB_DATA_BYTES(2),
	ECC_PAGE_PARITY_BYTES(13),
	ECC_PAGE_OOB_DATA_BYTES(2),
	ECC_PAGE_PARITY_BYTES(13),
	ECC_PAGE_OOB_FREE_BYTES(64),
);

static const struct spi_nand_flash_part paragon_parts[] = {
	SNAND_PART("PN26G01A", SNAND_ID(SNAND_ID_DUMMY, 0xa1, 0xe1), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&paragon_ecc_layout),
		   NAND_OTP_INFO(&paragon_otp),
	),

	SNAND_PART("PN26G02A", SNAND_ID(SNAND_ID_DUMMY, 0xa1, 0xe2), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&paragon_ecc_layout),
		   NAND_OTP_INFO(&paragon_otp),
	),
};

static ufprog_status paragon_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup paragon_fixups = {
	.pre_param_setup = paragon_part_fixup,
};

static ufprog_status paragon_read_uid(struct spi_nand *snand, void *data, uint32_t *retlen)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNAND_CMD_PARAGON_READ_UID, 1),
		SPI_MEM_OP_NO_ADDR,
		SPI_MEM_OP_DUMMY(4, 1),
		SPI_MEM_OP_DATA_IN(PARAGON_UID_LEN, data, 1)
	);

	if (retlen)
		*retlen = PARAGON_UID_LEN;

	if (!data)
		return UFP_OK;

	return ufprog_spi_mem_exec_op(snand->spi, &op);
}

static const struct spi_nand_flash_part_ops paragon_part_ops = {
	.check_ecc = spi_nand_check_ecc_8bits_sr_2bits,
	.read_uid = paragon_read_uid,
};

const struct spi_nand_vendor vendor_paragon = {
	.mfr_id = SNAND_VENDOR_PARAGON,
	.id = "paragon",
	.name = "Paragon",
	.parts = paragon_parts,
	.nparts = ARRAY_SIZE(paragon_parts),
	.default_part_ops = &paragon_part_ops,
	.default_part_fixups = &paragon_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
};
