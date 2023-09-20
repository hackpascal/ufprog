// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * EON SPI-NOR flash parts
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

#define EON_UID_LEN				12
#define EON_UID_4BH_LEN				16

 /* Status Register OTP bit */
#define SR_OTP_LOCK				BIT(7)

 /* Status Register bit */
#define SR_TB					BIT(5)
#define SR_BP3					BIT(5)
#define SR_SEC					BIT(6)

 /* BP Masks */
#define BP_2_0					(SR_BP2 | SR_BP1 | SR_BP0)
#define BP_2_0_TB				(SR_TB | SR_BP2 | SR_BP1 | SR_BP0)
#define BP_3_0					(SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0)
#define BP_2_0_TB_SEC				(SR_SEC | SR_TB | SR_BP2 | SR_BP1 | SR_BP0)

 /* EON vendor flags */
#define EON_F_OTP_TYPE_1			BIT(0)
#define EON_F_OTP_TYPE_2			BIT(1)
#define EON_F_OTP_TYPE_3			BIT(2)
#define EON_F_OTP_TYPE_4			BIT(3)
#define EON_F_OTP_TYPE_SECR			BIT(4)
#define EON_F_READ_UID_4BH			BIT(5)
#define EON_F_READ_UID_SFDP_1E0H		BIT(6)
#define EON_F_HIGH_BANK_LATCH			BIT(7)
#define EON_F_DC_SR3_BIT5_4			BIT(8)
#define EON_F_DC_SR3_BIT7			BIT(9)

static const struct spi_nor_part_flag_enum_info eon_vendor_flag_info[] = {
	{ 0, "otp-type-1" },
	{ 1, "otp-type-2" },
	{ 2, "otp-type-3" },
	{ 3, "otp-type-4" },
	{ 4, "otp-type-secr" },
	{ 5, "read-uid-4bh" },
	{ 6, "read-uid-sfdp-1e0h" },
	{ 7, "high-bank-latch" },
	{ 8, "dummy-cycles-sr3-bit5-4" },
	{ 9, "dummy-cycles-sr3-bit7" },
};

static ufprog_status eon_otp_sr_pre_acc(struct spi_nor *snor, const struct spi_nor_reg_access *access);
static ufprog_status eon_otp_sr_post_acc(struct spi_nor *snor, const struct spi_nor_reg_access *access);

static struct spi_nor_reg_access eon_otp_sr_acc = {
	.type = SNOR_REG_NORMAL,
	.num = 1,
	.desc[0] = {
		.ndata = 1,
		.read_opcode = SNOR_CMD_READ_SR,
		.write_opcode = SNOR_CMD_WRITE_SR,
	},
	.pre_acc = eon_otp_sr_pre_acc,
	.post_acc = eon_otp_sr_post_acc,
};

static const struct spi_nor_reg_access eon_sr1_sr4_acc = {
	.type = SNOR_REG_NORMAL,
	.num = 2,
	.desc[0] = {
		.read_opcode = SNOR_CMD_READ_SR,
		.write_opcode = SNOR_CMD_WRITE_SR,
	},
	.desc[1] = {
		.read_opcode = SNOR_CMD_EON_READ_SR4,
		.write_opcode = SNOR_CMD_EON_WRITE_SR4,
	},
};

static const struct spi_nor_reg_field_item en25p05_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def en25p05_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, en25p05_sr_fields);

static const struct snor_reg_info en25p05_regs = SNOR_REG_INFO(&en25p05_sr);

static const struct spi_nor_reg_field_item en25f_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def en25f_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, en25f_sr_fields);

static const struct snor_reg_info en25f_regs = SNOR_REG_INFO(&en25f_sr);

static const struct spi_nor_reg_field_item en25q_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD_YES_NO(6, 1, "WHDIS", "WP# and Hold# Disable"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def en25q_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, en25q_sr_fields);

static const struct snor_reg_info en25q_regs = SNOR_REG_INFO(&en25q_sr);

static const struct spi_nor_reg_field_item en25fxa_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD_YES_NO(6, 1, "WHDIS", "WP# and Hold# Disable"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def en25fxa_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, en25fxa_sr_fields);

static const struct snor_reg_info en25f40a_regs = SNOR_REG_INFO(&en25fxa_sr);

static const struct spi_nor_reg_field_item en25s40a_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def en25s40a_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, en25s40a_sr_fields);

static const struct snor_reg_info en25s40a_regs = SNOR_REG_INFO(&en25s40a_sr);

static const struct spi_nor_reg_field_values en25fxa_otp_sr_tb_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "Top"),
	VALUE_ITEM(1, "Bottom"),
);

static const struct spi_nor_reg_field_item en25fxa_otp_sr_fields[] = {
	SNOR_REG_FIELD_YES_NO(1, 1, "SPL2", "Security Sector 2 Lock"),
	SNOR_REG_FIELD_YES_NO(2, 1, "SPL1", "Security Sector 1 Lock"),
	SNOR_REG_FIELD_ENABLED_DISABLED(3, 1, "EBL", "Enable Boot Lock"),
	SNOR_REG_FIELD_YES_NO(4, 1, "4KBL", "4KB Boot Lock"),
	SNOR_REG_FIELD_FULL(6, 1, "TBL", "Top/Bottom Lock", &en25fxa_otp_sr_tb_values),
	SNOR_REG_FIELD_YES_NO(7, 1, "SPL0", "Security Sector 0 Lock"),
};

static const struct spi_nor_reg_def en25fxa_otp_sr = SNOR_REG_DEF("OTP", "OTP Status Register", &eon_otp_sr_acc,
								  en25fxa_otp_sr_fields);

static const struct snor_reg_info en25fxa_regs = SNOR_REG_INFO(&en25fxa_sr, &en25fxa_otp_sr);

static const struct spi_nor_reg_field_item en25qxb_sr1_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Protect"),
	SNOR_REG_FIELD(6, 1, "SEC", "Sector Protect"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def en25qxb_sr1 = SNOR_REG_DEF("SR1", "Status Register 1", &sr_acc, en25qxb_sr1_fields);

static const struct spi_nor_reg_field_item en25qxb_otp_sr_fields[] = {
	SNOR_REG_FIELD_YES_NO(1, 1, "SPL2", "Security Sector 2 Lock"),
	SNOR_REG_FIELD_YES_NO(2, 1, "SPL1", "Security Sector 1 Lock"),
	SNOR_REG_FIELD_ENABLED_DISABLED(3, 1, "EBL", "Enable Boot Lock"),
	SNOR_REG_FIELD_YES_NO(7, 1, "SPL0", "Security Sector 0 Lock"),
};

static const struct spi_nor_reg_def en25qxb_otp_sr = SNOR_REG_DEF("OTP", "OTP Status Register", &eon_otp_sr_acc,
								  en25qxb_otp_sr_fields);

static const struct spi_nor_reg_field_item en25qxb_sr4_fields[] = {
	SNOR_REG_FIELD_YES_NO(1, 1, "HDEN", "HOLD# Enable"),
	SNOR_REG_FIELD_YES_NO(2, 1, "WHDIS", "WP# Disable"),
	SNOR_REG_FIELD(6, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def en25qxb_sr4 = SNOR_REG_DEF("SR4", "Status Register 4", &sr_acc, en25qxb_sr4_fields);

static const struct snor_reg_info en25qxb_regs = SNOR_REG_INFO(&en25qxb_sr1, &en25qxb_otp_sr, &en25qxb_sr4);

static const struct spi_nor_reg_field_item en25qaxa_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(7, 1, "PPB", "Permanent Protection Bit"),
};

static const struct spi_nor_reg_def en25qaxa_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, en25qaxa_sr_fields);

static const struct spi_nor_reg_field_item en25qaxa_otp_sr_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(3, 1, "EBL", "Enable Boot Lock"),
	SNOR_REG_FIELD_YES_NO(4, 1, "4KBL", "4KB Boot Lock"),
	SNOR_REG_FIELD_FULL(6, 1, "TBL", "Top/Bottom Lock", &en25fxa_otp_sr_tb_values),
};

static const struct spi_nor_reg_def en25qaxa_otp_sr = SNOR_REG_DEF("OTP", "OTP Status Register", &eon_otp_sr_acc,
								 en25qaxa_otp_sr_fields);

static const struct snor_reg_info en25qaxa_regs = SNOR_REG_INFO(&en25qaxa_sr, &en25qaxa_otp_sr);

static const struct spi_nor_reg_field_item en25qaxb_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD_ENABLED_DISABLED(6, 1, "EBL", "Enable Boot Lock"),
	SNOR_REG_FIELD(7, 1, "PPB", "Permanent Protection Bit"),
};

static const struct spi_nor_reg_def en25qaxb_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, en25qaxb_sr_fields);

static const struct spi_nor_reg_field_item en25qaxb_otp_sr_fields[] = {
	SNOR_REG_FIELD_YES_NO(1, 1, "SPL2", "Security Sector 2 Lock"),
	SNOR_REG_FIELD_YES_NO(2, 1, "SPL1", "Security Sector 1 Lock"),
	SNOR_REG_FIELD_FULL(3, 1, "TBL", "Top/Bottom Lock", &en25fxa_otp_sr_tb_values),
	SNOR_REG_FIELD_YES_NO(4, 1, "4KBL", "4KB Boot Lock"),
	SNOR_REG_FIELD_YES_NO(7, 1, "SPL0", "Security Sector 0 Lock"),
};

static const struct spi_nor_reg_def en25qaxb_otp_sr = SNOR_REG_DEF("OTP", "OTP Status Register", &eon_otp_sr_acc,
								   en25qaxb_otp_sr_fields);

static const struct snor_reg_info en25qaxb_regs = SNOR_REG_INFO(&en25qaxb_sr, &en25qaxb_otp_sr);

static const struct spi_nor_reg_field_item en25qa64a_otp_sr_fields[] = {
	SNOR_REG_FIELD_FULL(3, 1, "TBL", "Top/Bottom Lock", &en25fxa_otp_sr_tb_values),
	SNOR_REG_FIELD_YES_NO(4, 1, "4KBL", "4KB Boot Lock"),
};

static const struct spi_nor_reg_def en25qa64a_otp_sr = SNOR_REG_DEF("OTP", "OTP Status Register", &eon_otp_sr_acc,
								    en25qa64a_otp_sr_fields);

static const struct snor_reg_info en25qa64a_regs = SNOR_REG_INFO(&en25qaxb_sr, &en25qa64a_otp_sr);

static const struct spi_nor_reg_field_item en25qe_sr2_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(1, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(3, 1, "SPL2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(4, 1, "SPL1", "Security Register Lock Bit 1"),
	SNOR_REG_FIELD(5, 1, "SPL0", "Security Register Lock Bit 0"),
	SNOR_REG_FIELD(6, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def en25qe_sr2 = SNOR_REG_DEF("SR2", "Status Register 2", &cr_acc, en25qe_sr2_fields);

static const struct spi_nor_reg_field_item en25qe_sr3_fields[] = {
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD(7, 1, "DC", "Dummy Configuration"),
};

static const struct spi_nor_reg_def en25qe_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc, en25qe_sr3_fields);

static const struct snor_reg_info en25qe_regs = SNOR_REG_INFO(&en25qxb_sr1, &en25qe_sr2, &en25qe_sr3);

static const struct spi_nor_reg_field_item en25qhxb_otp_sr_fields[] = {
	SNOR_REG_FIELD_YES_NO(1, 1, "SPL2", "Security Sector 2 Lock"),
	SNOR_REG_FIELD_YES_NO(2, 1, "SPL1", "Security Sector 1 Lock"),
	SNOR_REG_FIELD_ENABLED_DISABLED(3, 1, "EBL", "Enable Boot Lock"),
	SNOR_REG_FIELD(4, 1, "CMPL", "Complement Protect Lock"),
	SNOR_REG_FIELD_YES_NO(6, 1, "WHDIS", "WP# and Hold# Disable"),
	SNOR_REG_FIELD_YES_NO(7, 1, "SPL0", "Security Sector 0 Lock"),
};

static const struct spi_nor_reg_def en25qhxb_otp_sr = SNOR_REG_DEF("OTP", "OTP Status Register", &eon_otp_sr_acc,
								   en25qhxb_otp_sr_fields);

static const struct snor_reg_info en25qhxb_regs = SNOR_REG_INFO(&en25qxb_sr1, &en25qhxb_otp_sr);

static const struct snor_reg_info en25qh32a_regs = SNOR_REG_INFO(&en25fxa_sr, &en25qaxa_otp_sr);

static const struct spi_nor_reg_field_item en25qh32b_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD_ENABLED_DISABLED(6, 1, "EBL", "Enable Boot Lock"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def en25qh32b_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, en25qh32b_sr_fields);

static const struct spi_nor_reg_field_item en25qh32b_otp_sr_fields[] = {
	SNOR_REG_FIELD_YES_NO(1, 1, "SPL2", "Security Sector 2 Lock"),
	SNOR_REG_FIELD_YES_NO(2, 1, "SPL1", "Security Sector 1 Lock"),
	SNOR_REG_FIELD_FULL(3, 1, "TBL", "Top/Bottom Lock", &en25fxa_otp_sr_tb_values),
	SNOR_REG_FIELD_YES_NO(4, 1, "4KBL", "4KB Boot Lock"),
	SNOR_REG_FIELD_YES_NO(6, 1, "WHDIS", "WP# and Hold# Disable"),
	SNOR_REG_FIELD_YES_NO(7, 1, "SPL0", "Security Sector 0 Lock"),
};

static const struct spi_nor_reg_def en25qh32b_otp_sr = SNOR_REG_DEF("OTP", "OTP Status Register", &eon_otp_sr_acc,
								    en25qh32b_otp_sr_fields);

static const struct snor_reg_info en25qh32b_regs = SNOR_REG_INFO(&en25qh32b_sr, &en25qh32b_otp_sr);

static const struct spi_nor_reg_field_values en25qh64a_otp_sr_hrsw_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "HOLD#"),
	VALUE_ITEM(1, "RESET#"),
);

static const struct spi_nor_reg_field_item en25qh64a_otp_sr_fields[] = {
	SNOR_REG_FIELD_FULL(3, 1, "TBL", "Top/Bottom Lock", &en25fxa_otp_sr_tb_values),
	SNOR_REG_FIELD_YES_NO(4, 1, "4KBL", "4KB Boot Lock"),
	SNOR_REG_FIELD_FULL(5, 1, "HRSW", "HOLD#/RESET# Select", &en25qh64a_otp_sr_hrsw_values),
	SNOR_REG_FIELD_YES_NO(6, 1, "WXDIS", "WP# and HOLD#/RESET# Disable"),
};

static const struct spi_nor_reg_def en25qh64a_otp_sr = SNOR_REG_DEF("OTP", "OTP Status Register", &eon_otp_sr_acc,
								    en25qh64a_otp_sr_fields);

static const struct snor_reg_info en25qh64a_regs = SNOR_REG_INFO(&en25qh32b_sr, &en25qh64a_otp_sr);

static const struct spi_nor_reg_field_item en25qh256a_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(6, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def en25qh256a_sr1 = SNOR_REG_DEF("SR1", "Status Register 1", &sr_acc,
								  en25qh256a_sr_fields);

static const struct spi_nor_reg_field_item en25qh256a_otp_sr_fields[] = {
	SNOR_REG_FIELD_YES_NO(1, 1, "SPL2", "Security Sector 2 Lock"),
	SNOR_REG_FIELD_YES_NO(2, 1, "SPL1", "Security Sector 1 Lock"),
	SNOR_REG_FIELD_YES_NO(7, 1, "SPL0", "Security Sector 0 Lock"),
};

static const struct spi_nor_reg_def en25qh256a_otp_sr = SNOR_REG_DEF("OTP", "OTP Status Register", &eon_otp_sr_acc,
								     en25qh256a_otp_sr_fields);

static const struct spi_nor_reg_field_item en25qh256a_sr4_fields[] = {
	SNOR_REG_FIELD_YES_NO(1, 1, "HDDIS", "HOLD# Disable"),
	SNOR_REG_FIELD_YES_NO(2, 1, "WPDIS", "WP# Disable"),
	SNOR_REG_FIELD_YES_NO(3, 1, "RSEN", "RESET# Enable"),
	SNOR_REG_FIELD_FULL(4, 1, "4BP", "Power-up Address Mode Select", &w25q_sr3_adp_values),
	SNOR_REG_FIELD(6, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def en25qh256a_sr4 = SNOR_REG_DEF("SR4", "Status Register 4", &sr_acc,
								  en25qh256a_sr4_fields);

static const struct snor_reg_info en25qh256a_regs = SNOR_REG_INFO(&en25qh256a_sr1, &en25qh256a_otp_sr, &en25qh256a_sr4);

static const struct spi_nor_reg_field_values en25qx_sr3_bl_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "8 Bytes"),
	VALUE_ITEM(1, "16 Bytes"),
	VALUE_ITEM(2, "32 Bytes"),
	VALUE_ITEM(3, "64 Bytes"),
);

static const struct spi_nor_reg_field_item en25qx_sr3_fields[] = {
	SNOR_REG_FIELD_FULL(3, 3, "BL", "Burst Length", &en25qx_sr3_bl_values),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD(7, 1, "DC", "Dummy Configuration"),
};

static const struct spi_nor_reg_def en25qx_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc, en25qx_sr3_fields);

static const struct snor_reg_info en25qx_regs = SNOR_REG_INFO(&en25qxb_sr1, &en25qe_sr2, &en25qx_sr3);

static const struct spi_nor_reg_field_item en25qx256a_sr3_fields[] = {
	SNOR_REG_FIELD_FULL(1, 1, "4BP", "Power-up Address Mode Select", &w25q_sr3_adp_values),
	SNOR_REG_FIELD_FULL(3, 3, "BL", "Burst Length", &en25qx_sr3_bl_values),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD(7, 1, "DC", "Dummy Configuration"),
};

static const struct spi_nor_reg_def en25qx256a_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								  en25qx256a_sr3_fields);

static const struct snor_reg_info en25qx256a_regs = SNOR_REG_INFO(&en25qxb_sr1, &en25qe_sr2, &en25qx256a_sr3);

static const struct spi_nor_reg_field_item en25s32a_sr4_fields[] = {
	SNOR_REG_FIELD_YES_NO(1, 1, "HDDIS", "HOLD# Disable"),
	SNOR_REG_FIELD_YES_NO(2, 1, "WHDIS", "WP# Disable"),
	SNOR_REG_FIELD(6, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def en25s32a_sr4 = SNOR_REG_DEF("SR4", "Status Register 4", &sr_acc,
								en25s32a_sr4_fields);

static const struct snor_reg_info en25s32a_regs = SNOR_REG_INFO(&en25qxb_sr1, &en25qxb_otp_sr, &en25s32a_sr4);

static const struct spi_nor_otp_info eon_otp_128b = {
	.start_index = 0,
	.count = 1,
	.size = 0x80,
};

static const struct spi_nor_otp_info eon_otp_256b = {
	.start_index = 0,
	.count = 1,
	.size = 0x100,
};

static const struct spi_nor_otp_info eon_otp_512b = {
	.start_index = 0,
	.count = 1,
	.size = 0x200,
};

static const struct spi_nor_otp_info eon_otp_3x512b = {
	.start_index = 0,
	.count = 3,
	.size = 0x200,
};

static const struct spi_nor_otp_info eon_otp_3x1k = {
	.start_index = 0,
	.count = 3,
	.size = 0x400,
};

static const struct spi_nor_wp_info en25f05_wpr = SNOR_WP_BP(&sr_acc, BP_2_0,
	SNOR_WP_NONE(     0                          ),	/* None */
	SNOR_WP_NONE(     SR_BP2                     ),	/* None */

	SNOR_WP_ALL(      SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(               SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_RP_UP(                      SR_BP0, 2),		/* Upper 1/4 */
	SNOR_WP_RP_UP(             SR_BP1         , 1),		/* Upper 1/2 */

	SNOR_WP_SP_CMP_LO(SR_BP2 |          SR_BP0, 1),		/* Lower T - 8KB */
	SNOR_WP_SP_CMP_LO(SR_BP2 | SR_BP1         , 0),		/* Lower T - 4KB */
);

static const struct spi_nor_wp_info en25e10_wpr = SNOR_WP_BP(&sr_acc, BP_2_0,
	SNOR_WP_NONE(     0                          ),	/* None */

	SNOR_WP_ALL(      SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP2 |          SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP2 | SR_BP1            ),	/* All */

	SNOR_WP_SP_CMP_LO(                  SR_BP0, 1),		/* Lower T - 8KB */
	SNOR_WP_SP_CMP_LO(         SR_BP1         , 2),		/* Lower T - 16KB */
	SNOR_WP_SP_CMP_LO(         SR_BP1 | SR_BP0, 3),		/* Lower T - 32KB */
	SNOR_WP_SP_CMP_LO(SR_BP2                  , 4),		/* Lower T - 64KB */
);

static const struct spi_nor_wp_info en25s10_wpr = SNOR_WP_BP(&sr_acc, BP_2_0,
	SNOR_WP_NONE(     0                          ),	/* None */
	SNOR_WP_NONE(     SR_BP2                     ),	/* None */

	SNOR_WP_ALL(               SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_SP_CMP_LO(                  SR_BP0, 4),		/* Lower T - 64KB */
	SNOR_WP_SP_CMP_LO(         SR_BP1         , 3),		/* Lower T - 32KB */
	SNOR_WP_SP_CMP_LO(SR_BP2 |          SR_BP0, 2),		/* Lower T - 16KB */
	SNOR_WP_SP_CMP_LO(SR_BP2 | SR_BP1         , 1),		/* Lower T - 8KB */
);

static const struct spi_nor_wp_info en25q40_wpr = SNOR_WP_BP(&sr_acc, BP_2_0,
	SNOR_WP_NONE(     0                          ),	/* None */

	SNOR_WP_ALL(      SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_SP_CMP_LO(                  SR_BP0, 1),		/* Lower T - 8KB */
	SNOR_WP_SP_CMP_LO(         SR_BP1         , 2),		/* Lower T - 16KB */
	SNOR_WP_SP_CMP_LO(         SR_BP1 | SR_BP0, 3),		/* Lower T - 32KB */
	SNOR_WP_SP_CMP_LO(SR_BP2                  , 4),		/* Lower T - 64KB */
	SNOR_WP_SP_CMP_LO(SR_BP2 |          SR_BP0, 5),		/* Lower T - 128KB */
	SNOR_WP_SP_CMP_LO(SR_BP2 | SR_BP1         , 6),		/* Lower T - 256KB */
);

static const struct spi_nor_wp_info en25s20a_wpr = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_NONE(     0                                  ),	/* None */
	SNOR_WP_NONE(     SR_TB                              ),	/* None */

	SNOR_WP_ALL(              SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(              SR_BP2                     ),	/* All */
	SNOR_WP_ALL(              SR_BP2 |          SR_BP0   ),	/* All */
	SNOR_WP_ALL(              SR_BP2 | SR_BP1            ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2                     ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 |          SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 | SR_BP1            ),	/* All */

	SNOR_WP_BP_UP(                              SR_BP0, 0),		/* Upper 64KB */
	SNOR_WP_BP_UP(                     SR_BP1         , 1),		/* Upper 128KB */
	SNOR_WP_BP_CMP_UP(                 SR_BP1 | SR_BP0, 0),		/* Upper T - 64KB */

	SNOR_WP_BP_LO(    SR_TB |                   SR_BP0, 0),		/* Lower 64KB */
	SNOR_WP_BP_LO(    SR_TB |          SR_BP1         , 1),		/* Lower 128KB */
	SNOR_WP_BP_CMP_LO(SR_TB |          SR_BP1 | SR_BP0, 0),		/* Lower T - 64KB */
);

static const struct spi_nor_wp_info en25f40a_wpr = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_NONE(     0                                  ),	/* None */
	SNOR_WP_NONE(     SR_TB                              ),	/* None */

	SNOR_WP_ALL(              SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(              SR_BP2 | SR_BP1            ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 | SR_BP1            ),	/* All */

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

static const struct spi_nor_wp_info en25s40a_wpr = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_NONE(     0                                  ),	/* None */
	SNOR_WP_NONE(     SR_TB                              ),	/* None */

	SNOR_WP_ALL(              SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(              SR_BP2 |          SR_BP0   ),	/* All */
	SNOR_WP_ALL(              SR_BP2 | SR_BP1            ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 |          SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 | SR_BP1            ),	/* All */

	SNOR_WP_BP_UP(                              SR_BP0, 0),		/* Upper 64KB */
	SNOR_WP_BP_UP(                     SR_BP1         , 1),		/* Upper 128KB */
	SNOR_WP_BP_UP(                     SR_BP1 | SR_BP0, 2),		/* Upper 256KB */
	SNOR_WP_BP_UP(            SR_BP2                  , 3),		/* Upper 512KB */

	SNOR_WP_BP_CMP_LO(SR_TB |                   SR_BP0, 0),		/* Lower T - 64KB */
	SNOR_WP_BP_CMP_LO(SR_TB |          SR_BP1         , 1),		/* Lower T - 128KB */
	SNOR_WP_BP_CMP_LO(SR_TB |          SR_BP1 | SR_BP0, 2),		/* Lower T - 256KB */
	SNOR_WP_BP_CMP_LO(SR_TB | SR_BP2                  , 3),		/* Lower T - 512KB */
);

static const struct spi_nor_wp_info en25q80b_wpr = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_NONE(     0                                  ),	/* None */
	SNOR_WP_NONE(     SR_TB                              ),	/* None */

	SNOR_WP_ALL(              SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_SP_CMP_LO(                          SR_BP0, 1),		/* Lower T - 8KB */
	SNOR_WP_SP_CMP_LO(                 SR_BP1         , 2),		/* Lower T - 16KB */
	SNOR_WP_SP_CMP_LO(                 SR_BP1 | SR_BP0, 3),		/* Lower T - 32KB */
	SNOR_WP_SP_CMP_LO(        SR_BP2                  , 4),		/* Lower T - 64KB */
	SNOR_WP_SP_CMP_LO(        SR_BP2 |          SR_BP0, 5),		/* Lower T - 128KB */
	SNOR_WP_SP_CMP_LO(        SR_BP2 | SR_BP1         , 6),		/* Lower T - 256KB */

	SNOR_WP_SP_LO(    SR_TB |                   SR_BP0, 1),		/* Lower 8KB */
	SNOR_WP_SP_LO(    SR_TB |          SR_BP1         , 2),		/* Lower 16KB */
	SNOR_WP_SP_LO(    SR_TB |          SR_BP1 | SR_BP0, 3),		/* Lower 32KB */
	SNOR_WP_SP_LO(    SR_TB | SR_BP2                  , 4),		/* Lower 64KB */
	SNOR_WP_SP_LO(    SR_TB | SR_BP2 |          SR_BP0, 5),		/* Lower 128KB */
	SNOR_WP_SP_LO(    SR_TB | SR_BP2 | SR_BP1         , 6),		/* Lower 256KB */
);

static const struct spi_nor_wp_info en25q16_wpr = SNOR_WP_BP(&sr_acc, BP_2_0,
	SNOR_WP_NONE(     0                          ),	/* None */

	SNOR_WP_ALL(      SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP2 | SR_BP1            ),	/* All */

	SNOR_WP_BP_CMP_LO(                  SR_BP0, 0),		/* Lower T - 64KB */
	SNOR_WP_BP_CMP_LO(         SR_BP1         , 1),		/* Lower T - 128KB */
	SNOR_WP_BP_CMP_LO(         SR_BP1 | SR_BP0, 2),		/* Lower T - 256KB */
	SNOR_WP_BP_CMP_LO(SR_BP2                  , 3),		/* Lower T - 512KB */
	SNOR_WP_BP_CMP_LO(SR_BP2 |          SR_BP0, 4),		/* Lower T - 1MB */
);

static const struct spi_nor_wp_info en25q16a_wpr = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_NONE(     0                                  ),	/* None */
	SNOR_WP_NONE(     SR_TB                              ),	/* None */

	SNOR_WP_ALL(              SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(              SR_BP2 | SR_BP1            ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 | SR_BP1            ),	/* All */

	SNOR_WP_BP_CMP_LO(                          SR_BP0, 0),		/* Lower T - 64KB */
	SNOR_WP_BP_CMP_LO(                 SR_BP1         , 1),		/* Lower T - 128KB */
	SNOR_WP_BP_CMP_LO(                 SR_BP1 | SR_BP0, 2),		/* Lower T - 256KB */
	SNOR_WP_BP_CMP_LO(        SR_BP2                  , 3),		/* Lower T - 512KB */
	SNOR_WP_BP_CMP_LO(        SR_BP2 |          SR_BP0, 4),		/* Lower T - 1MB */

	SNOR_WP_BP_CMP_UP(SR_TB |                   SR_BP0, 0),		/* Upper T - 64KB */
	SNOR_WP_BP_CMP_UP(SR_TB |          SR_BP1         , 1),		/* Upper T - 128KB */
	SNOR_WP_BP_CMP_UP(SR_TB |          SR_BP1 | SR_BP0, 2),		/* Upper T - 256KB */
	SNOR_WP_BP_CMP_UP(SR_TB | SR_BP2                  , 3),		/* Upper T - 512KB */
	SNOR_WP_BP_CMP_UP(SR_TB | SR_BP2 |          SR_BP0, 4),		/* Upper T - 1MB */
);

static const struct spi_nor_wp_info en25q32b_wpr = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_NONE(     0                                  ),	/* None */
	SNOR_WP_NONE(     SR_TB                              ),	/* None */

	SNOR_WP_ALL(              SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_CMP_LO(                          SR_BP0, 0),		/* Lower T - 64KB */
	SNOR_WP_BP_CMP_LO(                 SR_BP1         , 1),		/* Lower T - 128KB */
	SNOR_WP_BP_CMP_LO(                 SR_BP1 | SR_BP0, 2),		/* Lower T - 256KB */
	SNOR_WP_BP_CMP_LO(        SR_BP2                  , 3),		/* Lower T - 512KB */
	SNOR_WP_BP_CMP_LO(        SR_BP2 |          SR_BP0, 4),		/* Lower T - 1MB */
	SNOR_WP_BP_CMP_LO(        SR_BP2 | SR_BP1         , 5),		/* Lower T - 2MB */

	SNOR_WP_BP_CMP_UP(SR_TB |                   SR_BP0, 0),		/* Upper T - 64KB */
	SNOR_WP_BP_CMP_UP(SR_TB |          SR_BP1         , 1),		/* Upper T - 128KB */
	SNOR_WP_BP_CMP_UP(SR_TB |          SR_BP1 | SR_BP0, 2),		/* Upper T - 256KB */
	SNOR_WP_BP_CMP_UP(SR_TB | SR_BP2                  , 3),		/* Upper T - 512KB */
	SNOR_WP_BP_CMP_UP(SR_TB | SR_BP2 |          SR_BP0, 4),		/* Upper T - 1MB */
	SNOR_WP_BP_CMP_UP(SR_TB | SR_BP2 | SR_BP1         , 5),		/* Upper T - 2MB */
);

static const struct spi_nor_wp_info en25qa32b_wpr_4bp_tbl0 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_NONE(     0                                   ),	/* None */

	SNOR_WP_ALL(      SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP3 | SR_BP2                     ),	/* All */
	SNOR_WP_ALL(      SR_BP3 | SR_BP2 |          SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP3 | SR_BP2 | SR_BP1            ),	/* All */

	SNOR_WP_RP_UP(                               SR_BP0, 6),	/* Upper 1/64 */
	SNOR_WP_RP_UP(                      SR_BP1         , 5),	/* Upper 1/32 */
	SNOR_WP_RP_UP(                      SR_BP1 | SR_BP0, 4),	/* Upper 1/16 */
	SNOR_WP_RP_UP(             SR_BP2                  , 3),	/* Upper 1/8 */
	SNOR_WP_RP_UP(             SR_BP2 |          SR_BP0, 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(             SR_BP2 | SR_BP1         , 1),	/* Upper 1/2 */
	SNOR_WP_RP_CMP_UP(         SR_BP2 | SR_BP1 | SR_BP0, 2),	/* Upper T - 1/4 */
	SNOR_WP_RP_CMP_UP(SR_BP3                           , 3),	/* Upper T - 1/8 */
	SNOR_WP_RP_CMP_UP(SR_BP3 |                   SR_BP0, 4),	/* Upper T - 1/16 */
	SNOR_WP_RP_CMP_UP(SR_BP3 |          SR_BP1         , 5),	/* Upper T - 1/32 */
	SNOR_WP_RP_CMP_UP(SR_BP3 |          SR_BP1 | SR_BP0, 6),	/* Upper T - 1/64 */
);

static const struct spi_nor_wp_info en25qa32b_wpr_4bp_tbl1 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_NONE(     0                                   ),	/* None */

	SNOR_WP_ALL(      SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP3 | SR_BP2                     ),	/* All */
	SNOR_WP_ALL(      SR_BP3 | SR_BP2 |          SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP3 | SR_BP2 | SR_BP1            ),	/* All */

	SNOR_WP_RP_LO(                               SR_BP0, 6),	/* Lower 1/64 */
	SNOR_WP_RP_LO(                      SR_BP1         , 5),	/* Lower 1/32 */
	SNOR_WP_RP_LO(                      SR_BP1 | SR_BP0, 4),	/* Lower 1/16 */
	SNOR_WP_RP_LO(             SR_BP2                  , 3),	/* Lower 1/8 */
	SNOR_WP_RP_LO(             SR_BP2 |          SR_BP0, 2),	/* Lower 1/4 */
	SNOR_WP_RP_LO(             SR_BP2 | SR_BP1         , 1),	/* Lower 1/2 */
	SNOR_WP_RP_CMP_LO(         SR_BP2 | SR_BP1 | SR_BP0, 2),	/* Lower T - 1/4 */
	SNOR_WP_RP_CMP_LO(SR_BP3                           , 3),	/* Lower T - 1/8 */
	SNOR_WP_RP_CMP_LO(SR_BP3 |                   SR_BP0, 4),	/* Lower T - 1/16 */
	SNOR_WP_RP_CMP_LO(SR_BP3 |          SR_BP1         , 5),	/* Lower T - 1/32 */
	SNOR_WP_RP_CMP_LO(SR_BP3 |          SR_BP1 | SR_BP0, 6),	/* Lower T - 1/64 */
);

static const struct spi_nor_wp_info en25s64_wpr = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_NONE(     0                                  ),	/* None */
	SNOR_WP_NONE(     SR_TB                              ),	/* None */

	SNOR_WP_ALL(              SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_RP_CMP_LO(                          SR_BP0, 6),		/* Lower T - 1/64 */
	SNOR_WP_RP_CMP_LO(                 SR_BP1         , 5),		/* Lower T - 1/32 */
	SNOR_WP_RP_CMP_LO(                 SR_BP1 | SR_BP0, 4),		/* Lower T - 1/16 */
	SNOR_WP_RP_CMP_LO(        SR_BP2                  , 3),		/* Lower T - 1/8 */
	SNOR_WP_RP_CMP_LO(        SR_BP2 |          SR_BP0, 2),		/* Lower T - 1/4 */
	SNOR_WP_RP_CMP_LO(        SR_BP2 | SR_BP1         , 1),		/* Lower T - 1/2 */

	SNOR_WP_RP_UP(    SR_TB |                   SR_BP0, 6),		/* Upper 1/64 */
	SNOR_WP_RP_UP(    SR_TB |          SR_BP1         , 5),		/* Upper 1/32 */
	SNOR_WP_RP_UP(    SR_TB |          SR_BP1 | SR_BP0, 4),		/* Upper 1/16 */
	SNOR_WP_RP_UP(    SR_TB | SR_BP2                  , 3),		/* Upper 1/8 */
	SNOR_WP_RP_UP(    SR_TB | SR_BP2 |          SR_BP0, 2),		/* Upper 1/4 */
	SNOR_WP_RP_UP(    SR_TB | SR_BP2 | SR_BP1         , 1),		/* Upper 1/2 */
);

static const struct spi_nor_wp_info en25qa64a_wpr_4bp_tbl0 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_NONE(     0                                   ),	/* None */

	SNOR_WP_ALL(      SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP3 | SR_BP2 | SR_BP1            ),	/* All */

	SNOR_WP_RP_UP(                               SR_BP0, 7),	/* Upper 1/128 */
	SNOR_WP_RP_UP(                      SR_BP1         , 6),	/* Upper 1/64 */
	SNOR_WP_RP_UP(                      SR_BP1 | SR_BP0, 5),	/* Upper 1/32 */
	SNOR_WP_RP_UP(             SR_BP2                  , 4),	/* Upper 1/16 */
	SNOR_WP_RP_UP(             SR_BP2 |          SR_BP0, 3),	/* Upper 1/8 */
	SNOR_WP_RP_UP(             SR_BP2 | SR_BP1         , 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(             SR_BP2 | SR_BP1 | SR_BP0, 1),	/* Upper 1/2 */
	SNOR_WP_RP_CMP_UP(SR_BP3                           , 2),	/* Upper T - 1/4 */
	SNOR_WP_RP_CMP_UP(SR_BP3 |                   SR_BP0, 3),	/* Upper T - 1/8 */
	SNOR_WP_RP_CMP_UP(SR_BP3 |          SR_BP1         , 4),	/* Upper T - 1/16 */
	SNOR_WP_RP_CMP_UP(SR_BP3 |          SR_BP1 | SR_BP0, 5),	/* Upper T - 1/32 */
	SNOR_WP_RP_CMP_UP(SR_BP3 | SR_BP2                  , 6),	/* Upper T - 1/64 */
	SNOR_WP_RP_CMP_UP(SR_BP3 | SR_BP2 |          SR_BP0, 7),	/* Upper T - 1/128 */
);

static const struct spi_nor_wp_info en25qa64a_wpr_4bp_tbl1 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_NONE(     0                                   ),	/* None */

	SNOR_WP_ALL(      SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP3 | SR_BP2 | SR_BP1            ),	/* All */

	SNOR_WP_RP_LO(                               SR_BP0, 7),	/* Lower 1/128 */
	SNOR_WP_RP_LO(                      SR_BP1         , 6),	/* Lower 1/64 */
	SNOR_WP_RP_LO(                      SR_BP1 | SR_BP0, 5),	/* Lower 1/32 */
	SNOR_WP_RP_LO(             SR_BP2                  , 4),	/* Lower 1/16 */
	SNOR_WP_RP_LO(             SR_BP2 |          SR_BP0, 3),	/* Lower 1/8 */
	SNOR_WP_RP_LO(             SR_BP2 | SR_BP1         , 2),	/* Lower 1/4 */
	SNOR_WP_RP_LO(             SR_BP2 | SR_BP1 | SR_BP0, 1),	/* Lower 1/2 */
	SNOR_WP_RP_CMP_LO(SR_BP3                           , 2),	/* Lower T - 1/4 */
	SNOR_WP_RP_CMP_LO(SR_BP3 |                   SR_BP0, 3),	/* Lower T - 1/8 */
	SNOR_WP_RP_CMP_LO(SR_BP3 |          SR_BP1         , 4),	/* Lower T - 1/16 */
	SNOR_WP_RP_CMP_LO(SR_BP3 |          SR_BP1 | SR_BP0, 5),	/* Lower T - 1/32 */
	SNOR_WP_RP_CMP_LO(SR_BP3 | SR_BP2                  , 6),	/* Lower T - 1/64 */
	SNOR_WP_RP_CMP_LO(SR_BP3 | SR_BP2 |          SR_BP0, 7),	/* Lower T - 1/128 */
);

static const struct spi_nor_wp_info wpr_3bp_tb_cmp_only_ratio = SNOR_WP_BP(&sr_acc, BP_2_0_TB,
	SNOR_WP_NONE(     0                                  ),	/* None */
	SNOR_WP_NONE(     SR_TB                              ),	/* None */

	SNOR_WP_ALL(              SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_RP_CMP_UP(                          SR_BP0, 6),		/* Upper T - 1/64 */
	SNOR_WP_RP_CMP_UP(                 SR_BP1         , 5),		/* Upper T - 1/32 */
	SNOR_WP_RP_CMP_UP(                 SR_BP1 | SR_BP0, 4),		/* Upper T - 1/16 */
	SNOR_WP_RP_CMP_UP(        SR_BP2                  , 3),		/* Upper T - 1/8 */
	SNOR_WP_RP_CMP_UP(        SR_BP2 |          SR_BP0, 2),		/* Upper T - 1/4 */
	SNOR_WP_RP_CMP_UP(        SR_BP2 | SR_BP1         , 1),		/* Upper T - 1/2 */

	SNOR_WP_RP_CMP_LO(SR_TB |                   SR_BP0, 6),		/* Lower T - 1/64 */
	SNOR_WP_RP_CMP_LO(SR_TB |          SR_BP1         , 5),		/* Lower T - 1/32 */
	SNOR_WP_RP_CMP_LO(SR_TB |          SR_BP1 | SR_BP0, 4),		/* Lower T - 1/16 */
	SNOR_WP_RP_CMP_LO(SR_TB | SR_BP2                  , 3),		/* Lower T - 1/8 */
	SNOR_WP_RP_CMP_LO(SR_TB | SR_BP2 |          SR_BP0, 2),		/* Lower T - 1/4 */
	SNOR_WP_RP_CMP_LO(SR_TB | SR_BP2 | SR_BP1         , 1),		/* Lower T - 1/2 */
);

static const struct spi_nor_wp_info wpr_3bp_tb_sec_cmp_only = SNOR_WP_BP(&sr_acc, BP_2_0_TB_SEC,
	SNOR_WP_NONE(     0                                           ),	/* None */
	SNOR_WP_NONE(              SR_TB                              ),	/* None */
	SNOR_WP_NONE(     SR_SEC                                      ),	/* None */
	SNOR_WP_NONE(     SR_SEC | SR_TB                              ),	/* None */

	SNOR_WP_ALL(                       SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(               SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_SEC |         SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_SEC | SR_TB | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_CMP_UP(                                   SR_BP0, 0),	/* Upper T - 64KB */
	SNOR_WP_BP_CMP_UP(                          SR_BP1         , 1),	/* Upper T - 128KB */
	SNOR_WP_BP_CMP_UP(                          SR_BP1 | SR_BP0, 2),	/* Upper T - 256KB */
	SNOR_WP_BP_CMP_UP(                 SR_BP2                  , 3),	/* Upper T - 512KB */
	SNOR_WP_BP_CMP_UP(                 SR_BP2 |          SR_BP0, 4),	/* Upper T - 1MB */
	SNOR_WP_BP_CMP_UP(                 SR_BP2 | SR_BP1         , 5),	/* Upper T - 2MB */

	SNOR_WP_BP_CMP_LO(         SR_TB |                   SR_BP0, 0),	/* Lower T - 64KB */
	SNOR_WP_BP_CMP_LO(         SR_TB |          SR_BP1         , 1),	/* Lower T - 128KB */
	SNOR_WP_BP_CMP_LO(         SR_TB |          SR_BP1 | SR_BP0, 2),	/* Lower T - 256KB */
	SNOR_WP_BP_CMP_LO(         SR_TB | SR_BP2                  , 3),	/* Lower T - 512KB */
	SNOR_WP_BP_CMP_LO(         SR_TB | SR_BP2 |          SR_BP0, 4),	/* Lower T - 1MB */
	SNOR_WP_BP_CMP_LO(         SR_TB | SR_BP2 | SR_BP1         , 5),	/* Lower T - 2MB */

	SNOR_WP_SP_CMP_UP(SR_SEC |                           SR_BP0, 0),	/* Upper T - 4KB */
	SNOR_WP_SP_CMP_UP(SR_SEC |                  SR_BP1         , 1),	/* Upper T - 8KB */
	SNOR_WP_SP_CMP_UP(SR_SEC |                  SR_BP1 | SR_BP0, 2),	/* Upper T - 16KB */
	SNOR_WP_SP_CMP_UP(SR_SEC |         SR_BP2                  , 3),	/* Upper T - 32KB */
	SNOR_WP_SP_CMP_UP(SR_SEC |         SR_BP2 |          SR_BP0, 3),	/* Upper T - 32KB */
	SNOR_WP_SP_CMP_UP(SR_SEC |         SR_BP2 | SR_BP1         , 3),	/* Upper T - 32KB */

	SNOR_WP_SP_CMP_LO(SR_SEC | SR_TB |                   SR_BP0, 0),	/* Lower T - 4KB */
	SNOR_WP_SP_CMP_LO(SR_SEC | SR_TB |          SR_BP1         , 1),	/* Lower T - 8KB */
	SNOR_WP_SP_CMP_LO(SR_SEC | SR_TB |          SR_BP1 | SR_BP0, 2),	/* Lower T - 16KB */
	SNOR_WP_SP_CMP_LO(SR_SEC | SR_TB | SR_BP2                  , 3),	/* Lower T - 32KB */
	SNOR_WP_SP_CMP_LO(SR_SEC | SR_TB | SR_BP2 |          SR_BP0, 3),	/* Lower T - 32KB */
	SNOR_WP_SP_CMP_LO(SR_SEC | SR_TB | SR_BP2 | SR_BP1         , 3),	/* Lower T - 32KB */
);

static ufprog_status en25f10_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25F10");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25F10A");
}

static const struct spi_nor_flash_part_fixup en25f10_fixups = {
	.pre_param_setup = en25f10_fixup_model,
};

static ufprog_status en25s10_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25S10");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25S10A");
}

static const struct spi_nor_flash_part_fixup en25s10_fixups = {
	.pre_param_setup = en25s10_fixup_model,
};

static ufprog_status en25f20_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25F20");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25F20A");
}

static const struct spi_nor_flash_part_fixup en25f20_fixups = {
	.pre_param_setup = en25f20_fixup_model,
};

static ufprog_status en25s20_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25S20");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25S20A");
}

static const struct spi_nor_flash_part_fixup en25s20_fixups = {
	.pre_param_setup = en25s20_fixup_model,
};

static ufprog_status en25f40_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25F40");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25F40A");
}

static const struct spi_nor_flash_part_fixup en25f40_fixups = {
	.pre_param_setup = en25f40_fixup_model,
};

static ufprog_status en25s40_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25S40");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25S40A");
}

static const struct spi_nor_flash_part_fixup en25s40_fixups = {
	.pre_param_setup = en25s40_fixup_model,
};

static ufprog_status en25q40_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	uint32_t dw;

	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25Q40");

	dw = sfdp_dw(snor->sfdp.bfpt, 3);
	if (FIELD_GET(BFPT_DW3_1S_1S_4S_FAST_READ_OPCODE, dw) == SNOR_CMD_FAST_READ_QUAD_OUT)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25Q40B");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25Q40A");
}

static const struct spi_nor_flash_part_fixup en25q40_fixups = {
	.pre_param_setup = en25q40_fixup_model,
};

static ufprog_status en25s32_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25S32");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25S32A");
}

static const struct spi_nor_flash_part_fixup en25s32_fixups = {
	.pre_param_setup = en25s32_fixup_model,
};

static ufprog_status en25q32_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25Q32B");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25Q32C");
}

static const struct spi_nor_flash_part_fixup en25q32_fixups = {
	.pre_param_setup = en25q32_fixup_model,
};

static ufprog_status en25qa32b_wpr_4bp_tbl_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						  struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &eon_otp_sr_acc, &regval));

	if (regval & BIT(3))
		bp->p.wp_ranges = &en25qa32b_wpr_4bp_tbl1;
	else
		bp->p.wp_ranges = &en25qa32b_wpr_4bp_tbl0;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup en25qa32b_wpr_4bp_tbl_fixups = {
	.pre_param_setup = en25qa32b_wpr_4bp_tbl_select,
};

static ufprog_status en25qa32_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	uint32_t dw;

	if (!snor->sfdp.bfpt)
		return UFP_OK;

	dw = sfdp_dw(snor->sfdp.bfpt, 3);
	if (FIELD_GET(BFPT_DW3_1S_1S_4S_FAST_READ_OPCODE, dw) == SNOR_CMD_FAST_READ_QUAD_OUT)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QA32B");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QA32A");
}

static const struct spi_nor_flash_part_fixup en25qa32_fixups = {
	.pre_param_setup = en25qa32_fixup_model,
};

static ufprog_status en25qa64a_wpr_4bp_tbl_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						  struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &eon_otp_sr_acc, &regval));

	if (regval & BIT(3))
		bp->p.wp_ranges = &en25qa64a_wpr_4bp_tbl1;
	else
		bp->p.wp_ranges = &en25qa64a_wpr_4bp_tbl0;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup en25qa64a_wpr_4bp_tbl_fixups = {
	.pre_param_setup = en25qa64a_wpr_4bp_tbl_select,
};

static ufprog_status en25s64_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25S64");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25S64A");
}

static const struct spi_nor_flash_part_fixup en25s64_fixups = {
	.pre_param_setup = en25s64_fixup_model,
};

static ufprog_status en25qa128a_wpr_4bp_tbl_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						   struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &eon_otp_sr_acc, &regval));

	if (regval & BIT(3))
		bp->p.wp_ranges = &wpr_3bp_tb_cmp_only_ratio;
	else
		bp->p.wp_ranges = &wpr_3bp_tb_ratio;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup en25qa128a_wpr_4bp_tbl_fixups = {
	.pre_param_setup = en25qa128a_wpr_4bp_tbl_select,
};

static ufprog_status en25qh16b_wpr_4bp_cmp_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						  struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &eon_otp_sr_acc, &regval));

	if (regval & BIT(4))
		bp->p.wp_ranges = &wpr_3bp_tb_sec_cmp_only;
	else
		bp->p.wp_ranges = &wpr_3bp_tb_sec;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup en25qh16b_wpr_4bp_cmp_fixups = {
	.pre_param_setup = en25qh16b_wpr_4bp_cmp_select,
};

static ufprog_status en25qh16_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	uint32_t dw;

	if (!snor->sfdp.bfpt)
		return UFP_OK;

	dw = sfdp_dw(snor->sfdp.bfpt, 3);
	if (FIELD_GET(BFPT_DW3_1S_1S_4S_FAST_READ_OPCODE, dw) == SNOR_CMD_FAST_READ_QUAD_OUT)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QH16B");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QH16A");
}

static const struct spi_nor_flash_part_fixup en25qh16_fixups = {
	.pre_param_setup = en25qh16_fixup_model,
};

static ufprog_status en25qh32_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	uint32_t dw;

	if (!snor->sfdp.bfpt)
		return UFP_OK;

	dw = sfdp_dw(snor->sfdp.bfpt, 3);
	if (FIELD_GET(BFPT_DW3_1S_1S_4S_FAST_READ_OPCODE, dw) == SNOR_CMD_FAST_READ_QUAD_OUT)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QH32B");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QH32A");
}

static const struct spi_nor_flash_part_fixup en25qh32_fixups = {
	.pre_param_setup = en25qh32_fixup_model,
};

static ufprog_status en25qh64_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	uint32_t dw;

	if (!snor->sfdp.bfpt)
		return UFP_OK;

	dw = sfdp_dw(snor->sfdp.bfpt, 3);
	if (FIELD_GET(BFPT_DW3_1S_1S_4S_FAST_READ_OPCODE, dw) == SNOR_CMD_FAST_READ_QUAD_OUT)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QH64A");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QH64");
}

static const struct spi_nor_flash_part_fixup en25qh64_fixups = {
	.pre_param_setup = en25qh64_fixup_model,
};

static ufprog_status en25qh128_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	uint32_t dw;

	if (!snor->sfdp.bfpt)
		return UFP_OK;

	dw = sfdp_dw(snor->sfdp.bfpt, 3);
	if (FIELD_GET(BFPT_DW3_1S_1S_4S_FAST_READ_OPCODE, dw) == SNOR_CMD_FAST_READ_QUAD_OUT)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QH128A");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QH128");
}

static const struct spi_nor_flash_part_fixup en25qh128_fixups = {
	.pre_param_setup = en25qh128_fixup_model,
};

static ufprog_status en25qh256_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return UFP_OK;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QH256A");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QH256");
}

static const struct spi_nor_flash_part_fixup en25qh256_fixups = {
	.pre_param_setup = en25qh256_fixup_model,
};

static struct spi_nor_wp_info *eon_3bp_tb_sec_cmp, eon_3bp_tb_sec_cmp_dummy;
static struct spi_nor_wp_info *eon_4bp_tb_cmp, eon_4bp_tb_cmp_dummy;

static const struct spi_nor_erase_info en25p05_erase_opcodes = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_32K, SNOR_CMD_BLOCK_ERASE),
);

static const struct spi_nor_flash_part eon_parts[] = {
	SNOR_PART("EN25P05", SNOR_ID(0x1c, 0x20, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&en25p05_erase_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25p05_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("EN25F05", SNOR_ID(0x1c, 0x31, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&en25f05_wpr),
		  SNOR_OTP_INFO(&eon_otp_256b),
	),

	SNOR_PART("EN25P10", SNOR_ID(0x1c, 0x20, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&en25p05_erase_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25p05_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up_ratio),
	),

	SNOR_PART("EN25F10(meta)", SNOR_ID(0x1c, 0x31, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_FIXUPS(&en25f10_fixups),
	),

	SNOR_PART("EN25F10", SNOR_ID(0x1c, 0x31, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&en25f05_wpr),
		  SNOR_OTP_INFO(&eon_otp_256b),
	),

	SNOR_PART("EN25F10A", SNOR_ID(0x1c, 0x31, 0x11), SZ_128K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_3),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(86),
		  SNOR_REGS(&en25fxa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
	),

	SNOR_PART("EN25S10(meta)", SNOR_ID(0x1c, 0x38, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_FIXUPS(&en25s10_fixups),
	),

	SNOR_PART("EN25S10", SNOR_ID(0x1c, 0x38, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&en25s10_wpr),
		  SNOR_OTP_INFO(&eon_otp_256b),
	),

	SNOR_PART("EN25S10A", SNOR_ID(0x1c, 0x38, 0x11), SZ_128K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25E10", SNOR_ID(0x1c, 0x42, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(86),
		  SNOR_REGS(&en25q_regs),
		  SNOR_WP_RANGES(&en25e10_wpr),
	),

	SNOR_PART("EN25P20", SNOR_ID(0x1c, 0x20, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25p05_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up_ratio),
	),

	SNOR_PART("EN25F20(meta)", SNOR_ID(0x1c, 0x31, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_FIXUPS(&en25f20_fixups),
	),

	SNOR_PART("EN25F20", SNOR_ID(0x1c, 0x31, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&en25f05_wpr),
		  SNOR_OTP_INFO(&eon_otp_256b),
	),

	SNOR_PART("EN25F20A", SNOR_ID(0x1c, 0x31, 0x12), SZ_256K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_3),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(86),
		  SNOR_REGS(&en25fxa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
	),

	SNOR_PART("EN25S20(meta)", SNOR_ID(0x1c, 0x38, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_FIXUPS(&en25s20_fixups),
	),

	SNOR_PART("EN25S20", SNOR_ID(0x1c, 0x38, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&en25s10_wpr),
		  SNOR_OTP_INFO(&eon_otp_256b),
	),

	SNOR_PART("EN25S20A", SNOR_ID(0x1c, 0x38, 0x12), SZ_256K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&en25s20a_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25P40", SNOR_ID(0x1c, 0x20, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("EN25Q40(meta)", SNOR_ID(0x1c, 0x30, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_FIXUPS(&en25q40_fixups),
	),

	SNOR_PART("EN25Q40", SNOR_ID(0x1c, 0x30, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(100), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25q_regs),
		  SNOR_WP_RANGES(&en25q40_wpr),
		  SNOR_OTP_INFO(&eon_otp_256b),
	),

	SNOR_PART("EN25Q40A", SNOR_ID(0x1c, 0x30, 0x13), SZ_512K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&en25f40a_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25Q40B", SNOR_ID(0x1c, 0x30, 0x13), SZ_512K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(86),
		  SNOR_REGS(&en25qxb_regs),
		  SNOR_WP_RANGES(&eon_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
	),

	SNOR_PART("EN25F40(meta)", SNOR_ID(0x1c, 0x31, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_FIXUPS(&en25f40_fixups),
	),

	SNOR_PART("EN25F40", SNOR_ID(0x1c, 0x31, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
		  SNOR_OTP_INFO(&eon_otp_256b),
	),

	SNOR_PART("EN25F40A", SNOR_ID(0x1c, 0x31, 0x13), SZ_512K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&en25f40a_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25S40(meta)", SNOR_ID(0x1c, 0x38, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_FIXUPS(&en25s40_fixups),
	),

	SNOR_PART("EN25S40", SNOR_ID(0x1c, 0x38, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&en25s10_wpr),
		  SNOR_OTP_INFO(&eon_otp_256b),
	),

	SNOR_PART("EN25S40A", SNOR_ID(0x1c, 0x38, 0x13), SZ_512K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&en25f40a_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25E40", SNOR_ID(0x1c, 0x42, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(86),
		  SNOR_REGS(&en25q_regs),
		  SNOR_WP_RANGES(&en25q40_wpr),
	),

	SNOR_PART("EN25P80", SNOR_ID(0x1c, 0x20, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("EN25Q80", SNOR_ID(0x1c, 0x30, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
	),

	SNOR_PART("EN25Q80A", SNOR_ID(0x1c, 0x30, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(100), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25q_regs),
		  SNOR_WP_RANGES(&en25q40_wpr),
		  SNOR_OTP_INFO(&eon_otp_256b),
	),

	SNOR_PART("EN25Q80B", SNOR_ID(0x1c, 0x30, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&en25q80b_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25Q80C", SNOR_ID(0x1c, 0x30, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_3),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(86),
		  SNOR_REGS(&en25qxb_regs),
		  SNOR_WP_RANGES(&eon_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
	),

	SNOR_PART("EN25F80", SNOR_ID(0x1c, 0x31, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&en25q40_wpr),
		  SNOR_OTP_INFO(&eon_otp_256b),
	),

	SNOR_PART("EN25S80(meta)", SNOR_ID(0x1c, 0x38, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75), SNOR_DUAL_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("EN25S80", SNOR_ID(0x1c, 0x38, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
		  SNOR_OTP_INFO(&eon_otp_256b),
	),

	SNOR_PART("EN25S80A", SNOR_ID(0x1c, 0x38, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25s40a_regs),
		  SNOR_WP_RANGES(&en25s40a_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25S80B", SNOR_ID(0x1c, 0x38, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_4 | EON_F_DC_SR3_BIT5_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qhxb_regs),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
		  SNOR_FIXUPS(&en25qh16b_wpr_4bp_cmp_fixups),
	),

	SNOR_PART("EN25P16", SNOR_ID(0x1c, 0x20, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("EN25Q16(meta)", SNOR_ID(0x1c, 0x30, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_QE_DONT_CARE,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
	),

	SNOR_PART("EN25Q16", SNOR_ID(0x1c, 0x30, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&en25q16_wpr),
		  SNOR_OTP_INFO(&eon_otp_128b),
	),

	SNOR_PART("EN25Q16A", SNOR_ID(0x1c, 0x30, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&en25q16a_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25F16", SNOR_ID(0x1c, 0x31, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25S16(meta)", SNOR_ID(0x1c, 0x38, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
	),

	SNOR_PART("EN25S16", SNOR_ID(0x1c, 0x38, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&en25q16a_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25S16A", SNOR_ID(0x1c, 0x38, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25s40a_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25S16B", SNOR_ID(0x1c, 0x38, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_4 | EON_F_DC_SR3_BIT5_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qhxb_regs),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
		  SNOR_FIXUPS(&en25qh16b_wpr_4bp_cmp_fixups),
	),

	SNOR_PART("EN25QE16A", SNOR_ID(0x1c, 0x41, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_SECR | EON_F_READ_UID_4BH | EON_F_DC_SR3_BIT7),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&eon_otp_3x1k),
	),

	SNOR_PART("EN25SE16A", SNOR_ID(0x1c, 0x48, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_SECR | EON_F_READ_UID_4BH | EON_F_DC_SR3_BIT7),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&eon_otp_3x1k),
	),

	SNOR_PART("EN25QW16A", SNOR_ID(0x1c, 0x61, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_SECR | EON_F_READ_UID_4BH | EON_F_DC_SR3_BIT7),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&eon_otp_3x1k),
	),

	SNOR_PART("EN25QH16", SNOR_ID(0x1c, 0x70, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_FIXUPS(&en25qh16_fixups),
	),

	SNOR_PART("EN25QH16A", SNOR_ID(0x1c, 0x70, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25QH16B", SNOR_ID(0x1c, 0x70, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_4 | EON_F_DC_SR3_BIT5_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(86), SNOR_QUAD_MAX_SPEED_MHZ(86),
		  SNOR_REGS(&en25qhxb_regs),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
		  SNOR_FIXUPS(&en25qh16b_wpr_4bp_cmp_fixups),
	),

	SNOR_PART("EN25P32", SNOR_ID(0x1c, 0x20, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_2),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25Q32(meta)", SNOR_ID(0x1c, 0x30, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(50),
		  SNOR_WP_RANGES(&en25q32b_wpr),
		  SNOR_FIXUPS(&en25q32_fixups),
	),

	SNOR_PART("EN25Q32B", SNOR_ID(0x1c, 0x30, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&en25q32b_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25Q32C", SNOR_ID(0x1c, 0x30, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25fxa_regs),
		  SNOR_WP_RANGES(&en25q32b_wpr),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
	),

	SNOR_PART("EN25Q32", SNOR_ID(0x1c, 0x33, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(100), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25S32(meta)", SNOR_ID(0x1c, 0x38, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_FIXUPS(&en25s32_fixups),
	),

	SNOR_PART("EN25S32", SNOR_ID(0x1c, 0x38, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&en25q32b_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25S32A", SNOR_ID(0x1c, 0x38, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_4 | EON_F_DC_SR3_BIT5_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25s32a_regs),
		  SNOR_WP_RANGES(&eon_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
	),

	SNOR_PART("EN25QE32A", SNOR_ID(0x1c, 0x41, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_SECR | EON_F_READ_UID_4BH | EON_F_DC_SR3_BIT7),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&eon_otp_3x1k),
	),

	SNOR_PART("EN25SE32A", SNOR_ID(0x1c, 0x48, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_SECR | EON_F_READ_UID_4BH | EON_F_DC_SR3_BIT7),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&eon_otp_3x1k),
	),

	SNOR_PART("EN25QW32A", SNOR_ID(0x1c, 0x61, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_SECR | EON_F_READ_UID_4BH | EON_F_DC_SR3_BIT7),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&eon_otp_3x1k),
	),

	SNOR_PART("EN25QA32", SNOR_ID(0x1c, 0x60, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_FIXUPS(&en25qa32_fixups),
	),

	SNOR_PART("EN25QA32A", SNOR_ID(0x1c, 0x60, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qaxa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25QA32B", SNOR_ID(0x1c, 0x60, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qaxb_regs),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
		  SNOR_FIXUPS(&en25qa32b_wpr_4bp_tbl_fixups),
	),

	SNOR_PART("EN25QH32", SNOR_ID(0x1c, 0x70, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_FIXUPS(&en25qh32_fixups),
	),

	SNOR_PART("EN25QH32A", SNOR_ID(0x1c, 0x70, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qh32a_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25QH32B", SNOR_ID(0x1c, 0x70, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_4 | EON_F_DC_SR3_BIT5_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qh32b_regs),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
		  SNOR_FIXUPS(&en25qa32b_wpr_4bp_tbl_fixups),
	),

	SNOR_PART("EN25P64", SNOR_ID(0x1c, 0x20, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_2),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
		  SNOR_REGS(&en25f_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up_ratio),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25Q64", SNOR_ID(0x1c, 0x30, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&en25q32b_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25S64(meta)", SNOR_ID(0x1c, 0x38, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&eon_otp_512b),
		  SNOR_FIXUPS(&en25s64_fixups),
	),

	SNOR_PART("EN25S64", SNOR_ID(0x1c, 0x38, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&en25s64_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25S64A", SNOR_ID(0x1c, 0x38, 0x17), SZ_8M, /* SFDP 1.0, INFO_REG */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1 | EON_F_DC_SR3_BIT5_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qh64a_regs),
		  SNOR_OTP_INFO(&eon_otp_512b),
		  SNOR_FIXUPS(&en25qa64a_wpr_4bp_tbl_fixups),
	),

	SNOR_PART("EN25QA64A", SNOR_ID(0x1c, 0x60, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1 | EON_F_DC_SR3_BIT5_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qa64a_regs),
		  SNOR_OTP_INFO(&eon_otp_512b),
		  SNOR_FIXUPS(&en25qa64a_wpr_4bp_tbl_fixups),
	),

	SNOR_PART("EN25QH64(meta)", SNOR_ID(0x1c, 0x70, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(50),
		  SNOR_OTP_INFO(&eon_otp_512b),
		  SNOR_FIXUPS(&en25qh64_fixups),
	),

	SNOR_PART("EN25QH64", SNOR_ID(0x1c, 0x70, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25QH64A", SNOR_ID(0x1c, 0x70, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1 | EON_F_DC_SR3_BIT5_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qh64a_regs),
		  SNOR_OTP_INFO(&eon_otp_512b),
		  SNOR_FIXUPS(&en25qa64a_wpr_4bp_tbl_fixups),
	),

	SNOR_PART("EN25QX64A", SNOR_ID(0x1c, 0x71, 0x17), SZ_8M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_SECR | EON_F_READ_UID_SFDP_1E0H | EON_F_DC_SR3_BIT7),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qx_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
	),

	SNOR_PART("EN25Q128", SNOR_ID(0x1c, 0x30, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&en25q32b_wpr),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25QA128A", SNOR_ID(0x1c, 0x60, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1 | EON_F_DC_SR3_BIT5_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qa64a_regs),
		  SNOR_OTP_INFO(&eon_otp_512b),
		  SNOR_FIXUPS(&en25qa128a_wpr_4bp_tbl_fixups),
	),

	SNOR_PART("EN25QH128(meta)", SNOR_ID(0x1c, 0x70, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(50),
		  SNOR_OTP_INFO(&eon_otp_512b),
		  SNOR_FIXUPS(&en25qh128_fixups),
	),

	SNOR_PART("EN25QH128", SNOR_ID(0x1c, 0x70, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25QH128A", SNOR_ID(0x1c, 0x70, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1 | EON_F_DC_SR3_BIT5_4),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qh64a_regs),
		  SNOR_OTP_INFO(&eon_otp_512b),
		  SNOR_FIXUPS(&en25qa128a_wpr_4bp_tbl_fixups),
	),

	SNOR_PART("EN25QX128A", SNOR_ID(0x1c, 0x71, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_SECR | EON_F_DC_SR3_BIT7),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qx_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
	),

	SNOR_PART("EN25SX128A", SNOR_ID(0x1c, 0x78, 0x17), SZ_8M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_SECR | EON_F_DC_SR3_BIT7),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qx_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
	),

	SNOR_PART("EN25QH256(meta)", SNOR_ID(0x1c, 0x70, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_VENDOR_FLAGS(EON_F_HIGH_BANK_LATCH),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_FIXUPS(&en25qh256_fixups),
	),

	SNOR_PART("EN25QH256", SNOR_ID(0x1c, 0x70, 0x19), SZ_32M, /* SFDP 1.0, INFO_REG */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_1 | EON_F_HIGH_BANK_LATCH),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25f40a_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&eon_otp_512b),
	),

	SNOR_PART("EN25QH256A", SNOR_ID(0x1c, 0x70, 0x19), SZ_32M, /* SFDP 1.6, INFO_REG */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_4 | EON_F_DC_SR3_BIT5_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qh256a_regs),
		  SNOR_WP_RANGES(&eon_4bp_tb_cmp_dummy),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
	),

	SNOR_PART("EN25QX256A", SNOR_ID(0x1c, 0x71, 0x19), SZ_32M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_SECR | EON_F_READ_UID_SFDP_1E0H | EON_F_DC_SR3_BIT7),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qx256a_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
	),

	SNOR_PART("EN35QX512A", SNOR_ID(0x1c, 0x71, 0x20), SZ_64M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(EON_F_OTP_TYPE_SECR | EON_F_READ_UID_SFDP_1E0H | EON_F_DC_SR3_BIT7),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&en25qx256a_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
		  SNOR_OTP_INFO(&eon_otp_3x512b),
	),
};

static ufprog_status eon_enter_otp_mode(struct spi_nor *snor)
{
	return spi_nor_issue_single_opcode(snor, SNOR_CMD_EON_ENTER_OTP_MODE);
}

static ufprog_status eon_exit_otp_mode(struct spi_nor *snor)
{
	return spi_nor_write_disable(snor);
}

static ufprog_status eon_otp_sr_pre_acc(struct spi_nor *snor, const struct spi_nor_reg_access *access)
{
	return eon_enter_otp_mode(snor);
}

static ufprog_status eon_otp_sr_post_acc(struct spi_nor *snor, const struct spi_nor_reg_access *access)
{
	return eon_exit_otp_mode(snor);
}

static ufprog_status eon_otp_read_cust(struct spi_nor *snor, uint32_t addr, uint32_t len, void *data)
{
	ufprog_status ret;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, spi_mem_io_info_cmd_bw(snor->state.read_io_info)));
	STATUS_CHECK_RET(eon_enter_otp_mode(snor));

	ret = scur_otp_read_raw(snor, addr, len, data);

	STATUS_CHECK_RET(eon_exit_otp_mode(snor));

	return ret;
}

static ufprog_status eon_otp_write_cust(struct spi_nor *snor, uint32_t addr, uint32_t len, const void *data)
{
	ufprog_status ret;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, spi_mem_io_info_cmd_bw(snor->state.pp_io_info)));
	STATUS_CHECK_RET(eon_enter_otp_mode(snor));

	ret = scur_otp_write_raw(snor, addr, len, data);

	STATUS_CHECK_RET(eon_exit_otp_mode(snor));

	return ret;
}

#if 0
static ufprog_status eon_otp_erase_cust(struct spi_nor *snor, uint32_t addr)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_SECTOR_ERASE, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_ADDR(snor->state.naddr, addr, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_NO_DATA
	);

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_setup_addr(snor, &op.addr.val));
	STATUS_CHECK_RET(spi_nor_write_enable(snor));
	STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));
	STATUS_CHECK_RET(spi_nor_wait_busy(snor, SNOR_ERASE_TIMEOUT_MS));

	return UFP_OK;
}
#endif

static ufprog_status eon_otp_1_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	return eon_otp_read_cust(snor, (uint32_t)snor->param.size - SZ_4K + addr, len, data);
}

static ufprog_status eon_otp_1_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len,
				     const void *data)
{
	return eon_otp_write_cust(snor, (uint32_t)snor->param.size - SZ_4K + addr, len, data);
}

#if 0
static ufprog_status eon_otp_1_erase(struct spi_nor *snor, uint32_t index)
{
	return eon_otp_erase_cust(snor, (uint32_t)snor->param.size - SZ_4K);
}
#endif

static ufprog_status eon_otp_1_lock(struct spi_nor *snor, uint32_t index)
{
	ufprog_status ret = UFP_OK;
	uint8_t reg = 0;

	STATUS_CHECK_RET(eon_enter_otp_mode(snor));

	STATUS_CHECK_GOTO_RET(spi_nor_write_sr(snor, 0, false), ret, out);
	STATUS_CHECK_GOTO_RET(spi_nor_read_sr(snor, &reg), ret, out);

out:
	STATUS_CHECK_RET(eon_exit_otp_mode(snor));

	if (reg & SR_OTP_LOCK)
		return UFP_OK;

	return UFP_FAIL;
}

static ufprog_status eon_otp_1_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{
	ufprog_status ret;
	uint8_t reg;

	STATUS_CHECK_RET(eon_enter_otp_mode(snor));
	ret = spi_nor_read_sr(snor, &reg);
	STATUS_CHECK_RET(eon_exit_otp_mode(snor));

	if (ret)
		return ret;

	if (reg & SR_OTP_LOCK)
		*retlocked = true;
	else
		*retlocked = false;

	return UFP_OK;
}

static const struct spi_nor_flash_part_otp_ops eon_otp_1_ops = {
	.read = eon_otp_1_read,
	.write = eon_otp_1_write,
#if 0
	/* Not working */
	.erase = eon_otp_1_erase,
#endif
	.lock = eon_otp_1_lock,
	.locked = eon_otp_1_locked,
};

static ufprog_status eon_otp_2_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	return eon_otp_read_cust(snor, (uint32_t)snor->param.size - snor->ext_param.otp->size + addr, len, data);
}

static ufprog_status eon_otp_2_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len,
				     const void *data)
{
	return eon_otp_write_cust(snor, (uint32_t)snor->param.size - snor->ext_param.otp->size + addr, len, data);
}

#if 0
static ufprog_status eon_otp_2_erase(struct spi_nor *snor, uint32_t index)
{
	return eon_otp_erase_cust(snor, (uint32_t)snor->param.size - snor->ext_param.otp->size);
}
#endif

static const struct spi_nor_flash_part_otp_ops eon_otp_2_ops = {
	.read = eon_otp_2_read,
	.write = eon_otp_2_write,
#if 0
	/* Not tested */
	.erase = eon_otp_2_erase,
#endif
	.lock = eon_otp_1_lock,
	.locked = eon_otp_1_locked,
};

static uint32_t eon_otp_3_addr(struct spi_nor *snor, uint32_t index)
{
	switch (index) {
	case 0:
		return (uint32_t)snor->param.size - SZ_4K;

	case 1:
		return (uint32_t)snor->param.size - SZ_8K;

	case 2:
		return (uint32_t)snor->param.size - SZ_64K;

	default:
		return 0;
	}
}

static uint32_t eon_otp_3_lock_bit(struct spi_nor *snor, uint32_t index)
{
	switch (index) {
	case 0:
		return 7;

	case 1:
		return 2;

	case 2:
		return 1;

	default:
		return 0;
	}
}

static ufprog_status eon_otp_3_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	return eon_otp_read_cust(snor, eon_otp_3_addr(snor, index) + addr, len, data);
}

static ufprog_status eon_otp_3_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len,
				     const void *data)
{
	return eon_otp_write_cust(snor, eon_otp_3_addr(snor, index) + addr, len, data);
}

#if 0
static ufprog_status eon_otp_3_erase(struct spi_nor *snor, uint32_t index)
{
	return eon_otp_erase_cust(snor, eon_otp_3_addr(snor, index));
}
#endif

static ufprog_status eon_otp_3_lock(struct spi_nor *snor, uint32_t index)
{
	ufprog_status ret = UFP_OK;
	uint32_t lock_bit;
	uint8_t reg = 0;

	lock_bit = eon_otp_3_lock_bit(snor, index);

	STATUS_CHECK_RET(eon_enter_otp_mode(snor));

	STATUS_CHECK_GOTO_RET(spi_nor_read_sr(snor, &reg), ret, out);
	reg |= BIT(lock_bit);
	STATUS_CHECK_GOTO_RET(spi_nor_write_sr(snor, 0, false), ret, out);

	STATUS_CHECK_GOTO_RET(spi_nor_read_sr(snor, &reg), ret, out);

out:
	STATUS_CHECK_RET(eon_exit_otp_mode(snor));

	if (reg & BIT(lock_bit))
		return UFP_OK;

	return UFP_FAIL;
}

static ufprog_status eon_otp_3_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{

	ufprog_status ret = UFP_OK;
	uint32_t lock_bit;
	uint8_t reg = 0;

	lock_bit = eon_otp_3_lock_bit(snor, index);

	STATUS_CHECK_RET(eon_enter_otp_mode(snor));
	ret = spi_nor_read_sr(snor, &reg);
	STATUS_CHECK_RET(eon_exit_otp_mode(snor));

	if (ret)
		return ret;

	if (reg & BIT(lock_bit))
		*retlocked = true;
	else
		*retlocked = false;

	return UFP_OK;
}

static const struct spi_nor_flash_part_otp_ops eon_otp_3_ops = {
	.read = eon_otp_3_read,
	.write = eon_otp_3_write,
#if 0
	/* Not tested */
	.erase = eon_otp_3_erase,
#endif
	.lock = eon_otp_3_lock,
	.locked = eon_otp_3_locked,
};

static uint32_t eon_otp_4_addr(struct spi_nor *snor, uint32_t index)
{
	switch (index) {
	case 0:
		return (uint32_t)snor->param.size - SZ_4K;

	case 1:
		return (uint32_t)snor->param.size - SZ_8K;

	case 2:
		return (uint32_t)snor->param.size - 3 * SZ_4K;

	default:
		return 0;
	}
}

static ufprog_status eon_otp_4_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	return eon_otp_read_cust(snor, eon_otp_4_addr(snor, index) + addr, len, data);
}

static ufprog_status eon_otp_4_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len,
				     const void *data)
{
	return eon_otp_write_cust(snor, eon_otp_4_addr(snor, index) + addr, len, data);
}

#if 0
/* Not tested */
static ufprog_status eon_otp_4_erase(struct spi_nor *snor, uint32_t index)
{
	return eon_otp_erase_cust(snor, eon_otp_4_addr(snor, index));
}
#endif

static const struct spi_nor_flash_part_otp_ops eon_otp_4_ops = {
	.read = eon_otp_4_read,
	.write = eon_otp_4_write,
#if 0
	/* Not tested */
	.erase = eon_otp_4_erase,
#endif
	.lock = eon_otp_3_lock,
	.locked = eon_otp_3_locked,
};

static uint32_t eon_secr_otp_addr(struct spi_nor *snor, uint32_t index, uint32_t addr)
{
	uint32_t otp_addr = (uint32_t)snor->param.size - (index + 1) * SZ_4K + addr;
	uint8_t high_byte;

	if (snor->param.size <= SZ_16M || snor->state.a4b_mode)
		return otp_addr;

	high_byte = (otp_addr >> 24) & 0xff;
	otp_addr &= 0xffffff;

	if (snor->param.vendor_flags & EON_F_HIGH_BANK_LATCH) {
		if (high_byte)
			spi_nor_issue_single_opcode(snor, SNOR_CMD_EON_EN_HIGH_BANK_MODE);
		else
			spi_nor_issue_single_opcode(snor, SNOR_CMD_EON_EX_HIGH_BANK_MODE);
	} else {
		spi_nor_write_reg(snor, SNOR_CMD_WRITE_EAR, high_byte);
	}

	return otp_addr;
}

static ufprog_status eon_secr_otp_lock_bit(struct spi_nor *snor, uint32_t index, uint32_t *retbit,
					   const struct spi_nor_reg_access **retacc)
{
	switch (index) {
	case 0:
		*retbit = 5;
		break;

	case 1:
		*retbit = 4;
		break;

	case 2:
		*retbit = 3;
		break;

	default:
		*retbit = 0;
	}

	*(retacc) = &cr_acc;

	return UFP_OK;
}

static const struct spi_nor_flash_secr_otp_ops eon_secr_otp_ops = {
	.otp_addr = eon_secr_otp_addr,
	.otp_lock_bit = eon_secr_otp_lock_bit,
};

static const struct spi_nor_flash_part_otp_ops eon_otp_secr_ops = {
	.read = secr_otp_read_paged,
	.write = secr_otp_write_paged,
	.erase = secr_otp_erase,
	.lock = secr_otp_lock,
	.locked = secr_otp_locked,
	.secr = &eon_secr_otp_ops,
};

static ufprog_status eon_part_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				    struct spi_nor_flash_part_blank *bp)
{
	if (bp->p.size > SZ_16M) {
		/* Set to a known address mode (3-Byte) */
		STATUS_CHECK_RET(spi_nor_disable_4b_addressing_e9h(snor));
		snor->state.a4b_mode = false;

		if (snor->param.vendor_flags & EON_F_HIGH_BANK_LATCH)
			STATUS_CHECK_RET(spi_nor_issue_single_opcode(snor, SNOR_CMD_EON_EX_HIGH_BANK_MODE));
	}

	spi_nor_blank_part_fill_default_opcodes(bp);

	if (snor->sfdp.bfpt && snor->sfdp.bfpt_hdr->minor_ver >= SFDP_REV_MINOR_A) {
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
	}

	if (bp->p.read_io_caps & BIT_SPI_MEM_IO_4_4_4) {
		/* 6/10 dummy cycles will be used for QPI read */
		if (snor->param.vendor_flags & EON_F_DC_SR3_BIT7)
			bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 10;
		else
			bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 6;

		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;
	}

	if (bp->p.wp_ranges == &eon_3bp_tb_sec_cmp_dummy)
		bp->p.wp_ranges = eon_3bp_tb_sec_cmp;

	if (bp->p.wp_ranges == &eon_4bp_tb_cmp_dummy)
		bp->p.wp_ranges = eon_4bp_tb_cmp;

	return UFP_OK;
}

static ufprog_status eon_otp_fixup(struct spi_nor *snor)
{
	if (snor->param.vendor_flags & EON_F_OTP_TYPE_1)
		snor->ext_param.ops.otp = &eon_otp_1_ops;
	else if (snor->param.vendor_flags & EON_F_OTP_TYPE_2)
		snor->ext_param.ops.otp = &eon_otp_2_ops;
	else if (snor->param.vendor_flags & EON_F_OTP_TYPE_3)
		snor->ext_param.ops.otp = &eon_otp_3_ops;
	else if (snor->param.vendor_flags & EON_F_OTP_TYPE_4)
		snor->ext_param.ops.otp = &eon_otp_4_ops;
	else if (snor->param.vendor_flags & EON_F_OTP_TYPE_SECR)
		snor->ext_param.ops.otp = &eon_otp_secr_ops;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup eon_fixups = {
	.pre_param_setup = eon_part_fixup,
	.pre_chip_setup = eon_otp_fixup,
};

static ufprog_status eon_chip_setup(struct spi_nor *snor)
{
	uint32_t regval;
	uint8_t val;

	if (snor->param.vendor_flags & EON_F_HIGH_BANK_LATCH)
		STATUS_CHECK_RET(spi_nor_issue_single_opcode(snor, SNOR_CMD_EON_EX_HIGH_BANK_MODE));

	if (snor->param.vendor_flags & EON_F_DC_SR3_BIT5_4) {
		STATUS_CHECK_RET(spi_nor_read_reg(snor, SNOR_CMD_EON_READ_SR3, &val));
		val &= ~BITS(5, 4);
		STATUS_CHECK_RET(spi_nor_write_reg(snor, SNOR_CMD_EON_READ_SR3, val));

		STATUS_CHECK_RET(spi_nor_read_reg(snor, SNOR_CMD_EON_READ_SR3, &val));
		if (((val & BITS(5, 4)) >> 4) != 0) {
			logm_err("Failed to set Read Dummy Cycles to 6\n");
			return UFP_FAIL;
		}
	} else if (snor->param.vendor_flags & EON_F_DC_SR3_BIT7) {
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &sr3_acc, 0, BIT(7), true));

		STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &sr3_acc, &regval));
		if (!(regval & BIT(7))) {
			logm_err("Failed to set Read Dummy Cycles to 10\n");
			return UFP_FAIL;
		}
	}

	return UFP_OK;
}

static ufprog_status eon_read_uid_4bh(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_READ_UNIQUE_ID, 1),
		SPI_MEM_OP_NO_ADDR,
		SPI_MEM_OP_DUMMY(snor->state.a4b_mode ? 5 : 4, 1),
		SPI_MEM_OP_DATA_IN(EON_UID_4BH_LEN, data, 1)
	);

	if (retlen)
		*retlen = EON_UID_4BH_LEN;

	if (!data)
		return UFP_OK;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_status eon_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	uint32_t addr = 0x80;

	if (snor->param.vendor_flags & EON_F_READ_UID_4BH)
		return eon_read_uid_4bh(snor, data, retlen);

	if (retlen)
		*retlen = EON_UID_LEN;

	if (!data)
		return UFP_OK;

	if (snor->param.vendor_flags & EON_F_READ_UID_SFDP_1E0H)
		addr = 0x1e0;

	return spi_nor_read_sfdp(snor, snor->state.cmd_buswidth_curr, addr, EON_UID_LEN, data);
}

static const struct spi_nor_flash_part_ops eon_part_ops = {
	.chip_setup = eon_chip_setup,
	.read_uid = eon_read_uid,
};

static ufprog_status eon_init(void)
{
	eon_3bp_tb_sec_cmp = wp_bp_info_copy(&wpr_3bp_tb_sec_cmp);
	if (!eon_3bp_tb_sec_cmp)
		return UFP_NOMEM;

	eon_3bp_tb_sec_cmp->access = &eon_sr1_sr4_acc;

	eon_4bp_tb_cmp = wp_bp_info_copy(&wpr_4bp_tb_cmp);
	if (!eon_4bp_tb_cmp) {
		free(eon_3bp_tb_sec_cmp);
		return UFP_NOMEM;
	}

	eon_4bp_tb_cmp->access = &eon_sr1_sr4_acc;

	return UFP_OK;
}

static const struct spi_nor_vendor_ops eon_ops = {
	.init = eon_init,
};

const struct spi_nor_vendor vendor_eon = {
	.mfr_id = SNOR_VENDOR_EON,
	.id = "eon",
	.name = "EON",
	.parts = eon_parts,
	.nparts = ARRAY_SIZE(eon_parts),
	.vendor_flag_names = eon_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(eon_vendor_flag_info),
	.ops = &eon_ops,
	.default_part_ops = &eon_part_ops,
	.default_part_fixups = &eon_fixups,
};
