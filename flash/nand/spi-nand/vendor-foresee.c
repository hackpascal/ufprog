// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * FORESEE SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include <ufprog/log.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"

/* FORESEE feature address */
#define SPI_NAND_FEATURE_FORESEE_ECC_Sx_STATUS_ADDR(x)		(0x80 + (x) * 4)

static struct nand_otp_info fs35xqa_otp = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 62,
};

static struct nand_otp_info fs35nd04g_otp = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 10,
};

static const struct nand_page_layout fs35_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(63),
);

static struct spi_nand_flash_part_ops fs35nd04g_ops = {
	.check_ecc = spi_nand_check_ecc_1bit_per_step,
};

static const struct spi_nand_flash_part foresee_parts[] = {

	SNAND_PART("FS35SQA512M", SNAND_ID(SNAND_ID_DUMMY, 0xcd, 0x70, 0x70), &snand_memorg_512m_2k_64,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&fs35_layout),
		   NAND_OTP_INFO(&fs35xqa_otp),
	),

	SNAND_PART("FS35UQA512M", SNAND_ID(SNAND_ID_DUMMY, 0xcd, 0x60, 0x60), &snand_memorg_512m_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&fs35_layout),
		   NAND_OTP_INFO(&fs35xqa_otp),
	),

	SNAND_PART("FS35SQA001G", SNAND_ID(SNAND_ID_DUMMY, 0xcd, 0x71, 0x71), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&fs35_layout),
		   NAND_OTP_INFO(&fs35xqa_otp),
	),

	SNAND_PART("FS35UQA001G", SNAND_ID(SNAND_ID_DUMMY, 0xcd, 0x61, 0x61), &snand_memorg_1g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(66),
		   SNAND_PAGE_LAYOUT(&fs35_layout),
		   NAND_OTP_INFO(&fs35xqa_otp),
	),

	SNAND_PART("FS35SQA002G", SNAND_ID(SNAND_ID_DUMMY, 0xcd, 0x72, 0x72), &snand_memorg_2g_2k_64,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&fs35_layout),
		   NAND_OTP_INFO(&fs35xqa_otp),
	),

	SNAND_PART("FS35UQA002G", SNAND_ID(SNAND_ID_DUMMY, 0xcd, 0x62, 0x62), &snand_memorg_2g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(83),
		   SNAND_PAGE_LAYOUT(&fs35_layout),
		   NAND_OTP_INFO(&fs35xqa_otp),
	),

	SNAND_PART("FS35ND04G-S2Y2", SNAND_ID(SNAND_ID_DUMMY, 0xcd, 0xec, 0x11), &snand_memorg_4g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&fs35_layout),
		   NAND_OTP_INFO(&fs35nd04g_otp),
		   SNAND_OPS(&fs35nd04g_ops),
	),
};

static ufprog_status spi_nand_check_ecc_fs35xqa(struct spi_nand *snand)
{
	bool ecc_err = false, ecc_corr = false;
	ufprog_status ret;
	uint32_t i;
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & SPI_NAND_STATUS_ECC_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;
	if (!sr)
		return UFP_OK;

	snand->ecc_status->per_step = true;

	if (sr == 1)
		ecc_corr = true;

	if (sr >= 2)
		ecc_err = true;

	for (i = 0; i < snand->state.ecc_steps; i++) {
		ret = spi_nand_get_feature(snand, SPI_NAND_FEATURE_FORESEE_ECC_Sx_STATUS_ADDR(i), &sr);
		if (ret) {
			logm_err("Failed to get ECC status of sector %u\n", i);
			return ret;
		}

		switch (sr) {
		case 0:
			break;

		case 1:
			snand->ecc_status->step_bitflips[i] = snand->nand.ecc_req.strength_per_step;
			break;

		default:
			snand->ecc_status->step_bitflips[i] = -1;
		}
	}

	if (ecc_err)
		return UFP_ECC_UNCORRECTABLE;

	if (ecc_corr)
		return UFP_ECC_CORRECTED;

	return UFP_OK;
}

static ufprog_status foresee_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup foresee_fixups = {
	.pre_param_setup = foresee_part_fixup,
};

static const struct spi_nand_flash_part_ops foresee_part_ops = {
	.check_ecc = spi_nand_check_ecc_fs35xqa,
};

static ufprog_status foresee_pp_post_init(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bp->p.qe_type = QE_CR_BIT0;
	bp->p.ecc_type = ECC_CR_BIT4;
	bp->p.otp_en_type = OTP_CR_BIT6;

	bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4;
	bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4;

	return UFP_OK;
}

static const struct spi_nand_vendor_ops foresee_ops = {
	.pp_post_init = foresee_pp_post_init,
};

const struct spi_nand_vendor vendor_foresee = {
	.mfr_id = SNAND_VENDOR_FORESEE,
	.id = "foresee",
	.name = "FORESEE",
	.parts = foresee_parts,
	.nparts = ARRAY_SIZE(foresee_parts),
	.ops = &foresee_ops,
	.default_part_ops = &foresee_part_ops,
	.default_part_fixups = &foresee_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
};
