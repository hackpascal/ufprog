// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * CoreStorage SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"

/* CoreStage ECC status bits */
#define CS_SR_ECC_STATUS_MASK				BITS(6, SPI_NAND_STATUS_ECC_SHIFT)

/* CoreStage vendor flags */
#define CS_F_ECC_CAP_4_BITS				BIT(0)
#define CS_F_ECC_CAP_8_BITS				BIT(1)

static const struct spi_nand_part_flag_enum_info cs_vendor_flag_info[] = {
	{ 0, "ecc-4-bits" },
	{ 1, "ecc-8-bits" },
};

static struct nand_otp_info cs_otp = {
	.start_index = 0,
	.count = 4,
};

static const struct nand_page_layout cs_ecc_4bits_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(31),
	ECC_PAGE_PARITY_BYTES(32),
);

static const struct nand_page_layout cs_ecc_8bits_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(63),
	ECC_PAGE_PARITY_BYTES(64),
);

static const struct spi_nand_flash_part corestorage_parts[] = {
	SNAND_PART("CS11G0-T0A0AA", SNAND_ID(SNAND_ID_DUMMY, 0x6b, 0x00), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(CS_F_ECC_CAP_8_BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&cs_ecc_8bits_layout),
		   NAND_OTP_INFO(&cs_otp),
	),

	SNAND_PART("CS11G0-G0A0AA", SNAND_ID(SNAND_ID_DUMMY, 0x6b, 0x10), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(CS_F_ECC_CAP_8_BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&cs_ecc_8bits_layout),
		   NAND_OTP_INFO(&cs_otp),
	),

	SNAND_PART("CS11G0-S0A0AA", SNAND_ID(SNAND_ID_DUMMY, 0x6b, 0x20), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(CS_F_ECC_CAP_4_BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&cs_ecc_4bits_layout),
		   NAND_OTP_INFO(&cs_otp),
	),

	SNAND_PART("CS11G1-T0A0AA", SNAND_ID(SNAND_ID_DUMMY, 0x6b, 0x01), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(CS_F_ECC_CAP_8_BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&cs_ecc_8bits_layout),
		   NAND_OTP_INFO(&cs_otp),
	),

	SNAND_PART("CS11G1-S0A0AA", SNAND_ID(SNAND_ID_DUMMY, 0x6b, 0x21), &snand_memorg_2g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(CS_F_ECC_CAP_4_BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&cs_ecc_4bits_layout),
		   NAND_OTP_INFO(&cs_otp),
	),

	SNAND_PART("CS11G2-T0A0AA", SNAND_ID(SNAND_ID_DUMMY, 0x6b, 0x02), &snand_memorg_4g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(CS_F_ECC_CAP_8_BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&cs_ecc_8bits_layout),
		   NAND_OTP_INFO(&cs_otp),
	),

	SNAND_PART("CS11G2-S0A0AA", SNAND_ID(SNAND_ID_DUMMY, 0x6b, 0x22), &snand_memorg_4g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(CS_F_ECC_CAP_4_BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&cs_ecc_4bits_layout),
		   NAND_OTP_INFO(&cs_otp),
	),
};

static ufprog_status corestorage_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	return UFP_OK;
}

static ufprog_status corestage_check_ecc_4_bits(struct spi_nand *snand)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & SPI_NAND_STATUS_ECC_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;

	if (!sr)
		return UFP_OK;

	if (sr <= 2) {
		snand->ecc_status->step_bitflips[0] = sr + 2;
		return UFP_ECC_CORRECTED;
	}

	snand->ecc_status->step_bitflips[0] = -1;

	return UFP_ECC_UNCORRECTABLE;
}

static ufprog_status corestage_check_ecc_8_bits(struct spi_nand *snand)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & CS_SR_ECC_STATUS_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;

	if (!sr)
		return UFP_OK;

	if (sr <= 6) {
		snand->ecc_status->step_bitflips[0] = sr + 2;
		return UFP_ECC_CORRECTED;
	}

	snand->ecc_status->step_bitflips[0] = -1;

	return UFP_ECC_UNCORRECTABLE;
}

static ufprog_status corestorage_part_set_ops(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	if (bp->p.vendor_flags & CS_F_ECC_CAP_4_BITS)
		snand->ext_param.ops.check_ecc = corestage_check_ecc_4_bits;
	else if (bp->p.vendor_flags & CS_F_ECC_CAP_8_BITS)
		snand->ext_param.ops.check_ecc = corestage_check_ecc_8_bits;

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup corestorage_fixups = {
	.pre_param_setup = corestorage_part_fixup,
	.post_param_setup = corestorage_part_set_ops,
};

const struct spi_nand_vendor vendor_corestorage = {
	.mfr_id = SNAND_VENDOR_CORESTORAGE,
	.id = "corestorage",
	.name = "CoreStorage",
	.parts = corestorage_parts,
	.nparts = ARRAY_SIZE(corestorage_parts),
	.default_part_fixups = &corestorage_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
	.vendor_flag_names = cs_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(cs_vendor_flag_info),
};
