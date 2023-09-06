// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Winbond SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"

/* Winbond feature address */
#define SPI_NAND_FEATURE_WINBOND_STATUS4_ADDR		0xd0
#define WINBOND_SR4_HS					BIT(2)

/* Winbond configuration bits */
#define SPI_NAND_CONFIG_WINBOND_BUF_EN			BIT(3)

/* Winbond vendor flags */
#define WINBOND_F_HS_BIT				BIT(0)

static const struct spi_nand_part_flag_enum_info winbond_vendor_flag_info[] = {
	{ 0, "hs-bit" },
};

static struct nand_otp_info w25n_otp = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 10,
};

static const struct nand_page_layout w25n01kv_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_PARITY_BYTES(7),
	ECC_PAGE_UNUSED_BYTES(1),
	ECC_PAGE_PARITY_BYTES(7),
	ECC_PAGE_UNUSED_BYTES(1),
	ECC_PAGE_PARITY_BYTES(7),
	ECC_PAGE_UNUSED_BYTES(1),
	ECC_PAGE_PARITY_BYTES(7),
	ECC_PAGE_UNUSED_BYTES(1),
);

static const struct nand_page_layout w25nxxk_ecc8bit_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_PARITY_BYTES(13),
	ECC_PAGE_UNUSED_BYTES(3),
	ECC_PAGE_PARITY_BYTES(13),
	ECC_PAGE_UNUSED_BYTES(3),
	ECC_PAGE_PARITY_BYTES(13),
	ECC_PAGE_UNUSED_BYTES(3),
	ECC_PAGE_PARITY_BYTES(13),
	ECC_PAGE_UNUSED_BYTES(3),
);

static const struct nand_page_layout w25nxxjw_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(6),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(4),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(6),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(4),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(6),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(4),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(6),
	ECC_PAGE_OOB_DATA_BYTES(4),
	ECC_PAGE_PARITY_BYTES(4),
);

static const struct nand_memorg snand_memorg_1g_2k_96 = SNAND_MEMORG(2048, 96, 64, 1024, 1, 1);

static ufprog_status w25n01xv_fixup_model(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	if (!snand->onfi.valid)
		return UFP_OK;

	ufprog_pp_read_str(snand->onfi.data, bp->model, sizeof(bp->model), PP_MODEL_OFFS, PP_MODEL_LEN);

	if (strncmp(bp->model, "W25N01KV", 8))
		return UFP_OK;

	return spi_nand_reprobe_part(snand, bp, NULL, "W25N01KV");
}

static struct spi_nand_flash_part_fixup w25n01gv_fixup = {
	.pre_param_setup = w25n01xv_fixup_model,
};

static struct spi_nand_flash_part_ops w25n01kv_ops = {
	.check_ecc = spi_nand_check_extended_ecc_bfr_4b,
};

static const struct spi_nand_flash_part winbond_parts[] = {
	SNAND_PART("W25N512GV", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xaa, 0x20), &snand_memorg_512m_2k_64,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&w25n_otp),
	),

	SNAND_PART("W25N512GW", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xba, 0x20), &snand_memorg_512m_2k_64,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&w25n_otp),
	),

	SNAND_PART("W25N01GV", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xaa, 0x21), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&w25n_otp),
		   SNAND_FIXUPS(&w25n01gv_fixup),
	),

	SNAND_PART("W25N01KV", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xae, 0x21), &snand_memorg_1g_2k_96,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&w25n01kv_layout),
		   NAND_OTP_INFO(&w25n_otp),
		   SNAND_OPS(&w25n01kv_ops),
	),

	SNAND_PART("W25N01GW", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xba, 0x21), &snand_memorg_1g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&w25n_otp),
	),

	SNAND_PART("W25N01JW", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xbc, 0x21), &snand_memorg_1g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_VENDOR_FLAGS(WINBOND_F_HS_BIT),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(166),
		   SNAND_PAGE_LAYOUT(&w25nxxjw_layout),
		   NAND_OTP_INFO(&w25n_otp),
	),

	SNAND_PART("W25M02GV", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xab, 0x21), &snand_memorg_2g_2k_64_2d,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&w25n_otp),
	),

	SNAND_PART("W25M02GW", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xbb, 0x21), &snand_memorg_2g_2k_64_2d, /* 1.8V */
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&w25n_otp),
	),

	SNAND_PART("W25N02KV", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xaa, 0x22), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&w25nxxk_ecc8bit_layout),
		   NAND_OTP_INFO(&w25n_otp),
	),

	SNAND_PART("W25N02KW", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xba, 0x22), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&w25nxxk_ecc8bit_layout),
		   NAND_OTP_INFO(&w25n_otp),
	),

	SNAND_PART("W25N02JW", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xbf, 0x22), &snand_memorg_2g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_VENDOR_FLAGS(WINBOND_F_HS_BIT),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(166),
		   SNAND_PAGE_LAYOUT(&w25nxxjw_layout),
		   NAND_OTP_INFO(&w25n_otp),
	),

	SNAND_PART("W25N04KV", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xaa, 0x23), &snand_memorg_4g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&w25nxxk_ecc8bit_layout),
		   NAND_OTP_INFO(&w25n_otp),
	),

	SNAND_PART("W25N04KW", SNAND_ID(SNAND_ID_DUMMY, 0xef, 0xba, 0x23), &snand_memorg_4g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_EXTENDED_ECC_BFR_8B),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&w25nxxk_ecc8bit_layout),
		   NAND_OTP_INFO(&w25n_otp),
	),
};

static ufprog_status winbond_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	uint8_t val;

	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	if (bp->p.vendor_flags & WINBOND_F_HS_BIT) {
		STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_WINBOND_STATUS4_ADDR, &val));
		val |= WINBOND_SR4_HS;
		STATUS_CHECK_RET(spi_nand_set_feature(snand, SPI_NAND_FEATURE_WINBOND_STATUS4_ADDR, val));

		STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_WINBOND_STATUS4_ADDR, &val));

		if (val & WINBOND_SR4_HS) {
			if (bp->p.rd_opcodes != bp->rd_opcodes) {
				memcpy(&bp->rd_opcodes, bp->p.rd_opcodes, sizeof(bp->rd_opcodes));
				bp->p.rd_opcodes = bp->rd_opcodes;
			}

			if (bp->rd_opcodes[SPI_MEM_IO_1_2_2].ndummy)
				bp->rd_opcodes[SPI_MEM_IO_1_2_2].ndummy = 8;

			if (bp->rd_opcodes[SPI_MEM_IO_1_4_4].ndummy)
				bp->rd_opcodes[SPI_MEM_IO_1_4_4].ndummy = 8;
		}
	}

	return UFP_OK;
}

static ufprog_status winbond_part_set_bbm_config(struct spi_nand *snand)
{
	snand->ecc.bbm_config.check.width = 16;
	snand->ecc.bbm_config.mark.bytes = 2;

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup winbond_fixups = {
	.pre_param_setup = winbond_part_fixup,
	.pre_chip_setup = winbond_part_set_bbm_config,
};

static ufprog_status winbond_setup_chip(struct spi_nand *snand)
{
	return spi_nand_update_config(snand, 0, SPI_NAND_CONFIG_WINBOND_BUF_EN);
}

static const struct spi_nand_flash_part_ops winbond_part_ops = {
	.chip_setup = winbond_setup_chip,
	.select_die = spi_nand_select_die_c2h,
	.check_ecc = spi_nand_check_ecc_1bit_per_step,
};

static ufprog_status winbond_pp_post_init(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bp->p.qe_type = QE_CR_BIT0;
	bp->p.ecc_type = ECC_UNKNOWN;
	bp->p.otp_en_type = OTP_CR_BIT6;

	bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4;
	bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4;

	return UFP_OK;
}

static const struct spi_nand_vendor_ops winbond_ops = {
	.pp_post_init = winbond_pp_post_init,
};

const struct spi_nand_vendor vendor_winbond = {
	.mfr_id = SNAND_VENDOR_WINBOND,
	.id = "winbond",
	.name = "Winbond",
	.parts = winbond_parts,
	.nparts = ARRAY_SIZE(winbond_parts),
	.ops = &winbond_ops,
	.default_part_ops = &winbond_part_ops,
	.default_part_fixups = &winbond_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
	.vendor_flag_names = winbond_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(winbond_vendor_flag_info),
};
