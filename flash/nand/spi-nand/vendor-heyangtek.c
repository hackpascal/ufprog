// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * HeYangTek SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"
#include "vendor-micron.h"

static struct nand_otp_info heyangtek_otp = {
	.start_index = 0,
	.count = 4,
};

static struct nand_otp_info heyangtek01_otp = {
	.start_index = 0x182,
	.count = 62,
};

static const struct nand_page_layout heyangtek_ecc_4bits_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_FREE_BYTES(3),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(8),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(8),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(8),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(8),
);

static const struct nand_page_layout heyangtek_2k_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_FREE_BYTES(3),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(24),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(24),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(24),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(24),
);

static const struct nand_page_layout heyangtek_4k_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(4096),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_FREE_BYTES(3),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(24),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(24),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(24),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(24),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(24),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(24),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(24),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(24),
);

static DEFINE_SNAND_ALIAS(hyf1gq4uaacae_alias, SNAND_ALIAS_MODEL("HYF1GQ4IAACAE"));
static DEFINE_SNAND_ALIAS(hyf2gq4uaacae_alias, SNAND_ALIAS_MODEL("HYF2GQ4UADCAE"));
static DEFINE_SNAND_ALIAS(hyf2gq4iaacae_alias, SNAND_ALIAS_MODEL("HYF2GQ4IACCAE"));
static DEFINE_SNAND_ALIAS(hyf4gq4uaacae_alias, SNAND_ALIAS_MODEL("HYF4GQ4UACCAE"));

static const struct spi_nand_flash_part heyangtek_parts[] = {
	SNAND_PART("HYF1GQ4UDACAE", SNAND_ID(SNAND_ID_DUMMY, 0xc9, 0x21), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(60),
		   SNAND_PAGE_LAYOUT(&heyangtek_ecc_4bits_layout),
		   NAND_OTP_INFO(&heyangtek_otp),
	),

	SNAND_PART("HYF1GQ4IDACAE", SNAND_ID(SNAND_ID_DUMMY, 0xc9, 0x81), &snand_memorg_1g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&heyangtek_ecc_4bits_layout),
		   NAND_OTP_INFO(&heyangtek_otp),
	),

	SNAND_PART("HYF1GQ4UAACAE", SNAND_ID(SNAND_ID_DUMMY, 0xc9, 0x51), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 14),
		   SNAND_ALIAS(&hyf1gq4uaacae_alias),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(80),
		   SNAND_PAGE_LAYOUT(&heyangtek_2k_ecc_layout),
		   NAND_OTP_INFO(&heyangtek_otp),
	),

	SNAND_PART("HYF2GQ4UAACAE", SNAND_ID(SNAND_ID_DUMMY, 0xc9, 0x52), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 14),
		   SNAND_ALIAS(&hyf2gq4uaacae_alias),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(80),
		   SNAND_PAGE_LAYOUT(&heyangtek_2k_ecc_layout),
		   NAND_OTP_INFO(&heyangtek_otp),
	),

	SNAND_PART("HYF2GQ4IAACAE", SNAND_ID(SNAND_ID_DUMMY, 0xc9, 0x82), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 14),
		   SNAND_ALIAS(&hyf2gq4iaacae_alias),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&heyangtek_2k_ecc_layout),
		   NAND_OTP_INFO(&heyangtek_otp),
	),

	SNAND_PART("HYF4GQ4UAACAE", SNAND_ID(SNAND_ID_DUMMY, 0xc9, 0x54), &snand_memorg_4g_2k_128,
		   NAND_ECC_REQ(512, 14),
		   SNAND_ALIAS(&hyf4gq4uaacae_alias),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&heyangtek_2k_ecc_layout),
		   NAND_OTP_INFO(&heyangtek_otp),
	),

	SNAND_PART("HYF4GQ4UAACBE", SNAND_ID(SNAND_ID_DUMMY, 0xc9, 0xd4), &snand_memorg_4g_4k_256,
		   NAND_ECC_REQ(512, 14),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(60),
		   SNAND_PAGE_LAYOUT(&heyangtek_4k_ecc_layout),
		   NAND_OTP_INFO(&heyangtek_otp),
	),

	SNAND_PART("HYF8GQ4UACCAE", SNAND_ID(SNAND_ID_DUMMY, 0xc9, 0x58), &snand_memorg_8g_2k_128,
		   NAND_ECC_REQ(512, 14),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&heyangtek_2k_ecc_layout),
		   NAND_OTP_INFO(&heyangtek_otp),
	),
};

static ufprog_status heyangtek_check_ecc(struct spi_nand *snand)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & SPI_NAND_STATUS_ECC_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;

	if (!sr)
		return UFP_OK;

	if (sr == 1) {
		snand->ecc_status->step_bitflips[0] = snand->nand.ecc_req.strength_per_step - 1;
		return UFP_ECC_CORRECTED;
	}

	if (sr == 3) {
		snand->ecc_status->step_bitflips[0] = snand->nand.ecc_req.strength_per_step;
		return UFP_ECC_CORRECTED;
	}

	snand->ecc_status->step_bitflips[0] = -1;

	return UFP_ECC_UNCORRECTABLE;
}

static ufprog_status heyangtek_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup heyangtek_fixups = {
	.pre_param_setup = heyangtek_part_fixup,
};

static const struct spi_nand_flash_part_ops heyangtek_part_ops = {
	.check_ecc = heyangtek_check_ecc,
};

const struct spi_nand_vendor vendor_heyangtek = {
	.mfr_id = SNAND_VENDOR_HEYANGTEK,
	.id = "heyangtek",
	.name = "HeYangTek",
	.parts = heyangtek_parts,
	.nparts = ARRAY_SIZE(heyangtek_parts),
	.default_part_ops = &heyangtek_part_ops,
	.default_part_fixups = &heyangtek_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
};

static const struct nand_page_layout hyf1gq4utacae_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(63),
);

static const struct nand_page_layout hyf2gq4utacae_ecc_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(127),
);

static const struct spi_nand_io_opcode heyangtek01_rd_opcodes_a8d[__SPI_MEM_IO_MAX] = {
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_1, SNAND_CMD_FAST_READ_FROM_CACHE, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_2, SNAND_CMD_READ_FROM_CACHE_DUAL_OUT, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_2_2, SNAND_CMD_READ_FROM_CACHE_DUAL_IO, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_4, SNAND_CMD_READ_FROM_CACHE_QUAD_OUT, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_4_4, SNAND_CMD_READ_FROM_CACHE_QUAD_IO, 2, 8),
};

static DEFINE_SNAND_ALIAS(hyf1gq4utacae_alias, SNAND_ALIAS_MODEL("HYF1GQ4UTDCAE"));
static DEFINE_SNAND_ALIAS(hyf2gq4utacae_alias, SNAND_ALIAS_MODEL("HYF2GQ4UTDCAE"));

static ufprog_status hyf1gq4utacae_check_ecc(struct spi_nand *snand);
static ufprog_status hyf2gq4utacae_check_ecc(struct spi_nand *snand);

static const struct spi_nand_flash_part_ops hyf1gq4utacae_part_ops = {
	.check_ecc = hyf1gq4utacae_check_ecc,
};

static const struct spi_nand_flash_part_ops hyf2gq4utacae_part_ops = {
	.check_ecc = hyf2gq4utacae_check_ecc,
};

static const struct spi_nand_flash_part heyangtek01_parts[] = {
	SNAND_PART("HYF1GQ4UTACAE", SNAND_ID(SNAND_ID_DUMMY, 0x01, 0x15), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 6),
		   SNAND_ALIAS(&hyf1gq4utacae_alias),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_DONT_CARE, SNAND_ECC_ALWAYS_ON,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(heyangtek01_rd_opcodes_a8d),
		   SNAND_SPI_MAX_SPEED_MHZ(60),
		   SNAND_PAGE_LAYOUT(&hyf1gq4utacae_ecc_layout),
		   NAND_OTP_INFO(&heyangtek01_otp),
		   SNAND_OPS(&hyf1gq4utacae_part_ops),
	),

	SNAND_PART("HYF2GQ4UTACAE", SNAND_ID(SNAND_ID_DUMMY, 0x01, 0x25), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 6),
		   SNAND_ALIAS(&hyf2gq4utacae_alias),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_QE_DONT_CARE, SNAND_ECC_ALWAYS_ON,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(heyangtek01_rd_opcodes_a8d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&hyf2gq4utacae_ecc_layout),
		   NAND_OTP_INFO(&heyangtek01_otp),
		   SNAND_OPS(&hyf2gq4utacae_part_ops),
	),
};

static ufprog_status hyf1gq4utacae_check_ecc(struct spi_nand *snand)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & SPI_NAND_STATUS_ECC_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;

	if (!sr)
		return UFP_OK;

	if (sr == 1) {
		snand->ecc_status->step_bitflips[0] = 2;
		return UFP_ECC_CORRECTED;
	}

	if (sr == 2) {
		snand->ecc_status->step_bitflips[0] = 6;
		return UFP_ECC_CORRECTED;
	}

	snand->ecc_status->step_bitflips[0] = -1;

	return UFP_ECC_UNCORRECTABLE;
}

static ufprog_status hyf2gq4utacae_check_ecc(struct spi_nand *snand)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & SPI_NAND_STATUS_ECC_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;

	if (!sr)
		return UFP_OK;

	if (sr == 1)
		snand->ecc_status->step_bitflips[0] = 2;
	else if (sr == 2)
		snand->ecc_status->step_bitflips[0] = 4;
	else
		snand->ecc_status->step_bitflips[0] = 6;

	return UFP_ECC_CORRECTED;
}

static ufprog_status heyangtek01_read_uid(struct spi_nand *snand, void *data, uint32_t *retlen)
{
	return spi_nand_read_uid_otp(snand, 0x180, data, retlen);
}

static ufprog_status heyangtek01_setup_chip(struct spi_nand *snand)
{
	/* As requested by datasheet, ECC_EN must be always set */
	return spi_nand_update_config(snand, 0, SPI_NAND_CONFIG_ECC_EN);
}

static const struct spi_nand_flash_part_ops heyangtek01_part_ops = {
	.chip_setup = heyangtek01_setup_chip,
	.read_uid = heyangtek01_read_uid,
	.otp_control = spi_nand_otp_control_micron,
};

const struct spi_nand_vendor vendor_heyangtek_01 = {
	.mfr_id = SNAND_VENDOR_HEYANGTEK_01,
	.id = "heyangtek01",
	.name = "HeYangTek",
	.parts = heyangtek01_parts,
	.nparts = ARRAY_SIZE(heyangtek01_parts),
	.default_part_ops = &heyangtek01_part_ops,
	.default_part_fixups = &heyangtek_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
};
