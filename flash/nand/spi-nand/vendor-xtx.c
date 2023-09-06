// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * XTX SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"

/* XTX read UID opcode */
#define SNAND_CMD_XTX_READ_UID				0x4b
#define XTX_UID_LEN					16

/* XTX ECC status bits */
#define XTX_SR_ECC_STATUS_SHIFT				2
#define XTX_SR_ECC_STATUS_MASK				BITS(5, XTX_SR_ECC_STATUS_SHIFT)

#define XT26G01C_SR_ECC_STATUS_MASK			BITS(7, SPI_NAND_STATUS_ECC_SHIFT)

/* XTX configuration bits */
#define SPI_NAND_CONFIG_XTX_HSE				BIT(1)

static struct nand_otp_info xtx_otp = {
	.start_index = 0,
	.count = 4,
};

static struct nand_otp_info xt26q01d_otp = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 4,
};

static const struct nand_page_layout xtx_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_FREE_BYTES(7),
	ECC_PAGE_OOB_DATA_BYTES(40),
	ECC_PAGE_PARITY_BYTES(16),
);

static const struct nand_page_layout xt26g01c_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(63),
	ECC_PAGE_PARITY_BYTES(52),
	ECC_PAGE_OOB_FREE_BYTES(12),
);

static const struct nand_page_layout xt26q01d_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(63),
	ECC_PAGE_PARITY_BYTES(64),
);

static const struct nand_memorg snand_memorg_4g_2k_64_128ppb = SNAND_MEMORG(2048, 64, 128, 2048, 1, 1);

static ufprog_status spi_nand_check_ecc_xt26g01c(struct spi_nand *snand);
static ufprog_status spi_nand_check_ecc_xt26q01d(struct spi_nand *snand);
static ufprog_status xtx_read_uid(struct spi_nand *snand, void *data, uint32_t *retlen);

static const struct spi_nand_flash_part_ops xt26g01c_part_ops = {
	.read_uid = xtx_read_uid,
	.check_ecc = spi_nand_check_ecc_xt26g01c,
};

static ufprog_status xt26q01d_setup_chip(struct spi_nand *snand)
{
	return spi_nand_update_config(snand, 0, SPI_NAND_CONFIG_XTX_HSE);
}

static const struct spi_nand_flash_part_ops xt26q01d_part_ops = {
	.chip_setup = xt26q01d_setup_chip,
	.check_ecc = spi_nand_check_ecc_xt26q01d,
};

static const struct spi_nand_flash_part xtx_parts[] = {
	SNAND_PART("XT26G01A", SNAND_ID(SNAND_ID_DUMMY, 0x0b, 0xe1), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(90),
		   SNAND_PAGE_LAYOUT(&xtx_ecc_layout),
		   NAND_OTP_INFO(&xtx_otp),
	),

	SNAND_PART("XT26G01C", SNAND_ID(SNAND_ID_DUMMY, 0x0b, 0x11), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&xt26g01c_ecc_layout),
		   NAND_OTP_INFO(&xtx_otp),
		   SNAND_OPS(&xt26g01c_part_ops),
	),

	SNAND_PART("XT26Q01D", SNAND_ID(SNAND_ID_DUMMY, 0x0b, 0x51), &snand_memorg_1g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&xt26q01d_ecc_layout),
		   NAND_OTP_INFO(&xt26q01d_otp),
		   SNAND_OPS(&xt26q01d_part_ops),
	),

	SNAND_PART("XT26G02A", SNAND_ID(SNAND_ID_DUMMY, 0x0b, 0xe2), &snand_memorg_2g_2k_64,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(90),
		   SNAND_PAGE_LAYOUT(&xtx_ecc_layout),
		   NAND_OTP_INFO(&xtx_otp),
	),

	SNAND_PART("XT26G04A", SNAND_ID(SNAND_ID_DUMMY, 0x0b, 0xe3), &snand_memorg_4g_2k_64_128ppb,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(90),
		   SNAND_PAGE_LAYOUT(&xtx_ecc_layout),
		   NAND_OTP_INFO(&xtx_otp),
	),
};

static ufprog_status spi_nand_check_ecc_xtx(struct spi_nand *snand)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & XTX_SR_ECC_STATUS_MASK) >> XTX_SR_ECC_STATUS_SHIFT;

	if (!sr)
		return UFP_OK;

	if (sr <= 7) {
		snand->ecc_status->step_bitflips[0] = sr;
		return UFP_ECC_CORRECTED;
	}

	if (sr == 0x0c) {
		snand->ecc_status->step_bitflips[0] = 8;
		return UFP_ECC_CORRECTED;
	}

	snand->ecc_status->step_bitflips[0] = -1;

	return UFP_ECC_UNCORRECTABLE;
}

static ufprog_status spi_nand_check_ecc_xt26g01c(struct spi_nand *snand)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & XT26G01C_SR_ECC_STATUS_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;

	if (!sr)
		return UFP_OK;

	if (sr <= 8) {
		snand->ecc_status->step_bitflips[0] = sr;
		return UFP_ECC_CORRECTED;
	}

	snand->ecc_status->step_bitflips[0] = -1;

	return UFP_ECC_UNCORRECTABLE;
}

static ufprog_status spi_nand_check_ecc_xt26q01d(struct spi_nand *snand)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & XT26G01C_SR_ECC_STATUS_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;

	if ((sr & 0x3) == 0)
		return UFP_OK;

	if ((sr & 0x3) == 1) {
		snand->ecc_status->step_bitflips[0] = (sr >> 2) + 4;
		return UFP_ECC_CORRECTED;
	}

	if ((sr & 0x3) == 3) {
		snand->ecc_status->step_bitflips[0] = 8;
		return UFP_ECC_CORRECTED;
	}

	snand->ecc_status->step_bitflips[0] = -1;

	return UFP_ECC_UNCORRECTABLE;
}

static ufprog_status xtx_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup xtx_fixups = {
	.pre_param_setup = xtx_part_fixup,
};

static ufprog_status xtx_read_uid(struct spi_nand *snand, void *data, uint32_t *retlen)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNAND_CMD_XTX_READ_UID, 1),
		SPI_MEM_OP_ADDR(3, 0, 1),
		SPI_MEM_OP_DUMMY(1, 1),
		SPI_MEM_OP_DATA_IN(XTX_UID_LEN, data, 1)
	);

	if (retlen)
		*retlen = XTX_UID_LEN;

	if (!data)
		return UFP_OK;

	return ufprog_spi_mem_exec_op(snand->spi, &op);
}

static const struct spi_nand_flash_part_ops xtx_part_ops = {
	.check_ecc = spi_nand_check_ecc_xtx,
};

const struct spi_nand_vendor vendor_xtx = {
	.mfr_id = SNAND_VENDOR_XTX,
	.id = "xtx",
	.name = "XTX",
	.parts = xtx_parts,
	.nparts = ARRAY_SIZE(xtx_parts),
	.default_part_ops = &xtx_part_ops,
	.default_part_fixups = &xtx_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
};
