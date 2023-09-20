// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ISSI SPI-NOR flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/log.h>
#include <ufprog/sizes.h>
#include <ufprog/spi-nor-opcode.h>
#include "core.h"
#include "part.h"
#include "regs.h"
#include "otp.h"

#define ISSI_UID_LEN				16

#define ISSI_FR_OTP_IRL_SHIFT			4

 /* SR1 bits */
#define SR_BP3					BIT(5)
#define SR_TB					BIT(5)

 /* FR bits */
#define FR_TBS					BIT(1)

/* BP Masks */
#define BP_1_0					(SR_BP1 | SR_BP0)
#define BP_2_0_TB				(SR_TB | SR_BP2 | SR_BP1 | SR_BP0)
#define BP_3_0					(SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0)

/* ISSI vendor flags */
#define ISSI_F_OTP_NO_ERASE			BIT(0)
#define ISSI_F_OTP_CB_MODE			BIT(1)
#define ISSI_F_OTP_WB_MODE			BIT(2)
#define ISSI_F_RR_DC_BIT4_3			BIT(3)
#define ISSI_F_RR_DC_BIT6_3			BIT(4)
#define ISSI_F_ECC				BIT(5)
#define ISSI_F_WP_TBS				BIT(6)

static const struct spi_nor_part_flag_enum_info issi_vendor_flag_info[] = {
	{ 0, "otp-no-erase" },
	{ 1, "otp-control-byte-mode" },
	{ 2, "otp-winbond-mode" },
	{ 3, "rr-dc-bit4-3" },
	{ 4, "rr-dc-bit6-3" },
	{ 5, "ecc" },
	{ 6, "wp-tbs" },
};

static const struct spi_nor_reg_access issi_fr_acc = SNOR_REG_ACC_NORMAL(SNOR_CMD_READ_FR, SNOR_CMD_WRITE_FR);

static const struct spi_nor_reg_access issi_rr_acc = {
	.type = SNOR_REG_NORMAL,
	.num = 1,
	.desc[0] = {
		.flags = SNOR_REGACC_F_HAS_VOLATILE_WR_OPCODE | SNOR_REGACC_F_NO_WREN,
		.read_opcode = SNOR_CMD_READ_READ_PARAMETERS,
		.write_opcode = SNOR_CMD_SET_READ_PARAMETERS_NV,
		.write_opcode_volatile = SNOR_CMD_SET_READ_PARAMETERS,
		.ndata = 1,
	},
};

static const struct spi_nor_reg_access issi_err_acc = {
	.type = SNOR_REG_NORMAL,
	.num = 1,
	.desc[0] = {
		.flags = SNOR_REGACC_F_HAS_VOLATILE_WR_OPCODE | SNOR_REGACC_F_NO_WREN,
		.read_opcode = SNOR_CMD_READ_EXT_READ_PARAMETERS,
		.write_opcode = SNOR_CMD_SET_EXT_READ_PARAMETERS_NV,
		.write_opcode_volatile = SNOR_CMD_SET_EXT_READ_PARAMETERS_V,
		.ndata = 1,
	},
};

static const struct spi_nor_reg_access issi_abr_acc = {
	.type = SNOR_REG_NORMAL,
	.num = 1,
	.desc[0] = {
		.read_opcode = SNOR_CMD_READ_AUTOBOOT_REG,
		.write_opcode = SNOR_CMD_WRITE_AUTOBOOT_REG,
		.ndata = 4,
	},
};

static const struct spi_nor_reg_access issi_bar_acc = {
	.type = SNOR_REG_NORMAL,
	.num = 1,
	.desc[0] = {
		.flags = SNOR_REGACC_F_HAS_VOLATILE_WR_OPCODE | SNOR_REGACC_F_NO_WREN,
		.read_opcode = SNOR_CMD_READ_BANK,
		.write_opcode = SNOR_CMD_WRITE_BANK_NV,
		.write_opcode_volatile = SNOR_CMD_WRITE_BANK,
		.ndata = 1,
	},
};

static const struct spi_nor_reg_access issi_dlpr_acc = {
	.type = SNOR_REG_NORMAL,
	.num = 1,
	.desc[0] = {
		.flags = SNOR_REGACC_F_HAS_VOLATILE_WR_OPCODE | SNOR_REGACC_F_NO_WREN,
		.read_opcode = SNOR_CMD_READ_DLP_REG,
		.write_opcode = SNOR_CMD_WRITE_DLP_REG_NV,
		.write_opcode_volatile = SNOR_CMD_WRITE_DLP_REG_V,
		.ndata = 1,
	},
};

static const struct spi_nor_reg_field_item is25cd_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def is25cd_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, is25cd_sr_fields);

static const struct snor_reg_info is25cd_regs = SNOR_REG_INFO(&is25cd_sr);

static const struct spi_nor_reg_field_item is25lq_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD_YES_NO(6, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def is25lq_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, is25lq_sr_fields);

static const struct spi_nor_reg_field_item is25lq_fr_fields[] = {
	SNOR_REG_FIELD(4, 1, "IRL0", "Information Row 0 Lock"),
	SNOR_REG_FIELD(5, 1, "IRL1", "Information Row 1 Lock"),
	SNOR_REG_FIELD(6, 1, "IRL2", "Information Row 2 Lock"),
	SNOR_REG_FIELD(7, 1, "IRL3", "Information Row 3 Lock"),
};

static const struct spi_nor_reg_def is25lq_fr = SNOR_REG_DEF("FR", "Function Register", &issi_fr_acc, is25lq_fr_fields);

static const struct snor_reg_info is25lqxc_regs = SNOR_REG_INFO(&is25lq_sr);
static const struct snor_reg_info is25lqxb_regs = SNOR_REG_INFO(&is25lq_sr, &is25lq_fr);

static const struct spi_nor_reg_field_item is25xpxab_fr_fields[] = {
	SNOR_REG_FIELD(1, 1, "TBS", "Top/Bottom Selection"),
	SNOR_REG_FIELD(4, 1, "IRL0", "Information Row 0 Lock"),
	SNOR_REG_FIELD(5, 1, "IRL1", "Information Row 1 Lock"),
	SNOR_REG_FIELD(6, 1, "IRL2", "Information Row 2 Lock"),
	SNOR_REG_FIELD(7, 1, "IRL3", "Information Row 3 Lock"),
};

static const struct spi_nor_reg_def is25xpxab_fr = SNOR_REG_DEF("FR", "Function Register", &issi_fr_acc,
								is25xpxab_fr_fields);

static const struct spi_nor_reg_field_values issi_rr_bl_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "8-Byte"),
	VALUE_ITEM(1, "16-Byte"),
	VALUE_ITEM(2, "32-Byte"),
	VALUE_ITEM(3, "64-Byte"),
);

static const struct spi_nor_reg_field_values issi_err_ods_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(1, "12.5%"),
	VALUE_ITEM(2, "25%"),
	VALUE_ITEM(3, "37.5%"),
	VALUE_ITEM(5, "75%"),
	VALUE_ITEM(6, "100%"),
	VALUE_ITEM(7, "50%"),
);

static const struct spi_nor_reg_field_item is25xpxab_rr_fields[] = {
	SNOR_REG_FIELD_FULL(0, 3, "BL", "Burst Length", &issi_rr_bl_values),
	SNOR_REG_FIELD_YES_NO(2, 1, "WE", "Wrap Enable"),
	SNOR_REG_FIELD(3, 3, "DC", "Dummy Cycles"),
	SNOR_REG_FIELD_FULL(5, 7, "ODS", "Output Driver Strength", &issi_err_ods_values),
};

static const struct spi_nor_reg_def is25xpxab_rr = SNOR_REG_DEF("RR", "Read Register", &issi_rr_acc,
								is25xpxab_rr_fields);

static const struct snor_reg_info is25xpxab_regs = SNOR_REG_INFO(&is25lq_sr, &is25xpxab_fr, &is25xpxab_rr);

static const struct spi_nor_reg_field_item is25xpxd_rr_fields[] = {
	SNOR_REG_FIELD_FULL(0, 3, "BL", "Burst Length", &issi_rr_bl_values),
	SNOR_REG_FIELD_YES_NO(2, 1, "BLSE", "Burst Length Set Enable"),
	SNOR_REG_FIELD(3, 0xf, "DC", "Dummy Cycles"),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "HOLD#/RESET# Pin Selection", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def is25xpxd_rr = SNOR_REG_DEF("RR", "Read Register", &issi_rr_acc, is25xpxd_rr_fields);

static const struct spi_nor_reg_field_item is25xpxd_err_fields[] = {
	SNOR_REG_FIELD_FULL(5, 7, "ODS", "Output Driver Strength", &issi_err_ods_values),
};

static const struct spi_nor_reg_def is25xpxd_err = SNOR_REG_DEF("ERR", "Extended Read Register", &issi_err_acc,
								is25xpxd_err_fields);

static const struct spi_nor_reg_field_item is25xpxd_abr_fields[] = {
	SNOR_REG_FIELD_YES_NO(0, 1, "ABE", "AutoBoot Enable"),
	SNOR_REG_FIELD(1, 0xf, "ABSD", "AutoBoot Start Delay"),
	SNOR_REG_FIELD(5, 0x7ffff, "ABSA", "AutoBoot Start Address"),
};

static const struct spi_nor_reg_def is25xpxd_abr = SNOR_REG_DEF("ABR", "AutoBoot Register", &issi_abr_acc,
								is25xpxd_abr_fields);

static const struct snor_reg_info is25xpxd_regs = SNOR_REG_INFO(&is25lq_sr, &is25lq_fr, &is25xpxd_rr, &is25xpxd_err,
								&is25xpxd_abr);

static const struct spi_nor_reg_field_item is25xpxd_fr_fields[] = {
	SNOR_REG_FIELD_YES_NO(0, 1, "DRSTDIS", "Dedicated RESET# Disable"),
	SNOR_REG_FIELD(4, 1, "IRL0", "Information Row 0 Lock"),
	SNOR_REG_FIELD(5, 1, "IRL1", "Information Row 1 Lock"),
	SNOR_REG_FIELD(6, 1, "IRL2", "Information Row 2 Lock"),
	SNOR_REG_FIELD(7, 1, "IRL3", "Information Row 3 Lock"),
};

static const struct spi_nor_reg_def is25xpxd_fr = SNOR_REG_DEF("FR", "Function Register", &issi_fr_acc,
							       is25xpxd_fr_fields);

static const struct snor_reg_info is25xpxd_16pins_regs = SNOR_REG_INFO(&is25lq_sr, &is25xpxd_fr, &is25xpxd_rr,
								       &is25xpxd_err, &is25xpxd_abr);

static const struct spi_nor_reg_field_item is25xpxa_16pins_fr_fields[] = {
	SNOR_REG_FIELD_YES_NO(0, 1, "DRSTDIS", "Dedicated RESET# Disable"),
	SNOR_REG_FIELD(1, 1, "TBS", "Top/Bottom Selection"),
	SNOR_REG_FIELD(4, 1, "IRL0", "Information Row 0 Lock"),
	SNOR_REG_FIELD(5, 1, "IRL1", "Information Row 1 Lock"),
	SNOR_REG_FIELD(6, 1, "IRL2", "Information Row 2 Lock"),
	SNOR_REG_FIELD(7, 1, "IRL3", "Information Row 3 Lock"),
};

static const struct spi_nor_reg_def is25xpxa_16pins_fr = SNOR_REG_DEF("FR", "Function Register", &issi_fr_acc,
								      is25xpxa_16pins_fr_fields);

static const struct snor_reg_info is25xpxa_16pins_regs = SNOR_REG_INFO(&is25lq_sr, &is25xpxa_16pins_fr, &is25xpxd_rr,
								       &is25xpxd_err, &is25xpxd_abr);

static const struct spi_nor_reg_field_item is25xpxd_bar_fields[] = {
	SNOR_REG_FIELD_FULL(7, 1, "EXTADD", "3-Byte or 4-Byte Addressing Selection", &w25q_sr3_adp_values),
};

static const struct spi_nor_reg_def is25xpxd_bar = SNOR_REG_DEF("BAR", "Bank Address Register", &issi_bar_acc,
								is25xpxd_bar_fields);

static const struct snor_reg_info is25xpxa_16pins_4b_regs = SNOR_REG_INFO(&is25lq_sr, &is25xpxa_16pins_fr, &is25xpxd_rr,
								       &is25xpxd_err, &is25xpxd_abr, &is25xpxd_bar);

static const struct spi_nor_reg_field_item is25xp512m_err_fields[] = {
	SNOR_REG_FIELD_YES_NO(4, 1, "DLPEN", "Data Learning Pattern Enable"),
	SNOR_REG_FIELD_FULL(5, 7, "ODS", "Output Driver Strength", &issi_err_ods_values),
};

static const struct spi_nor_reg_def is25xp512m_err = SNOR_REG_DEF("ERR", "Extended Read Register", &issi_err_acc,
								  is25xp512m_err_fields);

static const struct spi_nor_reg_field_item is25xp512m_dlpr_fields[] = {
	SNOR_REG_FIELD(0, 0xff, "DLP", "Data Learning Pattern"),
};

static const struct spi_nor_reg_def is25xp512m_dlpr = SNOR_REG_DEF("DLPR", "Data Learning Pattern Register",
								   &issi_dlpr_acc, is25xp512m_dlpr_fields);

static const struct snor_reg_info is25xp512m_regs = SNOR_REG_INFO(&is25lq_sr, &is25xpxa_16pins_fr, &is25xpxd_rr,
								  &is25xp512m_err, &is25xpxd_abr, &is25xpxd_bar,
								  &is25xp512m_dlpr);

static const struct spi_nor_reg_field_item is25wjxf_sr3_fields[] = {
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def is25wjxf_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								is25wjxf_sr3_fields);

static const struct snor_reg_info is25wjxf_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &is25wjxf_sr3);

static const struct snor_reg_info is25xe128e_regs = SNOR_REG_INFO(&is25lq_sr, &is25xpxa_16pins_fr, &is25xpxd_rr,
								  &is25xp512m_err, &is25xpxd_abr, &is25xp512m_dlpr);

static const struct spi_nor_otp_info issi_otp_1 = {
	.start_index = 0,
	.count = 1,
	.size = 0x100,
};

static const struct spi_nor_otp_info issi_otp_1_64b = {
	.start_index = 0,
	.count = 1,
	.size = 0x41,
};

static const struct spi_nor_otp_info issi_otp_4 = {
	.start_index = 0,
	.count = 4,
	.size = 0x100,
};

static const struct spi_nor_otp_info issi_otp_4_512b = {
	.start_index = 0,
	.count = 4,
	.size = 0x200,
};

static const struct spi_nor_otp_info issi_otp_3_1k = {
	.start_index = 1,
	.count = 4,
	.size = 0x400,
};

static const struct spi_nor_wp_info is25cd512_wpr_2bp = SNOR_WP_BP(&sr_acc, BP_1_0,
	SNOR_WP_BP_UP(0              , -1),	/* None */
	SNOR_WP_BP_UP(         SR_BP0, -1),	/* None */
	SNOR_WP_BP_UP(SR_BP1         , -1),	/* None */

	SNOR_WP_BP_UP(SR_BP1 | SR_BP0, -2),	/* All */
);

static const struct spi_nor_wp_info is25xp010ec_wpr_3bp_tb = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_BP_UP(0                               , -1),	/* None */

	SNOR_WP_BP_UP(                 SR_BP1 | SR_BP0, -2),	/* All */
	SNOR_WP_BP_UP(        SR_BP2                  , -2),	/* All */
	SNOR_WP_BP_UP(        SR_BP2 |          SR_BP0, -2),	/* All */
	SNOR_WP_BP_UP(        SR_BP2 | SR_BP1         , -2),	/* All */
	SNOR_WP_BP_UP(        SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */
	SNOR_WP_BP_UP(SR_TB                           , -2),	/* All */
	SNOR_WP_BP_UP(SR_TB |                   SR_BP0, -2),	/* All */
	SNOR_WP_BP_UP(SR_TB |          SR_BP1         , -2),	/* All */
	SNOR_WP_BP_UP(SR_TB |          SR_BP1 | SR_BP0, -2),	/* All */
	SNOR_WP_BP_UP(SR_TB | SR_BP2                  , -2),	/* All */
	SNOR_WP_BP_UP(SR_TB | SR_BP2 |          SR_BP0, -2),	/* All */
	SNOR_WP_BP_UP(SR_TB | SR_BP2 | SR_BP1         , -2),	/* All */
	SNOR_WP_BP_UP(SR_TB | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_SP_UP(                          SR_BP0, 3),	/* Upper 32KB */
	SNOR_WP_BP_UP(                 SR_BP1         , 0),	/* Upper 64KB */
);

static const struct spi_nor_wp_info is25xp020ej_wpr_3bp_tb = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_BP_UP(0                                   , -1),	/* None */

	SNOR_WP_BP_UP(            SR_BP2                  , -2),	/* All */
	SNOR_WP_BP_UP(            SR_BP2 |          SR_BP0, -2),	/* All */
	SNOR_WP_BP_UP(            SR_BP2 | SR_BP1         , -2),	/* All */
	SNOR_WP_BP_UP(            SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */
	SNOR_WP_BP_UP(    SR_TB                           , -2),	/* All */
	SNOR_WP_BP_UP(    SR_TB | SR_BP2                  , -2),	/* All */
	SNOR_WP_BP_UP(    SR_TB | SR_BP2 |          SR_BP0, -2),	/* All */
	SNOR_WP_BP_UP(    SR_TB | SR_BP2 | SR_BP1         , -2),	/* All */
	SNOR_WP_BP_UP(    SR_TB | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_UP(                              SR_BP0, 0),		/* Upper 64KB */
	SNOR_WP_BP_UP(                     SR_BP1         , 1),		/* Upper 128KB */
	SNOR_WP_BP_CMP_UP(                 SR_BP1 | SR_BP0, 0),		/* Upper T - 64KB */

	SNOR_WP_BP_LO(    SR_TB |                   SR_BP0, 0),		/* Lower 64KB */
	SNOR_WP_BP_LO(    SR_TB |          SR_BP1         , 1),		/* Lower 128KB */
	SNOR_WP_BP_CMP_LO(SR_TB |          SR_BP1 | SR_BP0, 0),		/* Lower T - 64KB */
);

static const struct spi_nor_wp_info is25xp040ej_wpr_3bp_tb = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_BP_UP(0                                   , -1),	/* None */

	SNOR_WP_BP_UP(            SR_BP2 | SR_BP1         , -2),	/* All */
	SNOR_WP_BP_UP(            SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */
	SNOR_WP_BP_UP(    SR_TB                           , -2),	/* All */
	SNOR_WP_BP_UP(    SR_TB | SR_BP2 | SR_BP1         , -2),	/* All */
	SNOR_WP_BP_UP(    SR_TB | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_UP(                              SR_BP0, 0),		/* Upper 64KB */
	SNOR_WP_BP_UP(                     SR_BP1         , 1),		/* Upper 128KB */
	SNOR_WP_BP_UP(                     SR_BP1 | SR_BP0, 2),		/* Upper 256KB */
	SNOR_WP_BP_CMP_UP(        SR_BP2                  , 1),		/* Upper T - 128KB */
	SNOR_WP_BP_CMP_UP(        SR_BP2 |          SR_BP0, 0),		/* Upper T - 64KB */

	SNOR_WP_BP_LO(    SR_TB |                   SR_BP0, 0),		/* Lower 64KB */
	SNOR_WP_BP_LO(    SR_TB |          SR_BP1         , 1),		/* Lower 128KB */
	SNOR_WP_BP_LO(    SR_TB |          SR_BP1 | SR_BP0, 2),		/* Lower 256KB */
	SNOR_WP_BP_CMP_LO(SR_TB | SR_BP2                  , 1),		/* Lower T - 128KB */
	SNOR_WP_BP_CMP_LO(SR_TB | SR_BP2 |          SR_BP0, 0),		/* Lower T - 64KB */
);

static const struct spi_nor_wp_info is25xp064db_wpr_4bp_tbs0 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_UP(0                                    , -1),	/* None */

	SNOR_WP_BP_UP(    SR_BP3 | SR_BP2 | SR_BP1         , -2),	/* All */
	SNOR_WP_BP_UP(    SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_UP(                               SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                      SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                      SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(             SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(             SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_CMP_UP(SR_BP3                           , 5),	/* Upper T - 2MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 |                   SR_BP0, 4),	/* Upper T - 1MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 |          SR_BP1         , 3),	/* Upper T - 512KB */
	SNOR_WP_BP_CMP_UP(SR_BP3 |          SR_BP1 | SR_BP0, 2),	/* Upper T - 256KB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2                  , 1),	/* Upper T - 128KB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 0),	/* Upper T - 64KB */
);

static const struct spi_nor_wp_info is25xp064db_wpr_4bp_tbs1 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_LO(0                                    , -1),	/* None */

	SNOR_WP_BP_LO(    SR_BP3 | SR_BP2 | SR_BP1         , -2),	/* All */
	SNOR_WP_BP_LO(    SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_LO(                               SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(                      SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(                      SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(             SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(             SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(             SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
	SNOR_WP_BP_LO(             SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Lower 4MB */
	SNOR_WP_BP_CMP_LO(SR_BP3                           , 5),	/* Lower T - 2MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 |                   SR_BP0, 4),	/* Lower T - 1MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 |          SR_BP1         , 3),	/* Lower T - 512KB */
	SNOR_WP_BP_CMP_LO(SR_BP3 |          SR_BP1 | SR_BP0, 2),	/* Lower T - 256KB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2                  , 1),	/* Lower T - 128KB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 0),	/* Lower T - 64KB */
);

static const struct spi_nor_wp_info is25xp128d_wpr_4bp_tbs0 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_UP(0                                    , -1),	/* None */

	SNOR_WP_BP_UP(    SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_UP(                               SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                      SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                      SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(             SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(             SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_UP(    SR_BP3                           , 7),	/* Upper 8MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 |                   SR_BP0, 6),	/* Upper T - 4MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 |          SR_BP1         , 5),	/* Upper T - 2MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 |          SR_BP1 | SR_BP0, 4),	/* Upper T - 1MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2                  , 3),	/* Upper T - 512KB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 2),	/* Upper T - 256KB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 1),	/* Upper T - 128KB */
);

static const struct spi_nor_wp_info is25xp128d_wpr_4bp_tbs1 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_LO(0                                    , -1),	/* None */

	SNOR_WP_BP_LO(    SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_LO(                               SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(                      SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(                      SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(             SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(             SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(             SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
	SNOR_WP_BP_LO(             SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Lower 4MB */
	SNOR_WP_BP_LO(    SR_BP3                           , 7),	/* Lower 8MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 |                   SR_BP0, 6),	/* Lower T - 4MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 |          SR_BP1         , 5),	/* Lower T - 2MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 |          SR_BP1 | SR_BP0, 4),	/* Lower T - 1MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2                  , 3),	/* Lower T - 512KB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 2),	/* Lower T - 256KB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 1),	/* Lower T - 128KB */
);

static const struct spi_nor_wp_info is25xp256ej_wpr_4bp_tbs0 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_UP(0                                    , -1),	/* None */

	SNOR_WP_BP_UP(    SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_UP(                               SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                      SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                      SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(             SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(             SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_UP(    SR_BP3                           , 7),	/* Upper 8MB */
	SNOR_WP_BP_UP(    SR_BP3 |                   SR_BP0, 8),	/* Upper 16MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 |          SR_BP1         , 7),	/* Upper T - 8MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 |          SR_BP1 | SR_BP0, 6),	/* Upper T - 4MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2                  , 5),	/* Upper T - 2MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 4),	/* Upper T - 1MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 3),	/* Upper T - 512KB */
);

static const struct spi_nor_wp_info is25xp256ej_wpr_4bp_tbs1 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_LO(0                                    , -1),	/* None */

	SNOR_WP_BP_LO(    SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_LO(                               SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(                      SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(                      SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(             SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(             SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(             SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
	SNOR_WP_BP_LO(             SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Lower 4MB */
	SNOR_WP_BP_LO(    SR_BP3                           , 7),	/* Lower 8MB */
	SNOR_WP_BP_LO(    SR_BP3 |                   SR_BP0, 8),	/* Lower 16MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 |          SR_BP1         , 7),	/* Lower T - 8MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 |          SR_BP1 | SR_BP0, 6),	/* Lower T - 4MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2                  , 5),	/* Lower T - 2MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 4),	/* Lower T - 1MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 3),	/* Lower T - 512KB */
);

static const struct spi_nor_wp_info is25xp256ek_wpr_4bp_tbs0 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_UP(0                                , -1),		/* None */

	SNOR_WP_BP_UP(                           SR_BP0, 0 * 4),	/* Upper 256KB */
	SNOR_WP_BP_UP(                  SR_BP1         , 1 * 4),	/* Upper 512KB */
	SNOR_WP_BP_UP(                  SR_BP1 | SR_BP0, 2 * 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(         SR_BP2                  , 3 * 4),	/* Upper 2MB */
	SNOR_WP_BP_UP(         SR_BP2 |          SR_BP0, 4 * 4),	/* Upper 4MB */
	SNOR_WP_BP_UP(         SR_BP2 | SR_BP1         , 5 * 4),	/* Upper 8MB */
	SNOR_WP_BP_UP(         SR_BP2 | SR_BP1 | SR_BP0, 6 * 4),	/* Upper 16MB */
	SNOR_WP_BP_UP(SR_BP3                           , 7 * 4),	/* Upper 32MB */
	SNOR_WP_BP_UP(SR_BP3 |                   SR_BP0, 8 * 4),	/* Upper 64MB */
	SNOR_WP_BP_UP(SR_BP3 |          SR_BP1         , 9 * 4),	/* Upper 128MB */
	SNOR_WP_BP_UP(SR_BP3 |          SR_BP1 | SR_BP0, 10 * 4),	/* Upper 256MB */
	SNOR_WP_BP_UP(SR_BP3 | SR_BP2                  , 11 * 4),	/* Upper 512MB */
	SNOR_WP_BP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 12 * 4),	/* Upper 1GB */
	SNOR_WP_BP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 13 * 4),	/* Upper 2GB */
	SNOR_WP_BP_UP(SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, 14 * 4),	/* Upper 4GB */
);

static const struct spi_nor_wp_info is25xp256ek_wpr_4bp_tbs1 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_UP(0                                , -1),		/* None */

	SNOR_WP_BP_LO(                           SR_BP0, 0 * 4),	/* Lower 256KB */
	SNOR_WP_BP_LO(                  SR_BP1         , 1 * 4),	/* Lower 512KB */
	SNOR_WP_BP_LO(                  SR_BP1 | SR_BP0, 2 * 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(         SR_BP2                  , 3 * 4),	/* Lower 2MB */
	SNOR_WP_BP_LO(         SR_BP2 |          SR_BP0, 4 * 4),	/* Lower 4MB */
	SNOR_WP_BP_LO(         SR_BP2 | SR_BP1         , 5 * 4),	/* Lower 8MB */
	SNOR_WP_BP_LO(         SR_BP2 | SR_BP1 | SR_BP0, 6 * 4),	/* Lower 16MB */
	SNOR_WP_BP_LO(SR_BP3                           , 7 * 4),	/* Lower 32MB */
	SNOR_WP_BP_LO(SR_BP3 |                   SR_BP0, 8 * 4),	/* Lower 64MB */
	SNOR_WP_BP_LO(SR_BP3 |          SR_BP1         , 9 * 4),	/* Lower 128MB */
	SNOR_WP_BP_LO(SR_BP3 |          SR_BP1 | SR_BP0, 10 * 4),	/* Lower 256MB */
	SNOR_WP_BP_LO(SR_BP3 | SR_BP2                  , 11 * 4),	/* Lower 512MB */
	SNOR_WP_BP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 12 * 4),	/* Lower 1GB */
	SNOR_WP_BP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 13 * 4),	/* Lower 2GB */
	SNOR_WP_BP_LO(SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, 14 * 4),	/* Lower 4GB */
);

static const struct spi_nor_wp_info is25xp512mj_wpr_4bp_tbs0 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_UP(0                                    , -1),	/* None */

	SNOR_WP_BP_UP(    SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_UP(                               SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                      SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                      SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(             SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(             SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_UP(    SR_BP3                           , 7),	/* Upper 8MB */
	SNOR_WP_BP_UP(    SR_BP3 |                   SR_BP0, 8),	/* Upper 16MB */
	SNOR_WP_BP_UP(    SR_BP3 |          SR_BP1         , 9),	/* Upper 32MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 |          SR_BP1 | SR_BP0, 8),	/* Upper T - 16MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2                  , 7),	/* Upper T - 8MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 6),	/* Upper T - 4MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 5),	/* Upper T - 2MB */
);

static const struct spi_nor_wp_info is25xp512mj_wpr_4bp_tbs1 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_LO(0                                    , -1),	/* None */

	SNOR_WP_BP_LO(    SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_LO(                               SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(                      SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(                      SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(             SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(             SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(             SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
	SNOR_WP_BP_LO(             SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Lower 4MB */
	SNOR_WP_BP_LO(    SR_BP3                           , 7),	/* Lower 8MB */
	SNOR_WP_BP_LO(    SR_BP3 |                   SR_BP0, 8),	/* Lower 16MB */
	SNOR_WP_BP_LO(    SR_BP3 |          SR_BP1         , 9),	/* Lower 32MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 |          SR_BP1 | SR_BP0, 8),	/* Lower T - 16MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2                  , 7),	/* Lower T - 8MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 6),	/* Lower T - 4MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 5),	/* Lower T - 2MB */
);

static const struct spi_nor_wp_info is25xp01gj_wpr_4bp_tbs0 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_UP(0                                    , -1),	/* None */

	SNOR_WP_BP_UP(    SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_UP(                               SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                      SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                      SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(             SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(             SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_UP(    SR_BP3                           , 7),	/* Upper 8MB */
	SNOR_WP_BP_UP(    SR_BP3 |                   SR_BP0, 8),	/* Upper 16MB */
	SNOR_WP_BP_UP(    SR_BP3 |          SR_BP1         , 9),	/* Upper 32MB */
	SNOR_WP_BP_UP(    SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Upper 64MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2                  , 9),	/* Upper T - 32MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 8),	/* Upper T - 16MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 7),	/* Upper T - 8MB */
);

static const struct spi_nor_wp_info is25xp01gj_wpr_4bp_tbs1 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_LO(0                                    , -1),	/* None */

	SNOR_WP_BP_LO(    SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_LO(                               SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(                      SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(                      SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(             SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(             SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(             SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
	SNOR_WP_BP_LO(             SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Lower 4MB */
	SNOR_WP_BP_LO(    SR_BP3                           , 7),	/* Lower 8MB */
	SNOR_WP_BP_LO(    SR_BP3 |                   SR_BP0, 8),	/* Lower 16MB */
	SNOR_WP_BP_LO(    SR_BP3 |          SR_BP1         , 9),	/* Lower 32MB */
	SNOR_WP_BP_LO(    SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Lower 64MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2                  , 9),	/* Lower T - 32MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 8),	/* Lower T - 16MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 7),	/* Lower T - 8MB */
);

static const struct spi_nor_wp_info is25xp02ggj_wpr_4bp_tbs0 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_UP(0                                    , -1),	/* None */

	SNOR_WP_BP_UP(    SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_UP(                               SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                      SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                      SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(             SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(             SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_UP(    SR_BP3                           , 7),	/* Upper 8MB */
	SNOR_WP_BP_UP(    SR_BP3 |                   SR_BP0, 8),	/* Upper 16MB */
	SNOR_WP_BP_UP(    SR_BP3 |          SR_BP1         , 9),	/* Upper 32MB */
	SNOR_WP_BP_UP(    SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Upper 64MB */
	SNOR_WP_BP_UP(    SR_BP3 | SR_BP2                  , 11),	/* Upper 128MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 10),	/* Upper T - 64MB */
	SNOR_WP_BP_CMP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 9),	/* Upper T - 32MB */
);

static const struct spi_nor_wp_info is25xp02ggj_wpr_4bp_tbs1 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_BP_LO(0                                    , -1),	/* None */

	SNOR_WP_BP_LO(    SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_LO(                               SR_BP0, 1),	/* Lower 64KB */
	SNOR_WP_BP_LO(                      SR_BP1         , 2),	/* Lower 128KB */
	SNOR_WP_BP_LO(                      SR_BP1 | SR_BP0, 4),	/* Lower 256KB */
	SNOR_WP_BP_LO(             SR_BP2                  , 8),	/* Lower 512KB */
	SNOR_WP_BP_LO(             SR_BP2 |          SR_BP0, 16),	/* Lower 1MB */
	SNOR_WP_BP_LO(             SR_BP2 | SR_BP1         , 32),	/* Lower 2MB */
	SNOR_WP_BP_LO(             SR_BP2 | SR_BP1 | SR_BP0, 64),	/* Lower 4MB */
	SNOR_WP_BP_LO(    SR_BP3                           , 128),	/* Lower 8MB */
	SNOR_WP_BP_LO(    SR_BP3 |                   SR_BP0, 256),	/* Lower 16MB */
	SNOR_WP_BP_LO(    SR_BP3 |          SR_BP1         , 512),	/* Lower 32MB */
	SNOR_WP_BP_LO(    SR_BP3 |          SR_BP1 | SR_BP0, 1024),	/* Lower 64MB */
	SNOR_WP_BP_LO(    SR_BP3 | SR_BP2                  , 2048),	/* Lower 128MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 3072),	/* Lower T - 64MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 3584),	/* Lower T - 32MB */
);

static DEFINE_SNOR_ALIAS(is25cd512_alias, SNOR_ALIAS_MODEL("IS25LD512A"));
static DEFINE_SNOR_ALIAS(is25cd010_alias, SNOR_ALIAS_MODEL("IS25LD010A"));
static DEFINE_SNOR_ALIAS(is25lp032_alias, SNOR_ALIAS_MODEL("IS25LP032A"), SNOR_ALIAS_MODEL("IS25LP032B"));
static DEFINE_SNOR_ALIAS(is25wp032_alias, SNOR_ALIAS_MODEL("IS25WP032A"));
static DEFINE_SNOR_ALIAS(is25lp064_alias, SNOR_ALIAS_MODEL("IS25LP064A"), SNOR_ALIAS_MODEL("IS25LP064B"));
static DEFINE_SNOR_ALIAS(is25wp064_alias, SNOR_ALIAS_MODEL("IS25WP064A"));
static DEFINE_SNOR_ALIAS(is25lp128a_alias, SNOR_ALIAS_MODEL("IS25LP128B"));
static DEFINE_SNOR_ALIAS(is25lp128fj_alias, SNOR_ALIAS_MODEL("IS25RLP128FJ"));
static DEFINE_SNOR_ALIAS(is25lp128fb_alias, SNOR_ALIAS_MODEL("IS25RLP128FB"));
static DEFINE_SNOR_ALIAS(is25wp128_alias, SNOR_ALIAS_MODEL("IS25WP128A"));
static DEFINE_SNOR_ALIAS(is25wp128fj_alias, SNOR_ALIAS_MODEL("IS25RWP128FJ"));
static DEFINE_SNOR_ALIAS(is25wp128fb_alias, SNOR_ALIAS_MODEL("IS25RWP128FB"));
static DEFINE_SNOR_ALIAS(is25lp256ej_alias, SNOR_ALIAS_MODEL("IS25RLP256EJ"));
static DEFINE_SNOR_ALIAS(is25lp256ek_alias, SNOR_ALIAS_MODEL("IS25RLP256EK"));
static DEFINE_SNOR_ALIAS(is25wp256_alias, SNOR_ALIAS_MODEL("IS25WP256A"));
static DEFINE_SNOR_ALIAS(is25wp256ej_alias, SNOR_ALIAS_MODEL("IS25RWP256EJ"));
static DEFINE_SNOR_ALIAS(is25wp256ek_alias, SNOR_ALIAS_MODEL("IS25RWP256EK"));

static ufprog_status is25lx025_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	bp->p.model = bp->model;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A) {
		snprintf(bp->model, sizeof(bp->model), "IS25LQ025B");
	} else if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B) {
		snprintf(bp->model, sizeof(bp->model), "IS25LP025E");
		bp->p.vendor_flags &= ~ISSI_F_OTP_NO_ERASE;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25lx025_fixups = {
	.pre_param_setup = is25lx025_fixup_model,
};

static ufprog_status is25lx512_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	bp->p.model = bp->model;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A) {
		snprintf(bp->model, sizeof(bp->model), "IS25LQ512B");
	} else if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B) {
		snprintf(bp->model, sizeof(bp->model), "IS25LP512E");
		bp->p.vendor_flags &= ~ISSI_F_OTP_NO_ERASE;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25lx512_fixups = {
	.pre_param_setup = is25lx512_fixup_model,
};

static ufprog_status pm25lq512b_erase_op_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					       struct spi_nor_flash_part_blank *bp)
{
	uint32_t i;

	/* 64KB erase type advertised in SFDP but actually not supported */
	for (i = 0; i < SPI_NOR_MAX_ERASE_INFO; i++) {
		if (bp->erase_info_3b.info[i].size == SZ_64K) {
			/* Use chip erase opcode for 64KB erase */
			bp->erase_info_3b.info[i].opcode = SNOR_CMD_CHIP_ERASE;
			bp->erase_info_3b.info[i].max_erase_time_ms = SNOR_ERASE_TIMEOUT_MS;
			break;
		}
	}

	return UFP_OK;
}

static ufprog_status is25cd512_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "PM25LQ512B"));

	return pm25lq512b_erase_op_fixup(snor, vp, bp);
}

static const struct spi_nor_flash_part_fixup is25cd512_fixups = {
	.pre_param_setup = is25cd512_fixup_model,
};

static const struct spi_nor_flash_part_fixup pm25lq512b_fixups = {
	.pre_param_setup = pm25lq512b_erase_op_fixup,
};

static ufprog_status is25lx010_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	bp->p.model = bp->model;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A) {
		snprintf(bp->model, sizeof(bp->model), "IS25LQ010B");
	} else if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B) {
		snprintf(bp->model, sizeof(bp->model), "IS25LP010E");
		bp->p.vendor_flags &= ~ISSI_F_OTP_NO_ERASE;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25lx010_fixups = {
	.pre_param_setup = is25lx010_fixup_model,
};

static ufprog_status is25cd010_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "PM25LQ010B");
}

static const struct spi_nor_flash_part_fixup is25cd010_fixups = {
	.pre_param_setup = is25cd010_fixup_model,
};

static ufprog_status is25lx020_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	bp->p.model = bp->model;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A) {
		snprintf(bp->model, sizeof(bp->model), "IS25LQ020B");
	} else if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B) {
		snprintf(bp->model, sizeof(bp->model), "IS25LP020E");
		bp->p.vendor_flags &= ~ISSI_F_OTP_NO_ERASE;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25lx020_fixups = {
	.pre_param_setup = is25lx020_fixup_model,
};

static ufprog_status is25lx040_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "PM25LQ040B");
}

static const struct spi_nor_flash_part_fixup is25lx040_fixups = {
	.pre_param_setup = is25lx040_fixup_model,
};

static ufprog_status is25wp080x_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	bp->p.model = bp->model;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A) {
		snprintf(bp->model, sizeof(bp->model), "IS25WP080");
		bp->p.regs = &is25xpxd_16pins_regs;
	} else if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B) {
		snprintf(bp->model, sizeof(bp->model), "IS25WP080D");
		bp->p.regs = &is25xpxd_regs;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25wp080x_fixups = {
	.pre_param_setup = is25wp080x_fixup_model,
};

static ufprog_status is25xp256ek_wpr_4bp_tbs_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						    struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &issi_fr_acc, &regval));

	if (regval & FR_TBS)
		bp->p.wp_ranges = &is25xp256ek_wpr_4bp_tbs1;
	else
		bp->p.wp_ranges = &is25xp256ek_wpr_4bp_tbs0;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25xp256ek_wpr_4bp_tbs_fixups = {
	.pre_param_setup = is25xp256ek_wpr_4bp_tbs_select,
};

static ufprog_status is25lp128x_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B && bp->p.page_size == 512) {
		bp->p.model = bp->model;
		snprintf(bp->model, sizeof(bp->model), "IS25LE128EK");
		bp->p.vendor_flags |= ISSI_F_ECC;
		bp->p.max_speed_spi_mhz = 166;
		bp->p.max_speed_quad_mhz = 151;
		bp->p.regs = &is25xe128e_regs;
		is25xp256ek_wpr_4bp_tbs_select(snor, vp, bp);
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25lp128x_fixups = {
	.pre_param_setup = is25lp128x_fixup_model,
};

static ufprog_status is25wp128x_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B && bp->p.page_size == 512) {
		bp->p.model = bp->model;
		snprintf(bp->model, sizeof(bp->model), "IS25WE128EK");
		bp->p.vendor_flags |= ISSI_F_ECC;
		bp->p.max_speed_spi_mhz = 166;
		bp->p.max_speed_quad_mhz = 151;
		bp->p.regs = &is25xe128e_regs;
		is25xp256ek_wpr_4bp_tbs_select(snor, vp, bp);
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25wp128x_fixups = {
	.pre_param_setup = is25wp128x_fixup_model,
};

static ufprog_status is25lx256_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B && bp->p.page_size == 512) {
		bp->p.model = bp->model;
		snprintf(bp->model, sizeof(bp->model), "IS25L*256EK");
		bp->p.max_speed_spi_mhz = 166;
		bp->p.max_speed_quad_mhz = 151;
		bp->p.vendor_flags |= ISSI_F_RR_DC_BIT6_3;
		is25xp256ek_wpr_4bp_tbs_select(snor, vp, bp);
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25lx256_fixups = {
	.pre_param_setup = is25lx256_fixup_model,
};

static ufprog_status is25wx256_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B && bp->p.page_size == 512) {
		bp->p.model = bp->model;
		snprintf(bp->model, sizeof(bp->model), "IS25W*256EK");
		bp->p.max_speed_spi_mhz = 166;
		bp->p.max_speed_quad_mhz = 151;
		is25xp256ek_wpr_4bp_tbs_select(snor, vp, bp);
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25wx256_fixups = {
	.pre_param_setup = is25wx256_fixup_model,
};

static ufprog_status is25lx512m_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B && bp->p.page_size == 512) {
		bp->p.model = bp->model;
		snprintf(bp->model, sizeof(bp->model), "IS25L*512MK");
		is25xp256ek_wpr_4bp_tbs_select(snor, vp, bp);
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25lx512m_fixups = {
	.pre_param_setup = is25lx512m_fixup_model,
};

static ufprog_status is25wx512m_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B && bp->p.page_size == 512) {
		bp->p.model = bp->model;
		snprintf(bp->model, sizeof(bp->model), "IS25W*512MK");
		is25xp256ek_wpr_4bp_tbs_select(snor, vp, bp);
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25wx512m_fixups = {
	.pre_param_setup = is25wx512m_fixup_model,
};

static ufprog_status is25lx01g_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B && bp->p.page_size == 512) {
		bp->p.model = bp->model;
		snprintf(bp->model, sizeof(bp->model), "IS25L*01GK");
		is25xp256ek_wpr_4bp_tbs_select(snor, vp, bp);
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25lx01g_fixups = {
	.pre_param_setup = is25lx01g_fixup_model,
};

static ufprog_status is25wx01g_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B && bp->p.page_size == 512) {
		bp->p.model = bp->model;
		snprintf(bp->model, sizeof(bp->model), "IS25W*01GK");
		is25xp256ek_wpr_4bp_tbs_select(snor, vp, bp);
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25wx01g_fixups = {
	.pre_param_setup = is25wx01g_fixup_model,
};

static ufprog_status is25lx02gg_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B && bp->p.page_size == 512) {
		bp->p.model = bp->model;
		snprintf(bp->model, sizeof(bp->model), "IS25L*02GGK");
		is25xp256ek_wpr_4bp_tbs_select(snor, vp, bp);
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25lx02gg_fixups = {
	.pre_param_setup = is25lx02gg_fixup_model,
};

static ufprog_status is25wx02gg_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B && bp->p.page_size == 512) {
		bp->p.model = bp->model;
		snprintf(bp->model, sizeof(bp->model), "IS25W*02GGK");
		is25xp256ek_wpr_4bp_tbs_select(snor, vp, bp);
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25wx02gg_fixups = {
	.pre_param_setup = is25wx02gg_fixup_model,
};

static ufprog_status is25xpxab_wpr_4bp_tbs_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						  struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &issi_fr_acc, &regval));

	if (regval & FR_TBS)
		bp->p.wp_ranges = &wpr_4bp_lo;
	else
		bp->p.wp_ranges = &wpr_4bp_up;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25xpxab_wpr_4bp_tbs_fixups = {
	.pre_param_setup = is25xpxab_wpr_4bp_tbs_select,
};

static ufprog_status is25xp064db_wpr_4bp_tbs_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						    struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &issi_fr_acc, &regval));

	if (regval & FR_TBS)
		bp->p.wp_ranges = &is25xp064db_wpr_4bp_tbs1;
	else
		bp->p.wp_ranges = &is25xp064db_wpr_4bp_tbs0;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25xp064db_wpr_4bp_tbs_fixups = {
	.pre_param_setup = is25xp064db_wpr_4bp_tbs_select,
};

static ufprog_status is25xp128d_wpr_4bp_tbs_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						   struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &issi_fr_acc, &regval));

	if (regval & FR_TBS)
		bp->p.wp_ranges = &is25xp128d_wpr_4bp_tbs1;
	else
		bp->p.wp_ranges = &is25xp128d_wpr_4bp_tbs0;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25xp128d_wpr_4bp_tbs_fixups = {
	.pre_param_setup = is25xp128d_wpr_4bp_tbs_select,
};

static ufprog_status is25xp256ej_wpr_4bp_tbs_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						    struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &issi_fr_acc, &regval));

	if (regval & FR_TBS)
		bp->p.wp_ranges = &is25xp256ej_wpr_4bp_tbs1;
	else
		bp->p.wp_ranges = &is25xp256ej_wpr_4bp_tbs0;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25xp256ej_wpr_4bp_tbs_fixups = {
	.pre_param_setup = is25xp256ej_wpr_4bp_tbs_select,
};

static ufprog_status is25xp512mj_wpr_4bp_tbs_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						    struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &issi_fr_acc, &regval));

	if (regval & FR_TBS)
		bp->p.wp_ranges = &is25xp512mj_wpr_4bp_tbs1;
	else
		bp->p.wp_ranges = &is25xp512mj_wpr_4bp_tbs0;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25xp512mj_wpr_4bp_tbs_fixups = {
	.pre_param_setup = is25xp512mj_wpr_4bp_tbs_select,
};

static ufprog_status is25xp01gj_wpr_4bp_tbs_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						   struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &issi_fr_acc, &regval));

	if (regval & FR_TBS)
		bp->p.wp_ranges = &is25xp01gj_wpr_4bp_tbs1;
	else
		bp->p.wp_ranges = &is25xp01gj_wpr_4bp_tbs0;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25xp01gj_wpr_4bp_tbs_fixups = {
	.pre_param_setup = is25xp01gj_wpr_4bp_tbs_select,
};

static ufprog_status is25xp02ggj_wpr_4bp_tbs_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						    struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &issi_fr_acc, &regval));

	if (regval & FR_TBS)
		bp->p.wp_ranges = &is25xp02ggj_wpr_4bp_tbs1;
	else
		bp->p.wp_ranges = &is25xp02ggj_wpr_4bp_tbs0;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup is25xp02ggj_wpr_4bp_tbs_fixups = {
	.pre_param_setup = is25xp02ggj_wpr_4bp_tbs_select,
};

static const struct spi_nor_erase_info is25cd512_010_erase_opcodes = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_4K, SNOR_CMD_SECTOR_ERASE),
	SNOR_ERASE_SECTOR(SZ_32K, SNOR_CMD_BLOCK_ERASE),
);

static const struct spi_nor_flash_part issi_parts[] = {
	SNOR_PART("IS25L*025", SNOR_ID(0x9d, 0x40, 0x09), SZ_32K,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_FIXUPS(&is25lx025_fixups),
	),

	SNOR_PART("IS25LQ025B", SNOR_ID(0x9d, 0x40, 0x09), SZ_32K, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LP025EJ", SNOR_ID(0x9d, 0x40, 0x09), SZ_32K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&is25xp010ec_wpr_3bp_tb),
	),

	SNOR_PART("IS25LP025EB", SNOR_ID(0x9d, 0x40, 0x09), SZ_32K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP025E", SNOR_ID(0x9d, 0x70, 0x09), SZ_32K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
	),

	SNOR_PART("IS25WP025EJ", SNOR_ID(0x9d, 0x70, 0x09), SZ_32K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&is25xp010ec_wpr_3bp_tb),
	),

	SNOR_PART("IS25WP025EB", SNOR_ID(0x9d, 0x70, 0x09), SZ_32K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25L*512", SNOR_ID(0x9d, 0x40, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_FIXUPS(&is25lx512_fixups),
	),

	SNOR_PART("IS25LQ512B", SNOR_ID(0x9d, 0x40, 0x10), SZ_64K, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LP512EJ", SNOR_ID(0x9d, 0x40, 0x10), SZ_64K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LP512EB", SNOR_ID(0x9d, 0x40, 0x10), SZ_64K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP512E", SNOR_ID(0x9d, 0x70, 0x10), SZ_64K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
	),

	SNOR_PART("IS25WP512EJ", SNOR_ID(0x9d, 0x70, 0x10), SZ_64K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP512EB", SNOR_ID(0x9d, 0x70, 0x10), SZ_64K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25CD512", SNOR_ID(0x7f, 0x9d, 0x20), SZ_64K,
		  SNOR_ALIAS(&is25cd512_alias), /* IS25LD512A */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_ERASE_INFO(&is25cd512_010_erase_opcodes),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&is25cd_regs),
		  SNOR_WP_RANGES(&is25cd512_wpr_2bp),
		  SNOR_FIXUPS(&is25cd512_fixups),
	),

	SNOR_PART("IS25L*010", SNOR_ID(0x9d, 0x40, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_FIXUPS(&is25lx010_fixups),
	),

	SNOR_PART("IS25LQ010B", SNOR_ID(0x9d, 0x40, 0x11), SZ_128K, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LP010EJ", SNOR_ID(0x9d, 0x40, 0x11), SZ_128K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LP010EC", SNOR_ID(0x9d, 0x40, 0x11), SZ_128K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&is25xp010ec_wpr_3bp_tb),
	),

	SNOR_PART("IS25LP010EB", SNOR_ID(0x9d, 0x40, 0x11), SZ_128K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP010E", SNOR_ID(0x9d, 0x70, 0x11), SZ_128K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
	),

	SNOR_PART("IS25WP010EJ", SNOR_ID(0x9d, 0x70, 0x11), SZ_128K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP010EC", SNOR_ID(0x9d, 0x70, 0x11), SZ_128K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&is25xp010ec_wpr_3bp_tb),
	),

	SNOR_PART("IS25WP010EB", SNOR_ID(0x9d, 0x70, 0x11), SZ_128K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25CD010", SNOR_ID(0x7f, 0x9d, 0x21), SZ_128K,
		  SNOR_ALIAS(&is25cd010_alias), /* IS25LD010A */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_ERASE_INFO(&is25cd512_010_erase_opcodes),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&is25cd_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up_ratio),
		  SNOR_FIXUPS(&is25cd010_fixups),
	),

	SNOR_PART("IS25L*020", SNOR_ID(0x9d, 0x40, 0x12), SZ_256K, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_FIXUPS(&is25lx020_fixups),
	),

	SNOR_PART("IS25LQ020B", SNOR_ID(0x9d, 0x40, 0x12), SZ_256K, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LP020EJ", SNOR_ID(0x9d, 0x40, 0x12), SZ_256K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&is25xp020ej_wpr_3bp_tb),
	),

	SNOR_PART("IS25LP020EB", SNOR_ID(0x9d, 0x40, 0x12), SZ_256K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("PM/IS25LQ020", SNOR_ID(0x9d, 0x11, 0x42), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LQ020C", SNOR_ID(0x9d, 0x11, 0x42), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP020*", SNOR_ID(0x9d, 0x70, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
	),

	SNOR_PART("IS25WP020", SNOR_ID(0x9d, 0x70, 0x12), SZ_256K, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(128),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_16pins_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP020D", SNOR_ID(0x9d, 0x70, 0x12), SZ_256K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP020EJ", SNOR_ID(0x9d, 0x70, 0x12), SZ_256K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&is25xp020ej_wpr_3bp_tb),
	),

	SNOR_PART("IS25WP020EB", SNOR_ID(0x9d, 0x70, 0x12), SZ_256K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WQ020", SNOR_ID(0x9d, 0x11, 0x52), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_CB_MODE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_1),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LD020", SNOR_ID(0x7f, 0x9d, 0x22), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&is25cd_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up_ratio),
	),

	SNOR_PART("IS25L*040", SNOR_ID(0x9d, 0x40, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
	),

	SNOR_PART("IS25LQ040B", SNOR_ID(0x9d, 0x40, 0x13), SZ_512K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LP040EJ", SNOR_ID(0x9d, 0x40, 0x13), SZ_512K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&is25xp040ej_wpr_3bp_tb),
	),

	SNOR_PART("IS25LP040EB", SNOR_ID(0x9d, 0x40, 0x13), SZ_512K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LQ040C", SNOR_ID(0x9d, 0x12, 0x43), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP040*", SNOR_ID(0x9d, 0x70, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
	),

	SNOR_PART("IS25WP040", SNOR_ID(0x9d, 0x70, 0x13), SZ_512K, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(128),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_16pins_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP040D", SNOR_ID(0x9d, 0x70, 0x13), SZ_512K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP040E", SNOR_ID(0x9d, 0x70, 0x13), SZ_512K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
	),

	SNOR_PART("IS25WP040EJ", SNOR_ID(0x9d, 0x70, 0x13), SZ_512K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&is25xp040ej_wpr_3bp_tb),
	),

	SNOR_PART("IS25WP040EB", SNOR_ID(0x9d, 0x70, 0x13), SZ_512K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WQ040", SNOR_ID(0x9d, 0x12, 0x53), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_CB_MODE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_1),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("PM/IS25L*040", SNOR_ID(0x7f, 0x9d, 0x7e), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&is25cd_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
		  SNOR_FIXUPS(&is25lx040_fixups),
	),

	SNOR_PART("IS25LD040", SNOR_ID(0x7f, 0x9d, 0x7e), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(100),
		  SNOR_REGS(&is25cd_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("IS25LQ080B", SNOR_ID(0x9d, 0x40, 0x14), SZ_1M, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LP080D", SNOR_ID(0x9d, 0x60, 0x14), SZ_1M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP080*", SNOR_ID(0x9d, 0x70, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(128),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_FIXUPS(&is25wp080x_fixups),
	),

	SNOR_PART("IS25WP080", SNOR_ID(0x9d, 0x70, 0x14), SZ_1M, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(128),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_16pins_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP080D", SNOR_ID(0x9d, 0x70, 0x14), SZ_1M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WQ080", SNOR_ID(0x7f, 0x9d, 0x54), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_CB_MODE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_1),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LQ080C", SNOR_ID(0x7f, 0x9d, 0x44), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LQ016B", SNOR_ID(0x9d, 0x40, 0x15), SZ_2M, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LP016D", SNOR_ID(0x9d, 0x60, 0x15), SZ_2M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_16pins_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP016*", SNOR_ID(0x9d, 0x70, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
	),

	SNOR_PART("IS25WP016", SNOR_ID(0x9d, 0x70, 0x15), SZ_2M, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(128),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_16pins_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP016D", SNOR_ID(0x9d, 0x70, 0x15), SZ_2M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_16pins_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WJ016F", SNOR_ID(0x9d, 0x70, 0x15), SZ_2M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_WB_MODE),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_3_1k),
		  SNOR_REGS(&is25wjxf_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("IS25LQ016C", SNOR_ID(0x9d, 0x14, 0x45), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_CB_MODE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_1),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LQ032B", SNOR_ID(0x9d, 0x40, 0x16), SZ_4M, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LP032*", SNOR_ID(0x9d, 0x60, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
	),

	SNOR_PART("IS25LP032", SNOR_ID(0x9d, 0x60, 0x16), SZ_4M, /* SFDP 1.5 */
		  SNOR_ALIAS(&is25lp032_alias), /* IS25LP032A/IS25LP032B */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT4_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxab_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP032D", SNOR_ID(0x9d, 0x60, 0x16), SZ_4M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_16pins_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25W*032", SNOR_ID(0x9d, 0x70, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
	),

	SNOR_PART("IS25WP032", SNOR_ID(0x9d, 0x70, 0x16), SZ_4M, /* SFDP 1.5 */
		  SNOR_ALIAS(&is25wp032_alias), /* IS25WP032A */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(128),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WP032D", SNOR_ID(0x9d, 0x70, 0x16), SZ_4M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_16pins_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WJ032F", SNOR_ID(0x9d, 0x70, 0x16), SZ_4M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_WB_MODE),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_3_1k),
		  SNOR_REGS(&is25wjxf_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("PM/IS25LQ032C", SNOR_ID(0x7f, 0x9d, 0x46), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LQ032C", SNOR_ID(0x7f, 0x9d, 0x46), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25LP064*", SNOR_ID(0x9d, 0x60, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
	),

	SNOR_PART("IS25LP064", SNOR_ID(0x9d, 0x60, 0x17), SZ_8M, /* SFDP 1.5 */
		  SNOR_ALIAS(&is25lp064_alias), /* IS25LP064A/IS25LP064B */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT4_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxab_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP064DJ", SNOR_ID(0x9d, 0x60, 0x17), SZ_8M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_16pins_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP064DB", SNOR_ID(0x9d, 0x60, 0x17), SZ_8M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_16pins_regs),
		  SNOR_FIXUPS(&is25xp064db_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25W*064", SNOR_ID(0x9d, 0x70, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
	),

	SNOR_PART("IS25WP064", SNOR_ID(0x9d, 0x70, 0x17), SZ_8M, /* SFDP 1.5 */
		  SNOR_ALIAS(&is25wp064_alias), /* IS25WP064A */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(128),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("IS25WP064DJ", SNOR_ID(0x9d, 0x70, 0x17), SZ_8M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(145), SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_16pins_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WP064DB", SNOR_ID(0x9d, 0x70, 0x17), SZ_8M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(145), SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxd_16pins_regs),
		  SNOR_FIXUPS(&is25xp064db_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WJ064F", SNOR_ID(0x9d, 0x70, 0x17), SZ_8M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_WB_MODE),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_3_1k),
		  SNOR_REGS(&is25wjxf_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("IS25LP128*", SNOR_ID(0x9d, 0x60, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_FIXUPS(&is25lp128x_fixups),
	),

	SNOR_PART("IS25LP128", SNOR_ID(0x9d, 0x60, 0x18), SZ_16M, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT4_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxab_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP128A", SNOR_ID(0x9d, 0x60, 0x18), SZ_16M, /* SFDP 1.5, ASP */
		  SNOR_ALIAS(&is25lp128a_alias), /* IS25LP128B */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(150), SNOR_QUAD_MAX_SPEED_MHZ(120),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxab_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP128D", SNOR_ID(0x9d, 0x60, 0x18), SZ_16M, /* SFDP 1.5, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(150), SNOR_QUAD_MAX_SPEED_MHZ(120),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_regs),
		  SNOR_FIXUPS(&is25xp128d_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP128FJ", SNOR_ID(0x9d, 0x60, 0x18), SZ_16M, /* SFDP 1.6, ASP */
		  SNOR_ALIAS(&is25lp128fj_alias), /* IS25RLP128FJ */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(150), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP128FB", SNOR_ID(0x9d, 0x60, 0x18), SZ_16M, /* SFDP 1.6, ASP */
		  SNOR_ALIAS(&is25lp128fb_alias), /* IS25RLP128FB */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(150), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_regs),
		  SNOR_FIXUPS(&is25xp128d_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LE128EJ", SNOR_ID(0x9d, 0x60, 0x18), SZ_16M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(156), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xe128e_regs),
		  SNOR_FIXUPS(&is25xp128d_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LE128EK", SNOR_ID(0x9d, 0x60, 0x18), SZ_16M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(156), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xe128e_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WP128*", SNOR_ID(0x9d, 0x70, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_FIXUPS(&is25wp128x_fixups),
	),

	SNOR_PART("IS25WP128", SNOR_ID(0x9d, 0x70, 0x18), SZ_16M, /* SFDP 1.5 */
		  SNOR_ALIAS(&is25wp128_alias), /* IS25WP128A */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(128),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WP128D", SNOR_ID(0x9d, 0x70, 0x18), SZ_16M, /* SFDP 1.5, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(150), SNOR_QUAD_MAX_SPEED_MHZ(120),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_regs),
		  SNOR_FIXUPS(&is25xp128d_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WP128FJ", SNOR_ID(0x9d, 0x70, 0x18), SZ_16M, /* SFDP 1.6, ASP */
		  SNOR_ALIAS(&is25wp128fj_alias), /* IS25RWP128FJ */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(150), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WP128FB", SNOR_ID(0x9d, 0x70, 0x18), SZ_16M, /* SFDP 1.6, ASP */
		  SNOR_ALIAS(&is25wp128fb_alias), /* IS25RWP128FB */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(150), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_regs),
		  SNOR_FIXUPS(&is25xp128d_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WE128EJ", SNOR_ID(0x9d, 0x70, 0x18), SZ_16M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(145), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xe128e_regs),
		  SNOR_FIXUPS(&is25xp128d_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WE128EK", SNOR_ID(0x9d, 0x70, 0x18), SZ_16M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(145), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xe128e_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25L*256", SNOR_ID(0x9d, 0x60, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK | SNOR_F_META),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_FIXUPS(&is25lx256_fixups),
	),

	SNOR_PART("IS25LP256A", SNOR_ID(0x9d, 0x60, 0x19), SZ_32M, /* SFDP 1.5, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(150), SNOR_QUAD_MAX_SPEED_MHZ(120),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_4b_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP256D", SNOR_ID(0x9d, 0x60, 0x19), SZ_32M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(156), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_4b_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP256EJ", SNOR_ID(0x9d, 0x60, 0x19), SZ_32M, /* SFDP 1.6, ASP */
		  SNOR_ALIAS(&is25lp256ej_alias), /* IS25RLP256EJ */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(156), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_4b_regs),
		  SNOR_FIXUPS(&is25xp256ej_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP256EK", SNOR_ID(0x9d, 0x60, 0x19), SZ_32M, /* SFDP 1.6, ASP */
		  SNOR_ALIAS(&is25lp256ek_alias), /* IS25RLP256EK */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(156), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_4b_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LE256EJ", SNOR_ID(0x9d, 0x60, 0x19), SZ_32M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(156), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ej_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LE256EK", SNOR_ID(0x9d, 0x60, 0x19), SZ_32M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(156), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25W*256", SNOR_ID(0x9d, 0x70, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_FIXUPS(&is25wx256_fixups),
	),

	SNOR_PART("IS25WP256", SNOR_ID(0x9d, 0x70, 0x19), SZ_32M, /* SFDP 1.5, ASP */
		  SNOR_ALIAS(&is25wp256_alias), /* IS25wP256A */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(150), SNOR_QUAD_MAX_SPEED_MHZ(120),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_4b_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WP256D", SNOR_ID(0x9d, 0x70, 0x19), SZ_32M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_WP_TBS),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_4b_regs),
		  SNOR_FIXUPS(&is25xpxab_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WP256EJ", SNOR_ID(0x9d, 0x70, 0x19), SZ_32M, /* SFDP 1.6, ASP */
		  SNOR_ALIAS(&is25wp256ej_alias), /* IS25RWP256EJ */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(145), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_4b_regs),
		  SNOR_FIXUPS(&is25xp256ej_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WP256EK", SNOR_ID(0x9d, 0x70, 0x19), SZ_32M, /* SFDP 1.6, ASP */
		  SNOR_ALIAS(&is25wp256ek_alias), /* IS25RWP256EJ */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(145), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xpxa_16pins_4b_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WE256EJ", SNOR_ID(0x9d, 0x70, 0x19), SZ_32M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(145), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ej_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WE256EK", SNOR_ID(0x9d, 0x70, 0x19), SZ_32M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_DUAL_MAX_SPEED_MHZ(145), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25L*512M", SNOR_ID(0x9d, 0x60, 0x1a), SZ_64M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25lx512m_fixups),
	),

	SNOR_PART("IS25LP512MJ", SNOR_ID(0x9d, 0x60, 0x1a), SZ_64M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp512mj_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP512MK", SNOR_ID(0x9d, 0x60, 0x1a), SZ_64M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LE512MJ", SNOR_ID(0x9d, 0x60, 0x1a), SZ_64M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp512mj_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LE512MK", SNOR_ID(0x9d, 0x60, 0x1a), SZ_64M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25W*512M", SNOR_ID(0x9d, 0x70, 0x1a), SZ_64M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(93),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25wx512m_fixups),
	),

	SNOR_PART("IS25WP512MJ", SNOR_ID(0x9d, 0x70, 0x1a), SZ_64M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(112), SNOR_DUAL_MAX_SPEED_MHZ(112), SNOR_QUAD_MAX_SPEED_MHZ(93),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp512mj_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WP512MK", SNOR_ID(0x9d, 0x70, 0x1a), SZ_64M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(112), SNOR_DUAL_MAX_SPEED_MHZ(112), SNOR_QUAD_MAX_SPEED_MHZ(93),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WE512MJ", SNOR_ID(0x9d, 0x70, 0x1a), SZ_64M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(112), SNOR_DUAL_MAX_SPEED_MHZ(112), SNOR_QUAD_MAX_SPEED_MHZ(93),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp512mj_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WE512MK", SNOR_ID(0x9d, 0x70, 0x1a), SZ_64M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(112), SNOR_DUAL_MAX_SPEED_MHZ(112), SNOR_QUAD_MAX_SPEED_MHZ(93),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25L*01G", SNOR_ID(0x9d, 0x60, 0x1b), SZ_128M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25lx01g_fixups),
	),

	SNOR_PART("IS25LP01GJ", SNOR_ID(0x9d, 0x60, 0x1b), SZ_128M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp01gj_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP01GK", SNOR_ID(0x9d, 0x60, 0x1b), SZ_128M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LE01GJ", SNOR_ID(0x9d, 0x60, 0x1b), SZ_128M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp01gj_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LE01GK", SNOR_ID(0x9d, 0x60, 0x1b), SZ_128M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25W*01G", SNOR_ID(0x9d, 0x70, 0x1b), SZ_128M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(93),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25wx01g_fixups),
	),

	SNOR_PART("IS25WP01GJ", SNOR_ID(0x9d, 0x70, 0x1b), SZ_128M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(93),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp01gj_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WP01GK", SNOR_ID(0x9d, 0x70, 0x1b), SZ_128M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(93),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WE01GJ", SNOR_ID(0x9d, 0x70, 0x1b), SZ_128M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(93),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp01gj_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WE01GK", SNOR_ID(0x9d, 0x70, 0x1b), SZ_128M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(93),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25L*02GG", SNOR_ID(0x9d, 0x60, 0x22), SZ_256M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(95),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25lx02gg_fixups),
	),

	SNOR_PART("IS25LP02GGJ", SNOR_ID(0x9d, 0x60, 0x22), SZ_256M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4_512b),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp02ggj_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LP02GGK", SNOR_ID(0x9d, 0x60, 0x22), SZ_256M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4_512b),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LE02GGJ", SNOR_ID(0x9d, 0x60, 0x22), SZ_256M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp02ggj_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25LE02GGK", SNOR_ID(0x9d, 0x60, 0x22), SZ_256M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25W*02GG", SNOR_ID(0x9d, 0x70, 0x22), SZ_256M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4_512b),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25wx02gg_fixups),
	),

	SNOR_PART("IS25WP02GGJ", SNOR_ID(0x9d, 0x70, 0x22), SZ_256M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4_512b),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp02ggj_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WP02GGK", SNOR_ID(0x9d, 0x70, 0x22), SZ_256M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4_512b),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WE02GGJ", SNOR_ID(0x9d, 0x70, 0x22), SZ_256M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4_512b),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp02ggj_wpr_4bp_tbs_fixups),
	),

	SNOR_PART("IS25WE02GGK", SNOR_ID(0x9d, 0x70, 0x22), SZ_256M, /* SFDP 1.6, ASP */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(ISSI_F_RR_DC_BIT6_3 | ISSI_F_ECC),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(117), SNOR_QUAD_MAX_SPEED_MHZ(95),
		  SNOR_OTP_INFO(&issi_otp_4_512b),
		  SNOR_REGS(&is25xp512m_regs),
		  SNOR_FIXUPS(&is25xp256ek_wpr_4bp_tbs_fixups),
	),
};

static ufprog_status issi_otp_cb_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_IRRD, 1),
		SPI_MEM_OP_ADDR(3, addr, 1),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_DATA_IN(len, data, 1)
	);

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

static ufprog_status issi_otp_cb_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len,
				       const void *data)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_IRP, 1),
		SPI_MEM_OP_ADDR(3, addr, 1),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_DATA_OUT(len, data, 1)
	);

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

static ufprog_status issi_otp_cb_lock(struct spi_nor *snor, uint32_t index)
{
	uint8_t cb;

	STATUS_CHECK_RET(issi_otp_cb_read(snor, 0, snor->ext_param.otp->size - 1, 1, &cb));

	if (cb & BIT(0))
		return UFP_OK;

	cb |= BIT(0);

	return issi_otp_cb_write(snor, 0, snor->ext_param.otp->size - 1, 1, &cb);
}

static ufprog_status issi_otp_cb_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{
	uint8_t cb;

	STATUS_CHECK_RET(issi_otp_cb_read(snor, 0, snor->ext_param.otp->size - 1, 1, &cb));

	if (cb & BIT(0))
		*retlocked = true;
	else
		*retlocked = false;

	return UFP_OK;
}

static const struct spi_nor_flash_part_otp_ops issi_otp_cb_ops = {
	.read = issi_otp_cb_read,
	.write = issi_otp_cb_write,
	.lock = issi_otp_cb_lock,
	.locked = issi_otp_cb_locked,
};

static ufprog_status issi_otp_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	return secr_otp_read_paged_naddr(snor, SNOR_CMD_READ_IRL, index, addr, 3, len, data);
}

static ufprog_status issi_otp_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, const void *data)
{
	return secr_otp_write_paged_naddr(snor, SNOR_CMD_PROG_IRL, index, addr, 3, len, data);
}

static ufprog_status issi_otp_erase(struct spi_nor *snor, uint32_t index)
{
	return secr_otp_erase_naddr(snor, SNOR_CMD_ERASE_IRL, index, 3);
}

static ufprog_status issi_otp_lock(struct spi_nor *snor, uint32_t index)
{
	uint32_t val, bit;

	bit = ISSI_FR_OTP_IRL_SHIFT + index;

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &issi_fr_acc, 0, BIT(bit), false));
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &issi_fr_acc, &val));
	if (val & BIT(bit))
		return UFP_OK;

	return UFP_FAIL;
}

static ufprog_status issi_otp_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{
	uint32_t val, bit;

	bit = ISSI_FR_OTP_IRL_SHIFT + index;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &issi_fr_acc, &val));

	if (val & BIT(bit))
		*retlocked = true;
	else
		*retlocked = false;

	return UFP_OK;
}

static const struct spi_nor_flash_part_otp_ops issi_otp_ops = {
	.read = issi_otp_read,
	.write = issi_otp_write,
	.erase = issi_otp_erase,
	.lock = issi_otp_lock,
	.locked = issi_otp_locked,
};

static const struct spi_nor_flash_part_otp_ops issi_otp_no_erase_ops = {
	.read = issi_otp_read,
	.write = issi_otp_write,
	.lock = issi_otp_lock,
	.locked = issi_otp_locked,
};

static const struct spi_nor_flash_part_otp_ops issi_otp_wb_ops = {
	.read = secr_otp_read_paged,
	.write = secr_otp_write_paged,
	.erase = secr_otp_erase,
	.lock = secr_otp_lock,
	.locked = secr_otp_locked,
};

static ufprog_status issi_otp_fixup(struct spi_nor *snor)
{
	if (snor->ext_param.otp) {
		if ((snor->param.vendor_flags & ISSI_F_OTP_WB_MODE))
			snor->ext_param.ops.otp = &issi_otp_wb_ops;
		else if ((snor->param.vendor_flags & ISSI_F_OTP_CB_MODE))
			snor->ext_param.ops.otp = &issi_otp_cb_ops;
		else if ((snor->param.vendor_flags & ISSI_F_OTP_NO_ERASE))
			snor->ext_param.ops.otp = &issi_otp_no_erase_ops;
		else
			snor->ext_param.ops.otp = &issi_otp_ops;
	}

	return UFP_OK;
}

static ufprog_status issi_part_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				     struct spi_nor_flash_part_blank *bp)
{
	spi_nor_blank_part_fill_default_opcodes(bp);

	if (snor->sfdp.bfpt && snor->sfdp.bfpt_hdr->minor_ver >= SFDP_REV_MINOR_B) {
		bp->p.flags |= SNOR_F_UNIQUE_ID;

		bp->p.pp_io_caps |= BIT_SPI_MEM_IO_1_1_4;
		bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].opcode = SNOR_CMD_PAGE_PROG_QUAD_IN;
		bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].nmode = 0;

		if (bp->p.read_io_caps & BIT_SPI_MEM_IO_4_4_4) {
			bp->p.pp_io_caps |= BIT_SPI_MEM_IO_4_4_4;
			bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].opcode = SNOR_CMD_PAGE_PROG;
			bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;
		}

		if (bp->p.size > SZ_16M && (bp->p.a4b_flags & SNOR_4B_F_OPCODE)) {
			bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].opcode = SNOR_CMD_4B_PAGE_PROG_QUAD_IN;
			bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].ndummy = bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].nmode = 0;
		}

		if (!bp->p.otp)
			bp->p.otp = &issi_otp_4;
	}

	if (!(bp->p.vendor_flags & ISSI_F_OTP_WB_MODE)) {
		bp->p.flags &= ~SNOR_F_SR_VOLATILE_WREN_50H;
		bp->p.flags |= SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE;
	}

	/* 6 dummy cycles will be used for QPI read by default */
	if (bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].opcode) {
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 6;
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;
	}

	if (spi_nor_test_io_opcode(snor, bp->read_opcodes_4b, SPI_MEM_IO_1_1_4, 3, SPI_DATA_IN) ||
		spi_nor_test_io_opcode(snor, bp->read_opcodes_4b, SPI_MEM_IO_1_4_4, 3, SPI_DATA_IN) ||
		spi_nor_test_io_opcode(snor, bp->read_opcodes_4b, SPI_MEM_IO_4_4_4, 3, SPI_DATA_IN) ||
		spi_nor_test_io_opcode(snor, bp->pp_opcodes_4b, SPI_MEM_IO_1_1_4, 3, SPI_DATA_OUT) ||
		spi_nor_test_io_opcode(snor, bp->pp_opcodes_4b, SPI_MEM_IO_4_4_4, 3, SPI_DATA_OUT)) {
		if (bp->p.vendor_flags & (ISSI_F_RR_DC_BIT4_3 | ISSI_F_RR_DC_BIT6_3)) {
			bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].ndummy = 8;
			bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].nmode = 0;
			bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].ndummy = 8;
			bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].nmode = 0;
			bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = 8;
			bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = 0;
			bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy = 8;
			bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].nmode = 0;
			bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = 8;
			bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;
			bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 8;
			bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;
		}
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup issi_fixups = {
	.pre_param_setup = issi_part_fixup,
	.pre_chip_setup = issi_otp_fixup,
};

static ufprog_status issi_chip_setup(struct spi_nor *snor)
{
	uint32_t regval;

	if (spi_mem_io_info_data_bw(snor->state.read_io_info) == 4 || spi_mem_io_info_data_bw(snor->state.pp_io_info)) {
		if (snor->param.vendor_flags & ISSI_F_RR_DC_BIT4_3) {
			STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &issi_rr_acc, BITS(4, 3), 2 << 3, true));
			STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &issi_rr_acc, &regval));
			if (((regval & BITS(4, 3)) >> 3) != 2) {
				logm_err("Failed to set Read Dummy Cycles to 8\n");
				return UFP_FAIL;
			}
		} else if (snor->param.vendor_flags & ISSI_F_RR_DC_BIT6_3) {
			STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &issi_rr_acc, BITS(6, 3), 8 << 3, true));
			STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &issi_rr_acc, &regval));
			if (((regval & BITS(6, 3)) >> 3) != 8) {
				logm_err("Failed to set Read Dummy Cycles to 8\n");
				return UFP_FAIL;
			}
		}
	}

	return UFP_OK;
}

static ufprog_status issi_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_READ_UNIQUE_ID, 1),
		SPI_MEM_OP_ADDR(3, 0, 1),
		SPI_MEM_OP_DUMMY(1, 1),
		SPI_MEM_OP_DATA_IN(ISSI_UID_LEN, data, 1)
	);

	if (retlen)
		*retlen = ISSI_UID_LEN;

	if (!data)
		return UFP_OK;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static const struct spi_nor_flash_part_ops issi_default_part_ops = {
	.chip_setup = issi_chip_setup,
	.read_uid = issi_read_uid,
	.qpi_dis = spi_nor_disable_qpi_f5h,
};

const struct spi_nor_vendor vendor_issi = {
	.mfr_id = SNOR_VENDOR_ISSI,
	.id = "issi",
	.name = "ISSI",
	.parts = issi_parts,
	.nparts = ARRAY_SIZE(issi_parts),
	.vendor_flag_names = issi_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(issi_vendor_flag_info),
	.default_part_ops = &issi_default_part_ops,
	.default_part_fixups = &issi_fixups,
};

static const struct spi_nor_reg_field_item is25lv_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(7, 1, "WPEN", "WP# Pin Enable"),
};

static const struct spi_nor_reg_def is25lv_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, is25lv_sr_fields);

static const struct snor_reg_info is25lv_regs = SNOR_REG_INFO(&is25lv_sr);

static const struct spi_nor_erase_info pm25lv_erase_opcodes = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_4K, SNOR_CMD_PMC_SECTOR_ERASE),
	SNOR_ERASE_SECTOR(SZ_32K, SNOR_CMD_BLOCK_ERASE),
);

static const struct spi_nor_erase_info pm25lv020_erase_opcodes = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_4K, SNOR_CMD_PMC_SECTOR_ERASE),
	SNOR_ERASE_SECTOR(SZ_64K, SNOR_CMD_BLOCK_ERASE),
);

static DEFINE_SNOR_ALIAS(pm25lq020_alias, SNOR_ALIAS_MODEL("PM25LQ020C"));
static DEFINE_SNOR_ALIAS(pm25lq040_alias, SNOR_ALIAS_MODEL("PM25LQ040C"));

static const struct spi_nor_flash_part pmc_parts[] = {
	SNOR_PART("PM25LV512", SNOR_ID_NONE, SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_ERASE_INFO(&pm25lv_erase_opcodes),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(25),
		  SNOR_REGS(&is25lv_regs),
		  SNOR_WP_RANGES(&is25cd512_wpr_2bp),
	),

	SNOR_PART("PM25LV512A", SNOR_ID(0x7f, 0x9d, 0x7b), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_ERASE_INFO(&pm25lv_erase_opcodes),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(100),
		  SNOR_REGS(&is25lv_regs),
		  SNOR_WP_RANGES(&is25cd512_wpr_2bp),
	),

	SNOR_PART("PM25LQ512B", SNOR_ID(0x7f, 0x9d, 0x20), SZ_64K, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_FIXUPS(&pm25lq512b_fixups),
	),

	SNOR_PART("PM25LV010", SNOR_ID_NONE, SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_ERASE_INFO(&pm25lv_erase_opcodes),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(25),
		  SNOR_REGS(&is25lv_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up_ratio),
	),

	SNOR_PART("PM25LV010A", SNOR_ID(0x7f, 0x9d, 0x7c), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_ERASE_INFO(&pm25lv_erase_opcodes),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(100),
		  SNOR_REGS(&is25lv_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up_ratio),
	),

	SNOR_PART("PM25LQ010B", SNOR_ID(0x7f, 0x9d, 0x21), SZ_128K, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("PM25LV020", SNOR_ID(0x7f, 0x9d, 0x7d), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_ERASE_INFO(&pm25lv020_erase_opcodes),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&is25lv_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up_ratio),
	),

	SNOR_PART("PM25LQ020", SNOR_ID(0x9d, 0x11, 0x42), SZ_256K,
		  SNOR_ALIAS(&pm25lq020_alias), /* PM25LQ020C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_CB_MODE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_1),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("PM25LQ020B", SNOR_ID(0x7f, 0x9d, 0x42), SZ_256K, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("PM25LV040", SNOR_ID(0x7f, 0x9d, 0x7e), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_ERASE_INFO(&pm25lv020_erase_opcodes),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&is25cd_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("PM25LQ040", SNOR_ID(0x9d, 0x11, 0x43), SZ_512K,
		  SNOR_ALIAS(&pm25lq040_alias), /* PM25LQ040C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_CB_MODE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_1),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("PM25LQ040B", SNOR_ID(0x7f, 0x9d, 0x7e), SZ_512K, /* SFDP 1.5 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_NO_ERASE),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_4),
		  SNOR_REGS(&is25lqxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("PM25LV080B", SNOR_ID(0x7f, 0x9d, 0x13), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(100),
		  SNOR_REGS(&is25cd_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("PM25LV016B", SNOR_ID(0x7f, 0x9d, 0x14), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(100),
		  SNOR_REGS(&is25cd_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("PM25LQ080", SNOR_ID(0x9d, 0x13, 0x44), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_CB_MODE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_1),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("PM25LQ016", SNOR_ID(0x7f, 0x9d, 0x45), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_CB_MODE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_1),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("PM25LQ032C", SNOR_ID(0x7f, 0x9d, 0x46), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ISSI_F_OTP_CB_MODE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&issi_otp_1_64b),
		  SNOR_REGS(&is25lqxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),
};

const struct spi_nor_vendor vendor_issi_pmc = {
	.mfr_id = SNOR_VENDOR_ISSI,
	.id = "pmc",
	.name = "ISSI/PMC",
	.parts = pmc_parts,
	.nparts = ARRAY_SIZE(pmc_parts),
	.vendor_flag_names = issi_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(issi_vendor_flag_info),
	.default_part_ops = &issi_default_part_ops,
	.default_part_fixups = &issi_fixups,
};
