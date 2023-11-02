// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Micron SPI-NOR flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/log.h>
#include <ufprog/sizes.h>
#include <ufprog/spi-nor-opcode.h>
#include "core.h"
#include "part.h"
#include "regs.h"

#define MICRON_UID_LEN				16
#define MICRON_OTP_LEN				64

/* BP bits */
#define SR_TB					BIT(5)
#define SR_BP3					BIT(6)
#define BP_3_0_TB				(SR_TB | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0)

/* Micron VCR fields */
#define MT_VCR_DC_SHIFT				4
#define MT_VCR_DC_MASK				BITS(7, MT_VCR_DC_SHIFT)

/* Micron EVCR fields */
#define MT_EVCR_QPI_DIS				BIT(7)
#define MT_EVCR_DPI_DIS				BIT(6)

/* Micron Flag register fields */
#define MT_FLAGR_4B_MODE			BIT(7)

/* Micron vendor flags */
#define MT_F_FLAG_REG				BIT(0)
#define MT_F_DC_10_VCR				BIT(1)
#define MT_F_DC_14_VCR				BIT(2)
#define MT_F_UID_14B				BIT(3)
#define MT_F_MULTI_DIE				BIT(4)

static const struct spi_nor_reg_access flagr_acc = SNOR_REG_ACC_NORMAL(SNOR_CMD_READ_FLAGR, 0);

static const struct spi_nor_part_flag_enum_info micron_vendor_flag_info[] = {
	{ 0, "flag-register" },
	{ 1, "dc-vcr-max-10" },
	{ 2, "dc-vcr-max-14" },
	{ 3, "uid-14-bytes" },
	{ 4, "multi-die" },
};

static const struct spi_nor_otp_info micron_otp = {
	.start_index = 0,
	.count = 1,
	.size = MICRON_OTP_LEN,
};

static const struct spi_nor_reg_field_item m25p_2bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def m25p_2bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, m25p_2bp_sr_fields);

static const struct snor_reg_info m25p_2bp_regs = SNOR_REG_INFO(&m25p_2bp_sr);

static const struct spi_nor_reg_field_item m25p_3bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def m25p_3bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, m25p_3bp_sr_fields);

static const struct snor_reg_info m25p_3bp_regs = SNOR_REG_INFO(&m25p_3bp_sr);

static const struct spi_nor_reg_field_item m25p_3bp_tb_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def m25p_3bp_tb_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc,
								  m25p_3bp_tb_sr_fields);

static const struct snor_reg_info m25p_3bp_tb_regs = SNOR_REG_INFO(&m25p_3bp_tb_sr);

static const struct spi_nor_reg_field_item m25p_4bp_tb_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(6, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def m25p_4bp_tb_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc,
								  m25p_4bp_tb_sr_fields);

static const struct spi_nor_reg_field_values nvcr_adp_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "4B Address"),
	VALUE_ITEM(1, "3B Address"),
);

static const struct spi_nor_reg_field_values nvcr_segsel_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "Upper 128Mb segment"),
	VALUE_ITEM(1, "Lower 128Mb segment"),
);

static const struct spi_nor_reg_field_values n25q_nvcr_ods_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(1, "90 Ohms"),
	VALUE_ITEM(2, "60 Ohms"),
	VALUE_ITEM(3, "45 Ohms"),
	VALUE_ITEM(5, "20 Ohms"),
	VALUE_ITEM(6, "15 Ohms"),
	VALUE_ITEM(7, "30 Ohms"),
);

static const struct spi_nor_reg_field_values mt25q_nvcr_ods_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(1, "90 Ohms"),
	VALUE_ITEM(3, "45 Ohms"),
	VALUE_ITEM(5, "20 Ohms"),
	VALUE_ITEM(7, "30 Ohms"),
);

static const struct spi_nor_reg_field_values nvcr_xip_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "Fast Read"),
	VALUE_ITEM(1, "Dual Output Fast Read"),
	VALUE_ITEM(2, "Dual I/O Fast Read"),
	VALUE_ITEM(3, "Quad Output Fast Read"),
	VALUE_ITEM(4, "Quad I/O Fast Read"),
	VALUE_ITEM(7, "Disabled"),
);

static const struct spi_nor_reg_field_item n25q_nvcr_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(2, 1, "DUALIO", "Dual I/O Protocol"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(3, 1, "QUADIO", "Quad I/O Protocol"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(4, 1, "RSTHOLD", "Reset/Hold"),
	SNOR_REG_FIELD_FULL(6, 7, "ODS", "Output Driver Strength", &n25q_nvcr_ods_values),
	SNOR_REG_FIELD_FULL(9, 7, "XIP", "XIP Mode", &nvcr_xip_values),
	SNOR_REG_FIELD(12, 0xf, "DC", "Dummy Cycles"),
};

static const struct spi_nor_reg_def n25q_nvcr = SNOR_REG_DEF("NVCR", "Non-volatile Configuration Register", &nvcr_acc,
							     n25q_nvcr_fields);

static const struct snor_reg_info n25q_3bp_tb_regs = SNOR_REG_INFO(&m25p_3bp_tb_sr, &n25q_nvcr);

static const struct snor_reg_info n25q_4bp_tb_regs = SNOR_REG_INFO(&m25p_4bp_tb_sr, &n25q_nvcr);

static const struct spi_nor_reg_field_item n25q_adp_nvcr_fields[] = {
	SNOR_REG_FIELD_FULL(0, 1, "ADP", "Address Bytes", &nvcr_adp_values),
	SNOR_REG_FIELD_FULL(1, 1, "SEGSEL", "128Mb Segment Select", &nvcr_segsel_values),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(2, 1, "DUALIO", "Dual I/O Protocol"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(3, 1, "QUADIO", "Quad I/O Protocol"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(4, 1, "RSTHOLD", "Reset/Hold"),
	SNOR_REG_FIELD_FULL(6, 7, "ODS", "Output Driver Strength", &n25q_nvcr_ods_values),
	SNOR_REG_FIELD_FULL(9, 7, "XIP", "XIP Mode", &nvcr_xip_values),
	SNOR_REG_FIELD(12, 0xf, "DC", "Dummy Cycles"),
};

static const struct spi_nor_reg_def n25q_adp_nvcr = SNOR_REG_DEF("NVCR", "Non-volatile Configuration Register",
								 &nvcr_acc, n25q_adp_nvcr_fields);

static const struct snor_reg_info n25q_adp_regs = SNOR_REG_INFO(&m25p_4bp_tb_sr, &n25q_adp_nvcr);

static const struct spi_nor_reg_field_item mt25q_nvcr_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(2, 1, "DUALIO", "Dual I/O Protocol"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(3, 1, "QUADIO", "Quad I/O Protocol"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(4, 1, "RSTHOLD", "Reset/Hold"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(5, 1, "DTR", "DTR Protocol"),
	SNOR_REG_FIELD_FULL(6, 7, "ODS", "Output Driver Strength", &mt25q_nvcr_ods_values),
	SNOR_REG_FIELD_FULL(9, 7, "XIP", "XIP Mode", &nvcr_xip_values),
	SNOR_REG_FIELD(12, 0xf, "DC", "Dummy Cycles"),
};

static const struct spi_nor_reg_def mt25q_nvcr = SNOR_REG_DEF("NVCR", "Non-volatile Configuration Register", &nvcr_acc,
							      mt25q_nvcr_fields);

static const struct snor_reg_info mt25q_regs = SNOR_REG_INFO(&m25p_3bp_tb_sr, &mt25q_nvcr);

static const struct spi_nor_reg_field_item mt25q_adp_nvcr_fields[] = {
	SNOR_REG_FIELD_FULL(0, 1, "ADP", "Address Bytes", &nvcr_adp_values),
	SNOR_REG_FIELD_FULL(1, 1, "SEGSEL", "128Mb Segment Select", &nvcr_segsel_values),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(2, 1, "DUALIO", "Dual I/O Protocol"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(3, 1, "QUADIO", "Quad I/O Protocol"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(4, 1, "RSTHOLD", "Reset/Hold"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(5, 1, "DTR", "DTR Protocol"),
	SNOR_REG_FIELD_FULL(6, 7, "ODS", "Output Driver Strength", &mt25q_nvcr_ods_values),
	SNOR_REG_FIELD_FULL(9, 7, "XIP", "XIP Mode", &nvcr_xip_values),
	SNOR_REG_FIELD(12, 0xf, "DC", "Dummy Cycles"),
};

static const struct spi_nor_reg_def mt25q_adp_nvcr = SNOR_REG_DEF("NVCR", "Non-volatile Configuration Register",
								  &nvcr_acc, mt25q_adp_nvcr_fields);

static const struct snor_reg_info mt25q_adp_regs = SNOR_REG_INFO(&m25p_4bp_tb_sr, &mt25q_adp_nvcr);

static const struct spi_nor_wp_info micron_wpr_4bp_tb = SNOR_WP_BP(&sr_acc, BP_3_0_TB,
	SNOR_WP_NONE( 0                                           ),	/* None */
	SNOR_WP_NONE( SR_TB                                       ),	/* None */

	SNOR_WP_ALL(           SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_TB | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_UP(                                   SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                          SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                          SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(                 SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(                 SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(                 SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(                 SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_UP(        SR_BP3                           , 7),	/* Upper 8MB */
	SNOR_WP_BP_UP(        SR_BP3 |                   SR_BP0, 8),	/* Upper 16MB */
	SNOR_WP_BP_UP(        SR_BP3 |          SR_BP1         , 9),	/* Upper 32MB */
	SNOR_WP_BP_UP(        SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Upper 64MB */
	SNOR_WP_BP_UP(        SR_BP3 | SR_BP2                  , 11),	/* Upper 128MB */
	SNOR_WP_BP_UP(        SR_BP3 | SR_BP2 | SR_BP1         , 12),	/* Upper 256MB */
	SNOR_WP_BP_UP(        SR_BP3 | SR_BP2 | SR_BP1         , 13),	/* Upper 512MB */

	SNOR_WP_BP_LO(SR_TB |                            SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(SR_TB |                   SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(SR_TB |                   SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(SR_TB |          SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(SR_TB |          SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(SR_TB |          SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
	SNOR_WP_BP_LO(SR_TB |          SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Lower 4MB */
	SNOR_WP_BP_LO(SR_TB | SR_BP3                           , 7),	/* Lower 8MB */
	SNOR_WP_BP_LO(SR_TB | SR_BP3 |                   SR_BP0, 8),	/* Lower 16MB */
	SNOR_WP_BP_LO(SR_TB | SR_BP3 |          SR_BP1         , 9),	/* Lower 32MB */
	SNOR_WP_BP_LO(SR_TB | SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Lower 64MB */
	SNOR_WP_BP_LO(SR_TB | SR_BP3 | SR_BP2                  , 11),	/* Lower 128MB */
	SNOR_WP_BP_LO(SR_TB | SR_BP3 | SR_BP2 | SR_BP1         , 12),	/* Lower 256MB */
	SNOR_WP_BP_LO(SR_TB | SR_BP3 | SR_BP2 | SR_BP1         , 13),	/* Lower 512MB */
);

static ufprog_status n25q128ax3_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt_hdr->minor_ver >= 6)
		STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "MT25QL128ABA"));

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup n25q128ax3_fixups = {
	.pre_param_setup = n25q128ax3_fixup_model,
};

static ufprog_status n25q128ax1_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt_hdr->minor_ver >= 6)
		STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "MT25QU128ABA"));

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup n25q128ax1_fixups = {
	.pre_param_setup = n25q128ax1_fixup_model,
};

static ufprog_status n25q256ax3_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt_hdr->minor_ver >= 6)
		STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "MT25QL256ABA"));

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup n25q256ax3_fixups = {
	.pre_param_setup = n25q256ax3_fixup_model,
};

static ufprog_status n25q256ax1_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt_hdr->minor_ver >= 6)
		STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "MT25QU256ABA"));

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup n25q256ax1_fixups = {
	.pre_param_setup = n25q256ax1_fixup_model,
};

static ufprog_status n25q512ax3_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt_hdr->minor_ver >= 6)
		STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "MT25QL512ABB"));

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup n25q512ax3_fixups = {
	.pre_param_setup = n25q512ax3_fixup_model,
};

static ufprog_status n25q512ax1_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt_hdr->minor_ver >= 6)
		STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "MT25QU512ABB"));

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup n25q512ax1_fixups = {
	.pre_param_setup = n25q512ax1_fixup_model,
};

static ufprog_status n25q00aax3_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt_hdr->minor_ver >= 6)
		STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "MT25QL01GBBB"));

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup n25q00aax3_fixups = {
	.pre_param_setup = n25q00aax3_fixup_model,
};

static ufprog_status n25q00aax1_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt_hdr->minor_ver >= 6)
		STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "MT25QU01GBBB"));

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup n25q00aax1_fixups = {
	.pre_param_setup = n25q00aax1_fixup_model,
};

static const struct spi_nor_erase_info m25p_erase_32k_opcodes = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_32K, SNOR_CMD_BLOCK_ERASE),
);

static const struct spi_nor_erase_info m45pe_erase_page_opcodes = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_256, SNOR_CMD_MICRON_PAGE_ERASE),
	SNOR_ERASE_SECTOR(SZ_64K, SNOR_CMD_BLOCK_ERASE),
);

static const struct spi_nor_erase_info m25pe_erase_page_opcodes = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_256, SNOR_CMD_MICRON_PAGE_ERASE),
	SNOR_ERASE_SECTOR(SZ_4K, SNOR_CMD_SECTOR_ERASE),
	SNOR_ERASE_SECTOR(SZ_64K, SNOR_CMD_BLOCK_ERASE),
);

static DEFINE_SNOR_ALIAS(mt25ql128aba_alias, SNOR_ALIAS_MODEL("MT25QL128ABB"));
static DEFINE_SNOR_ALIAS(mt25qu128aba_alias, SNOR_ALIAS_MODEL("MT25QU128ABB"));

static const struct spi_nor_flash_part micron_parts[] = {
	SNOR_PART("M25P05A", SNOR_ID(0x20, 0x20, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&m25p_erase_32k_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(25),
		  SNOR_REGS(&m25p_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("M25P10A", SNOR_ID(0x20, 0x20, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&m25p_erase_32k_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&m25p_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up_ratio),
	),

	SNOR_PART("M45PE10", SNOR_ID(0x20, 0x40, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&m45pe_erase_page_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("M25PE10", SNOR_ID(0x20, 0x80, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&m25pe_erase_page_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&m25p_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up),
	),

	SNOR_PART("M25P20", SNOR_ID(0x20, 0x20, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(25),
		  SNOR_REGS(&m25p_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up),
	),

	SNOR_PART("M45PE20", SNOR_ID(0x20, 0x40, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&m45pe_erase_page_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("M25PE20", SNOR_ID(0x20, 0x80, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&m25pe_erase_page_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&m25p_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up),
	),

	SNOR_PART("M25P40", SNOR_ID(0x20, 0x20, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(25),
		  SNOR_REGS(&m25p_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("M45PE40", SNOR_ID(0x20, 0x40, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&m45pe_erase_page_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("M25PE40", SNOR_ID(0x20, 0x80, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&m25pe_erase_page_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&m25p_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("M25P80", SNOR_ID(0x20, 0x20, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(25),
		  SNOR_REGS(&m25p_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("M45PE80", SNOR_ID(0x20, 0x40, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&m45pe_erase_page_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("M25PX80", SNOR_ID(0x20, 0x71, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&m25p_3bp_tb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("M25PE80", SNOR_ID(0x20, 0x80, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&m25pe_erase_page_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&m25p_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("M25P16", SNOR_ID(0x20, 0x20, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(25),
		  SNOR_REGS(&m25p_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("M45PE16", SNOR_ID(0x20, 0x40, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&m45pe_erase_page_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("M25PX16", SNOR_ID(0x20, 0x71, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&m25p_3bp_tb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("M25PE16", SNOR_ID(0x20, 0x80, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&m25pe_erase_page_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&m25p_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("N25Q016Ax3", SNOR_ID(0x20, 0xba, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_3bp_tb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("N25Q016Ax1", SNOR_ID(0x20, 0xbb, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_3bp_tb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("M25P32", SNOR_ID(0x20, 0x20, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&m25p_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("M25PX32", SNOR_ID(0x20, 0x71, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&m25p_3bp_tb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("N25Q032Ax3", SNOR_ID(0x20, 0xba, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_3bp_tb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("N25Q032Ax1", SNOR_ID(0x20, 0xbb, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_3bp_tb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("M25P64", SNOR_ID(0x20, 0x20, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&m25p_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up_ratio),
	),

	SNOR_PART("M25PX64", SNOR_ID(0x20, 0x71, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&m25p_3bp_tb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_ratio),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("N25Q064Ax3", SNOR_ID(0x20, 0xba, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_4bp_tb_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("N25Q064Ax1", SNOR_ID(0x20, 0xbb, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_4bp_tb_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("M25P128", SNOR_ID(0x20, 0x20, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_256K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&m25p_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up_ratio),
	),

	SNOR_PART("N25Q128Ax3", SNOR_ID(0x20, 0xba, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_4bp_tb_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
		  SNOR_FIXUPS(&n25q128ax3_fixups),
	),

	SNOR_PART("MT25QL128ABA", SNOR_ID(0x20, 0xba, 0x18), SZ_16M, /* SFDP 1.6 */
		  SNOR_ALIAS(&mt25ql128aba_alias),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_14_VCR | MT_F_UID_14B),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mt25q_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("N25Q128Ax1", SNOR_ID(0x20, 0xbb, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_4bp_tb_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
		  SNOR_FIXUPS(&n25q128ax1_fixups),
	),

	SNOR_PART("MT25QU128ABA", SNOR_ID(0x20, 0xbb, 0x18), SZ_16M, /* SFDP 1.6 */
		  SNOR_ALIAS(&mt25qu128aba_alias),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_14_VCR | MT_F_UID_14B),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_QUAD_MAX_SPEED_MHZ(145),
		  SNOR_REGS(&mt25q_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("N25Q256Ax3", SNOR_ID(0x20, 0xba, 0x19), SZ_32M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_4B_FLAGS(SNOR_4B_F_WREN_B7H_E9H | SNOR_4B_F_EAR | SNOR_4B_F_OPCODE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
		  SNOR_FIXUPS(&n25q256ax3_fixups),
	),

	SNOR_PART("MT25QL256ABA", SNOR_ID(0x20, 0xba, 0x19), SZ_32M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_14_VCR | MT_F_UID_14B),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mt25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("N25Q256Ax1", SNOR_ID(0x20, 0xbb, 0x19), SZ_32M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_4B_FLAGS(SNOR_4B_F_WREN_B7H_E9H | SNOR_4B_F_EAR | SNOR_4B_F_OPCODE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
		  SNOR_FIXUPS(&n25q256ax1_fixups),
	),

	SNOR_PART("MT25QU256ABA", SNOR_ID(0x20, 0xbb, 0x19), SZ_32M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_14_VCR | MT_F_UID_14B),
		  SNOR_SPI_MAX_SPEED_MHZ(166),
		  SNOR_REGS(&mt25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("N25Q512Ax3", SNOR_ID(0x20, 0xba, 0x20), SZ_64M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B | MT_F_MULTI_DIE),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_4B_FLAGS(SNOR_4B_F_WREN_B7H_E9H | SNOR_4B_F_EAR | SNOR_4B_F_OPCODE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
		  SNOR_FIXUPS(&n25q512ax3_fixups),
	),

	SNOR_PART("MT25QL512ABB", SNOR_ID(0x20, 0xba, 0x20), SZ_64M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_14_VCR | MT_F_UID_14B),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mt25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("N25Q512Ax1", SNOR_ID(0x20, 0xbb, 0x20), SZ_64M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B | MT_F_MULTI_DIE),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_4B_FLAGS(SNOR_4B_F_WREN_B7H_E9H | SNOR_4B_F_EAR | SNOR_4B_F_OPCODE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
		  SNOR_FIXUPS(&n25q512ax1_fixups),
	),

	SNOR_PART("MT25QU512ABB", SNOR_ID(0x20, 0xbb, 0x20), SZ_64M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_14_VCR | MT_F_UID_14B),
		  SNOR_SPI_MAX_SPEED_MHZ(166),
		  SNOR_REGS(&mt25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("N25Q00AAx3", SNOR_ID(0x20, 0xba, 0x21), SZ_128M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B | MT_F_MULTI_DIE),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_4B_FLAGS(SNOR_4B_F_WREN_B7H_E9H | SNOR_4B_F_EAR | SNOR_4B_F_OPCODE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
		  SNOR_FIXUPS(&n25q00aax3_fixups),
	),

	SNOR_PART("MT25QL01GBBB", SNOR_ID(0x20, 0xba, 0x21), SZ_128M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_14_VCR | MT_F_UID_14B | MT_F_MULTI_DIE),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mt25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("N25Q00AAx1", SNOR_ID(0x20, 0xbb, 0x21), SZ_128M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_10_VCR | MT_F_UID_14B | MT_F_MULTI_DIE),
		  SNOR_QE_NVCR_BIT4, SNOR_QPI_VENDOR,
		  SNOR_4B_FLAGS(SNOR_4B_F_WREN_B7H_E9H | SNOR_4B_F_EAR | SNOR_4B_F_OPCODE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 |
				  BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&n25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
		  SNOR_FIXUPS(&n25q00aax1_fixups),
	),

	SNOR_PART("MT25QU01GBBB", SNOR_ID(0x20, 0xbb, 0x21), SZ_128M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_14_VCR | MT_F_UID_14B | MT_F_MULTI_DIE),
		  SNOR_SPI_MAX_SPEED_MHZ(166),
		  SNOR_REGS(&mt25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("MT25QL02GCBB", SNOR_ID(0x20, 0xba, 0x22), SZ_256M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_14_VCR | MT_F_UID_14B | MT_F_MULTI_DIE),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mt25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),

	SNOR_PART("MT25QU02GCBB", SNOR_ID(0x20, 0xbb, 0x22), SZ_256M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MT_F_FLAG_REG | MT_F_DC_14_VCR | MT_F_UID_14B | MT_F_MULTI_DIE),
		  SNOR_SPI_MAX_SPEED_MHZ(166),
		  SNOR_REGS(&mt25q_adp_regs),
		  SNOR_WP_RANGES(&micron_wpr_4bp_tb),
		  SNOR_OTP_INFO(&micron_otp),
	),
};

static ufprog_status micron_dpi_en(struct spi_nor *snor)
{
	return spi_nor_update_reg_acc(snor, &evcr_acc, MT_EVCR_DPI_DIS, MT_EVCR_QPI_DIS, false);
}

static ufprog_status micron_dpi_dis(struct spi_nor *snor)
{
	return spi_nor_update_reg_acc(snor, &evcr_acc, 0, MT_EVCR_QPI_DIS | MT_EVCR_DPI_DIS, false);
}

static ufprog_status micron_qpi_en(struct spi_nor *snor)
{
	return spi_nor_update_reg_acc(snor, &evcr_acc, MT_EVCR_QPI_DIS, MT_EVCR_DPI_DIS, false);
}

static ufprog_status micron_qpi_dis(struct spi_nor *snor)
{
	return spi_nor_update_reg_acc(snor, &evcr_acc, 0, MT_EVCR_QPI_DIS | MT_EVCR_DPI_DIS, false);
}

static const uint8_t dc_10_2_2_2[] = { 8 };
static const uint8_t dc_14_2_2_2[] = { 8, 12 };
static const uint8_t dc_14_166_2_2_2[] = { 12 };

static ufprog_status micron_part_select_dummy_cycles(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	const uint8_t *dcs;
	uint32_t ndcs, i;

	/* No test for 4-4-4/1-4-4 */
	if (bp->p.vendor_flags & MT_F_DC_10_VCR)
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 10;
	else
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 14;

	bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy;
	bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;
	bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;

	/* No test for 1-1-4 */
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy = 8;
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].nmode = 0;

	/* Test for 2-2-2/1-2-2 */
	if (bp->p.vendor_flags & MT_F_DC_10_VCR) {
		dcs = dc_10_2_2_2;
		ndcs = ARRAY_SIZE(dc_10_2_2_2);
	} else {
		if (bp->p.max_speed_spi_mhz > 133) {
			dcs = dc_14_166_2_2_2;
			ndcs = ARRAY_SIZE(dc_14_166_2_2_2);
		} else {
			dcs = dc_14_2_2_2;
			ndcs = ARRAY_SIZE(dc_14_2_2_2);
		}
	}

	bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = 0;
	bp->read_opcodes_3b[SPI_MEM_IO_2_2_2].nmode = 0;

	for (i = 0; i < ndcs; i++) {
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = dcs[i];

		if (spi_nor_test_io_opcode(snor, bp->read_opcodes_3b, SPI_MEM_IO_1_2_2, 3, SPI_DATA_IN))
			break;
	}

	for (i = 0; i < ndcs; i++) {
		bp->read_opcodes_3b[SPI_MEM_IO_2_2_2].ndummy = dcs[i];

		if (spi_nor_test_io_opcode(snor, bp->read_opcodes_3b, SPI_MEM_IO_2_2_2, 3, SPI_DATA_IN))
			break;
	}

	/* No test for 1-1-2 */
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].ndummy = 8;
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].nmode = 0;

	/* No test for 1-1-1 */
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].ndummy = 8;
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].nmode = 0;

	if (bp->p.size >= SZ_32M) {
		bp->read_opcodes_4b[SPI_MEM_IO_1_1_1].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].ndummy;
		bp->read_opcodes_4b[SPI_MEM_IO_1_1_1].nmode = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_1_1_2].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].ndummy;
		bp->read_opcodes_4b[SPI_MEM_IO_1_1_2].nmode = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy;
		bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].nmode = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_2_2_2].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_2_2_2].ndummy;
		bp->read_opcodes_4b[SPI_MEM_IO_2_2_2].nmode = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_1_1_4].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy;
		bp->read_opcodes_4b[SPI_MEM_IO_1_1_4].nmode = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy;
		bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].nmode = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy;
		bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].nmode = 0;
	}

	return UFP_OK;
}

static ufprog_status micron_part_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	spi_nor_blank_part_fill_default_opcodes(bp);

	if (snor->sfdp.bfpt && snor->sfdp.bfpt_hdr->minor_ver >= SFDP_REV_MINOR_A) {
		bp->p.pp_io_caps |= BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4;
		if (snor->sfdp.bfpt_hdr->minor_ver >= SFDP_REV_MINOR_B)
			bp->p.pp_io_caps |= BIT_SPI_MEM_IO_1_4_4;

		bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].opcode = SNOR_CMD_PAGE_PROG_QUAD_IN;
		bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].nmode = 0;
		bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].opcode = SNOR_CMD_PAGE_PROG;
		bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;

		if (bp->p.size >= SZ_32M) {
			bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].opcode = SNOR_CMD_4B_PAGE_PROG_QUAD_IN;
			bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].ndummy = bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].nmode = 0;
			bp->pp_opcodes_4b[SPI_MEM_IO_4_4_4].opcode = SNOR_CMD_4B_PAGE_PROG;
			bp->pp_opcodes_4b[SPI_MEM_IO_4_4_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;
		}
	}

	if (bp->p.pp_io_caps & BIT_SPI_MEM_IO_1_1_2) {
		bp->pp_opcodes_3b[SPI_MEM_IO_1_1_2].opcode = SNOR_CMD_PAGE_PROG_DUAL_IN;
		bp->pp_opcodes_3b[SPI_MEM_IO_1_1_2].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_1_1_2].nmode = 0;
	}

	if (bp->p.pp_io_caps & BIT_SPI_MEM_IO_1_2_2) {
		bp->pp_opcodes_3b[SPI_MEM_IO_1_2_2].opcode = SNOR_CMD_PAGE_PROG_DUAL_IO;
		bp->pp_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = 0;
	}

	if (bp->p.pp_io_caps & BIT_SPI_MEM_IO_2_2_2) {
		bp->pp_opcodes_3b[SPI_MEM_IO_2_2_2].opcode = SNOR_CMD_PAGE_PROG;
		bp->pp_opcodes_3b[SPI_MEM_IO_2_2_2].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_2_2_2].nmode = 0;
	}

	if (bp->p.pp_io_caps & BIT_SPI_MEM_IO_1_4_4) {
		bp->pp_opcodes_3b[SPI_MEM_IO_1_4_4].opcode = SNOR_CMD_PAGE_PROG_QUAD_IO;
		bp->pp_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;
	}

	if (bp->p.pp_io_caps & BIT_SPI_MEM_IO_4_4_4) {
		bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].opcode = SNOR_CMD_PAGE_PROG;
		bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;
	}

	if (bp->p.size >= SZ_32M) {
		bp->pp_opcodes_4b[SPI_MEM_IO_1_1_2].opcode = 0;
		bp->pp_opcodes_4b[SPI_MEM_IO_1_2_2].opcode = 0;
		bp->pp_opcodes_4b[SPI_MEM_IO_2_2_2].opcode = 0;
		bp->pp_opcodes_4b[SPI_MEM_IO_1_4_4].opcode = 0;
	}

	if (bp->p.vendor_flags & (MT_F_DC_10_VCR | MT_F_DC_14_VCR))
		STATUS_CHECK_RET(micron_part_select_dummy_cycles(snor, bp));

	if (bp->p.vendor_flags & MT_F_MULTI_DIE) {
		if (bp->p.regs == &mt25q_adp_regs)
			snor->state.die_read_granularity = SZ_64M;
		else
			snor->state.die_read_granularity = SZ_32M;
	}

	if (bp->p.pp_io_caps & BIT_SPI_MEM_IO_2_2_2) {
		snor->ext_param.ops.dpi_en = micron_dpi_en;
		snor->ext_param.ops.dpi_dis = micron_dpi_dis;
	}

	if (bp->p.pp_io_caps & BIT_SPI_MEM_IO_4_4_4) {
		snor->ext_param.ops.qpi_en = micron_qpi_en;
		snor->ext_param.ops.qpi_dis = micron_qpi_dis;
	}

	return UFP_OK;
};

static const struct spi_nor_flash_part_fixup micron_fixups = {
	.pre_param_setup = micron_part_fixup,
};

static ufprog_status micron_otp_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_MICRON_READ_OTP, 1),
		SPI_MEM_OP_ADDR(3, addr, 1),
		SPI_MEM_OP_DUMMY(1, 1),
		SPI_MEM_OP_DATA_IN(len, data, 1)
	);

	if (snor->state.a4b_mode)
		op.addr.len = 4;

	if (!ufprog_spi_mem_supports_op(snor->spi, &op))
		return UFP_UNSUPPORTED;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	while (len) {
		STATUS_CHECK_RET(ufprog_spi_mem_adjust_op_size(snor->spi, &op));
		STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));

		op.data.buf.rx = (void *)((uintptr_t)op.data.buf.rx + op.data.len);

		addr += (uint32_t)op.data.len;
		op.addr.val = addr;

		len -= (uint32_t)op.data.len;
		op.data.len = len;
	}

	return UFP_OK;
}

static ufprog_status micron_otp_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, const void *data)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_MICRON_PROG_OTP, 1),
		SPI_MEM_OP_ADDR(3, addr, 1),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_DATA_OUT(len, data, 1)
	);

	if (snor->state.a4b_mode)
		op.addr.len = 4;

	if (!ufprog_spi_mem_supports_op(snor->spi, &op))
		return UFP_UNSUPPORTED;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	while (len) {
		STATUS_CHECK_RET(spi_nor_write_enable(snor));

		STATUS_CHECK_RET(ufprog_spi_mem_adjust_op_size(snor->spi, &op));
		STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));

		STATUS_CHECK_RET(spi_nor_wait_busy(snor, SNOR_PP_TIMEOUT_MS));

		op.data.buf.tx = (const void *)((uintptr_t)op.data.buf.tx + op.data.len);

		addr += (uint32_t)op.data.len;
		op.addr.val = addr;

		len -= (uint32_t)op.data.len;
		op.data.len = len;
	}

	return UFP_OK;
}

static ufprog_status micron_otp_lock(struct spi_nor *snor, uint32_t index)
{
	uint8_t data = 0;

	return micron_otp_write(snor, 0, MICRON_OTP_LEN, 1, &data);
}

static ufprog_status micron_otp_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{
	uint8_t data;

	STATUS_CHECK_RET(micron_otp_read(snor, 0, MICRON_OTP_LEN, 1, &data));

	*retlocked = (data & 1) ? false : true;

	return UFP_OK;
}

static const struct spi_nor_flash_part_otp_ops micron_otp_ops = {
	.read = micron_otp_read,
	.write = micron_otp_write,
	.lock = micron_otp_lock,
	.locked = micron_otp_locked,
};

static ufprog_status micron_chip_setup(struct spi_nor *snor)
{
	uint32_t val, ndummy;

	if (snor->param.vendor_flags & (MT_F_DC_10_VCR | MT_F_DC_14_VCR)) {
		ndummy = snor->state.read_ndummy * 8 / spi_mem_io_info_addr_bw(snor->state.read_io_info);
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &vcr_acc, MT_VCR_DC_MASK,
							ndummy << MT_VCR_DC_SHIFT, false));
		STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &vcr_acc, &val));

		val = (val & MT_VCR_DC_MASK) >> MT_VCR_DC_SHIFT;

		if (val != ndummy) {
			logm_err("Failed to set read dummy cycles to %u\n", ndummy);
			return UFP_UNSUPPORTED;
		}
	}

	if (snor->param.size > SZ_16M) {
		STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &flagr_acc, &val));

		if (val & MT_FLAGR_4B_MODE) {
			/* Restore 3B mode by default */
			spi_nor_disable_4b_addressing_e9h(snor);
		}
	}

	return UFP_OK;
}

static ufprog_status micron_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	uint8_t id[20];
	struct ufprog_spi_mem_op op = SNOR_READ_ID_OP(SNOR_CMD_READ_ID, 1, sizeof(id), 0, id);
	uint32_t i, p = 0, s = 4;

	if (!ufprog_spi_mem_supports_op(snor->spi, &op))
		return UFP_UNSUPPORTED;

	STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));

	if (id[3] != MICRON_UID_LEN)
		return UFP_UNSUPPORTED;

	if (snor->param.vendor_flags & MT_F_UID_14B)
		p = 2;

	for (i = s + p + 1; i < sizeof(id); i++) {
		if (id[i] != id[s + p])
			break;
	}

	if (i == sizeof(id))
		return UFP_UNSUPPORTED;

	if (retlen)
		*retlen = MICRON_UID_LEN - p;

	if (!data)
		return UFP_OK;

	memcpy(data, id + s + p, MICRON_UID_LEN - p);

	return UFP_OK;
}

static const struct spi_nor_flash_part_ops micron_default_part_ops = {
	.otp = &micron_otp_ops,

	.chip_setup = micron_chip_setup,
	.read_uid = micron_read_uid,
	.dpi_en = micron_dpi_en,
	.dpi_dis = micron_dpi_dis,
	.qpi_en = micron_qpi_en,
	.qpi_dis = micron_qpi_dis,
};

const struct spi_nor_vendor vendor_micron = {
	.mfr_id = SNOR_VENDOR_MICRON,
	.id = "micron",
	.name = "Micron/Numonyx",
	.parts = micron_parts,
	.nparts = ARRAY_SIZE(micron_parts),
	.vendor_flag_names = micron_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(micron_vendor_flag_info),
	.default_part_ops = &micron_default_part_ops,
	.default_part_fixups = &micron_fixups,
};
