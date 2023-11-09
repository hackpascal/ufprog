// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Infineon/Cypress/Spansion SPI-NOR flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/log.h>
#include <ufprog/sizes.h>
#include <ufprog/spi-nor-opcode.h>
#include <ufprog/spi-nor-sfdp.h>
#include "core.h"
#include "part.h"
#include "regs.h"
#include "otp.h"

#define SPANSION_UID_LEN			8
#define S25FLxS_UID_LEN				16

/* BP bits */
#define SR_BP3					BIT(5)
#define BP_3_0					(SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0)

/* Spansion S25FL1xK register fields */
#define S25FL1xK_SRCR_LC_SHIFT			16
#define S25FL1xK_SRCR_LC_MASK			BITS(19, S25FL1xK_SRCR_LC_SHIFT)

/* Spansion S25FLxP register fields */
#define SP_SRCR_TBPARM				BIT(10)
#define SP_SRCR_BPNV				BIT(11)
#define SP_SRCR_TBPROT				BIT(13)

/* Spansion S25FLxS register fields */
#define S25FLxS_SRCR_LC_SHIFT			14
#define S25FLxS_SRCR_LC_MASK			BITS(15, S25FLxS_SRCR_LC_SHIFT)

/* Spansion S25FL127S register fields */
#define S25FL127S_SR2_PS			BIT(6)
#define S25FL127S_SR2_BS			BIT(7)

/* Spansion S25FLxL register fields */
#define S25FLxL_SR3_DC_SHIFT			24
#define S25FLxL_SR3_DC_MASK			BITS(27, S25FLxL_SR3_DC_SHIFT)
#define S25FLxL_SR3_WE				BIT(28)

/* Spansion S25FSxS register fields */
#define S25FSxS_CR1_TBPARM			BIT(2)
#define S25FSxS_CR1_BPNV			BIT(3)
#define S25FSxS_CR1_TBPROT			BIT(5)

#define S25FSxS_CR2_DC_SHIFT			0
#define S25FSxS_CR2_DC_MASK			BITS(3, S25FSxS_CR2_DC_SHIFT)

#define S25FSxS_CR2_AL				BIT(7)

#define S25FSxS_CR3_BS				BIT(1)
#define S25FSxS_CR3_PS				BIT(4)
#define S25FSxS_CR3_BC				BIT(5)

#define S25FSxS_CR4_WE				BIT(4)

/* Spansion vendor flags */
#define SP_F_SR_PE_ERR_BITS			BIT(0)
#define SP_F_SR2_PE_ERR_BITS			BIT(1)
#define SP_F_DC_CR3_BIT3_0_SET_8		BIT(2)

static const struct spi_nor_part_flag_enum_info spansion_vendor_flag_info[] = {
	{ 0, "sr-has-pe-err-bits" },
	{ 1, "sr2-has-pe-err-bits" },
	{ 2, "dc-cr3-bit0-3-set-to-8" },
};
static const struct spi_nor_reg_access s25fl1xk_srcr_acc = {
	.type = SNOR_REG_READ_MULTI_WRITE_ONCE,
	.num = 3,
	.desc[0] = {
		.ndata = 1,
		.read_opcode = SNOR_CMD_READ_SR,
		.write_opcode = SNOR_CMD_WRITE_SR,
		.flags = SNOR_REGACC_F_SR,
	},
	.desc[1] = {
		.ndata = 1,
		.read_opcode = SNOR_CMD_READ_CR,
	},
	.desc[2] = {
		.ndata = 1,
		.read_opcode = SNOR_CMD_READ_SR3,
	},
};

static const struct spi_nor_reg_access s25fl127s_srcrsr2_acc = {
	.type = SNOR_REG_READ_MULTI_WRITE_ONCE,
	.num = 3,
	.desc[0] = {
		.ndata = 1,
		.read_opcode = SNOR_CMD_READ_SR,
		.write_opcode = SNOR_CMD_WRITE_SR,
		.flags = SNOR_REGACC_F_SR,
	},
	.desc[1] = {
		.ndata = 1,
		.read_opcode = SNOR_CMD_READ_CR,
	},
	.desc[2] = {
		.ndata = 1,
		.read_opcode = SNOR_CMD_SPANDION_READ_SR2,
	},
};

static const struct spi_nor_reg_access s25fl127s_sr2_acc = SNOR_REG_ACC_NORMAL(SNOR_CMD_SPANDION_READ_SR2, 0);

static const struct spi_nor_reg_access s25flxl_sr1cr123_acc = {
	.type = SNOR_REG_READ_MULTI_WRITE_ONCE,
	.num = 4,
	.desc[0] = {
		.ndata = 1,
		.read_opcode = SNOR_CMD_READ_SR,
		.write_opcode = SNOR_CMD_WRITE_SR,
		.flags = SNOR_REGACC_F_SR,
	},
	.desc[1] = {
		.ndata = 1,
		.read_opcode = SNOR_CMD_READ_CR,
	},
	.desc[2] = {
		.ndata = 1,
		.read_opcode = SNOR_CMD_READ_SR3,
	},
	.desc[3] = {
		.ndata = 1,
		.read_opcode = SNOR_CMD_SPANDION_READ_CR3,
	},
};

#define S25FxS_ANY_REG(_addr)											\
	{ .type = SNOR_REG_NORMAL, .num = 1,									\
	  .desc[0] = { .flags = SNOR_REGACC_F_ADDR_4B_MODE | SNOR_REGACC_F_DATA_ACC_TIMING, .addr = (_addr),	\
		       .read_opcode = SNOR_CMD_READ_AR, .write_opcode = SNOR_CMD_WRITE_AR, .ndata = 1 }		\
	}

static const struct spi_nor_reg_access s25fxs_sr1nv = S25FxS_ANY_REG(0);
static const struct spi_nor_reg_access s25fxs_cr1nv = S25FxS_ANY_REG(2);
static const struct spi_nor_reg_access s25fxs_cr2nv = S25FxS_ANY_REG(3);
static const struct spi_nor_reg_access s25fxs_cr3nv = S25FxS_ANY_REG(4);
static const struct spi_nor_reg_access s25fxs_cr4nv = S25FxS_ANY_REG(5);
static const struct spi_nor_reg_access s25fxs_cr2v = S25FxS_ANY_REG(0x800003);
static const struct spi_nor_reg_access s25fxs_cr3v = S25FxS_ANY_REG(0x800004);
static const struct spi_nor_reg_access s25fxs_cr4v = S25FxS_ANY_REG(0x800005);

static const struct spi_nor_reg_field_item s25flxxd_2bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def s25flxxd_2bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc,
								   s25flxxd_2bp_sr_fields);

static const struct snor_reg_info s25flxxd_2bp_regs = SNOR_REG_INFO(&s25flxxd_2bp_sr);

static const struct spi_nor_reg_field_item s25fl_3bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def s25fl_3bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc,
								s25fl_3bp_sr_fields);

static const struct snor_reg_info s25fl_3bp_regs = SNOR_REG_INFO(&s25fl_3bp_sr);

static const struct spi_nor_reg_field_item s25fl_4bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def s25fl_4bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc,
								      s25fl_4bp_sr_fields);

static const struct snor_reg_info s25fl_4bp_regs = SNOR_REG_INFO(&s25fl_4bp_sr);

static const struct spi_nor_reg_field_item s25fl1xk_srcr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(6, 1, "SEC", "Sector Protect"),
	SNOR_REG_FIELD(7, 1, "SRP0", "Status Register Protect 0"),
	SNOR_REG_FIELD(8, 1, "SRP1", "Status Register Protect 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(11, 1, "LB1", "Security Register Lock Bit 1"),
	SNOR_REG_FIELD(12, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(13, 1, "LB3", "Security Register Lock Bit 3"),
	SNOR_REG_FIELD(14, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def s25fl1xk_srcr = SNOR_REG_DEF("SRCR", "Status & Configuration Register",
								 &s25fl1xk_srcr_acc, s25fl1xk_srcr_fields);

static const struct snor_reg_info s25fl1xk_regs = SNOR_REG_INFO(&s25fl1xk_srcr);

static const struct spi_nor_reg_field_values s25flxp_cr_tbparm_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "Bottom (Low address)"),
	VALUE_ITEM(1, "Top (High address)"),
);

static const struct spi_nor_reg_field_values s25flxp_cr_tbprot_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "Top (High address)"),
	VALUE_ITEM(1, "Bottom (Low address)"),
);

static const struct spi_nor_reg_field_item s25flxp_srcr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD_FULL(10, 1, "TBPARM", "Parameter sector location (OTP)", &s25flxp_cr_tbparm_values),
	SNOR_REG_FIELD_YES_NO(11, 1, "BPNV", "Volatile BP2-0 bits (OTP)"),
	SNOR_REG_FIELD_FULL(13, 1, "TBPROT", "Block protection location (OTP)", &s25flxp_cr_tbprot_values),
};

static const struct spi_nor_reg_def s25flxp_srcr = SNOR_REG_DEF("SRCR", "Status & Configuration Register", &srcr_acc,
								s25flxp_srcr_fields);

static const struct snor_reg_info s25flxp_regs = SNOR_REG_INFO(&s25flxp_srcr);

static const struct spi_nor_reg_field_item s25flxp_256k_srcr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD_YES_NO(11, 1, "BPNV", "Volatile BP2-0 bits (OTP)"),
	SNOR_REG_FIELD_FULL(13, 1, "TBPROT", "Block protection location (OTP)", &s25flxp_cr_tbprot_values),
};

static const struct spi_nor_reg_def s25flxp_256k_srcr = SNOR_REG_DEF("SRCR", "Status & Configuration Register",
								     &srcr_acc, s25flxp_256k_srcr_fields);

static const struct snor_reg_info s25flxp_256k_regs = SNOR_REG_INFO(&s25flxp_256k_srcr);

static const struct spi_nor_reg_field_item s25flxs_srcr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD_FULL(10, 1, "TBPARM", "Parameter sector location (OTP)", &s25flxp_cr_tbparm_values),
	SNOR_REG_FIELD_YES_NO(11, 1, "BPNV", "Volatile BP2-0 bits (OTP)"),
	SNOR_REG_FIELD_FULL(13, 1, "TBPROT", "Block protection location (OTP)", &s25flxp_cr_tbprot_values),
	SNOR_REG_FIELD(14, 3, "LC", "Latency Code"),
};

static const struct spi_nor_reg_def s25flxs_srcr = SNOR_REG_DEF("SRCR", "Status & Configuration Register", &srcr_acc,
								s25flxs_srcr_fields);

static const struct snor_reg_info s25flxs_regs = SNOR_REG_INFO(&s25flxs_srcr);

static const struct spi_nor_reg_field_item s25flxs_256k_srcr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD_FULL(10, 1, "TBPARM", "Parameter sector location (OTP)", &s25flxp_cr_tbparm_values),
	SNOR_REG_FIELD_YES_NO(11, 1, "BPNV", "Volatile BP2-0 bits (OTP)"),
	SNOR_REG_FIELD_FULL(13, 1, "TBPROT", "Block protection location (OTP)", &s25flxp_cr_tbprot_values),
	SNOR_REG_FIELD(14, 3, "LC", "Latency Code"),
};

static const struct spi_nor_reg_def s25flxs_256k_srcr = SNOR_REG_DEF("SRCR", "Status & Configuration Register",
								     &srcr_acc, s25flxs_256k_srcr_fields);

static const struct snor_reg_info s25flxs_256k_regs = SNOR_REG_INFO(&s25flxs_256k_srcr);

static const struct spi_nor_reg_field_values s25fl127s_ps_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "256B"),
	VALUE_ITEM(1, "512B"),
);

static const struct spi_nor_reg_field_values s25fl127s_bs_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "64KB (Hybrid 4KB/64KB)"),
	VALUE_ITEM(1, "256KB Uniform"),
);

static const struct spi_nor_reg_field_item s25fl127s_srcrsr2_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD_FULL(10, 1, "TBPARM", "Parameter sector location (OTP)", &s25flxp_cr_tbparm_values),
	SNOR_REG_FIELD_YES_NO(11, 1, "BPNV", "Volatile BP2-0 bits (OTP)"),
	SNOR_REG_FIELD_FULL(13, 1, "TBPROT", "Block protection location (OTP)", &s25flxp_cr_tbprot_values),
	SNOR_REG_FIELD(14, 3, "LC", "Latency Code"),
	SNOR_REG_FIELD_FULL(21, 1, "IO3F", "IO3 Function (OTP)", &w25q_sr3_hold_rst_values),
	SNOR_REG_FIELD_FULL(22, 1, "PS", "Page Size (OTP)", &s25fl127s_ps_values),
	SNOR_REG_FIELD_FULL(23, 1, "BS", "Block Size (OTP)", &s25fl127s_bs_values),
};

static const struct spi_nor_reg_def s25fl127s_srcr = SNOR_REG_DEF("SRCRSR2", "Status 1/2 & Configuration Registers",
								  &s25fl127s_srcrsr2_acc, s25fl127s_srcrsr2_fields);

static const struct snor_reg_info s25fl127s_regs = SNOR_REG_INFO(&s25fl127s_srcr);

static const struct spi_nor_reg_field_values s25flxl_cr3_wl_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "8-Byte"),
	VALUE_ITEM(1, "16-Byte"),
	VALUE_ITEM(2, "32-Byte"),
	VALUE_ITEM(3, "16-Byte"),
);

static const struct spi_nor_reg_field_item s25flxl_sr1cr123_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(6, 1, "SEC", "Sector Protect"),
	SNOR_REG_FIELD(7, 1, "SRP0", "Status Register Protect 0"),
	SNOR_REG_FIELD(8, 1, "SRP1", "Status Register Protect 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(10, 1, "LB0", "Security Register Lock Bit 0"),
	SNOR_REG_FIELD(11, 1, "LB1", "Security Register Lock Bit 1"),
	SNOR_REG_FIELD(12, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(13, 1, "LB3", "Security Register Lock Bit 3"),
	SNOR_REG_FIELD(14, 1, "CMP", "Complement Protect"),
	SNOR_REG_FIELD_FULL(17, 1, "ADP", "Power-up Address Mode", &w25q_sr3_adp_values),
	SNOR_REG_FIELD_FULL(18, 1, "WPS", "Write Protection Selection", &w25q_sr3_wps_values),
	SNOR_REG_FIELD_ENABLED_DISABLED(19, 1, "QPI", "QPI Enable"),
	SNOR_REG_FIELD(21, 3, "OI", "Output Impedance"),
	SNOR_REG_FIELD_YES_NO(23, 1, "IO3R", "IO3 is RESET#"),
	SNOR_REG_FIELD(24, 0xf, "DC", "Read Dummy Cycles"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(28, 1, "WE", "Wrap Enable"),
	SNOR_REG_FIELD_FULL(29, 3, "WL", "Wrap Length", &s25flxl_cr3_wl_values),
};

static const struct spi_nor_reg_def s25flxl_sr1cr123 = SNOR_REG_DEF("SR1CR123", "Status & Configuration Registers",
								    &s25flxl_sr1cr123_acc, s25flxl_sr1cr123_fields);

static const struct snor_reg_info s25flxl_regs = SNOR_REG_INFO(&s25flxl_sr1cr123);

static const struct spi_nor_reg_field_item s25fl256l_sr1cr123_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(6, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(7, 1, "SRP0", "Status Register Protect 0"),
	SNOR_REG_FIELD(8, 1, "SRP1", "Status Register Protect 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(10, 1, "LB0", "Security Register Lock Bit 0"),
	SNOR_REG_FIELD(11, 1, "LB1", "Security Register Lock Bit 1"),
	SNOR_REG_FIELD(12, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(13, 1, "LB3", "Security Register Lock Bit 3"),
	SNOR_REG_FIELD(14, 1, "CMP", "Complement Protect"),
	SNOR_REG_FIELD_FULL(17, 1, "ADP", "Power-up Address Mode", &w25q_sr3_adp_values),
	SNOR_REG_FIELD_FULL(18, 1, "WPS", "Write Protection Selection", &w25q_sr3_wps_values),
	SNOR_REG_FIELD_ENABLED_DISABLED(19, 1, "QPI", "QPI Enable"),
	SNOR_REG_FIELD(21, 3, "OI", "Output Impedance"),
	SNOR_REG_FIELD_YES_NO(23, 1, "IO3R", "IO3 is RESET#"),
	SNOR_REG_FIELD(24, 0xf, "DC", "Read Dummy Cycles"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(28, 1, "WE", "Wrap Enable"),
	SNOR_REG_FIELD_FULL(29, 3, "WL", "Wrap Length", &s25flxl_cr3_wl_values),
};

static const struct spi_nor_reg_def s25fl256l_sr1cr123 = SNOR_REG_DEF("SR1CR123", "Status & Configuration Registers",
								      &s25flxl_sr1cr123_acc, s25fl256l_sr1cr123_fields);

static const struct snor_reg_info s25fl256l_regs = SNOR_REG_INFO(&s25fl256l_sr1cr123);

static const struct spi_nor_reg_field_item s25fsxs_sr1_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def s25fsxs_sr1 = SNOR_REG_DEF("SR1", "Status Register 1", &s25fxs_sr1nv,
							       s25fsxs_sr1_fields);

static const struct spi_nor_reg_field_item s25fsxs_cr1_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(1, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD_FULL(2, 1, "TBPARM", "Parameter sector location (OTP)", &s25flxp_cr_tbparm_values),
	SNOR_REG_FIELD_YES_NO(3, 1, "BPNV", "Volatile BP2-0 bits (OTP)"),
	SNOR_REG_FIELD_FULL(5, 1, "TBPROT", "Block protection location (OTP)", &s25flxp_cr_tbprot_values),
};

static const struct spi_nor_reg_def s25fsxs_cr1 = SNOR_REG_DEF("CR1", "Configuration Register 1", &s25fxs_cr1nv,
							       s25fsxs_cr1_fields);

static const struct spi_nor_reg_field_item s25fsxs_cr2_fields[] = {
	SNOR_REG_FIELD(0, 0xf, "DC", "Read Dummy Cycles (OTP)"),
	SNOR_REG_FIELD_ENABLED_DISABLED(5, 1, "IO3R", "IO3 is RESET# (OTP)"),
	SNOR_REG_FIELD_ENABLED_DISABLED(6, 1, "QPI", "QPI Enable (OTP)"),
	SNOR_REG_FIELD_FULL(7, 1, "ADP", "Power-up Address Mode (OTP)", &w25q_sr3_adp_values),
};

static const struct spi_nor_reg_def s25fsxs_cr2 = SNOR_REG_DEF("CR2", "Configuration Register 2", &s25fxs_cr2nv,
							       s25fsxs_cr2_fields);

static const struct spi_nor_reg_field_values s25fsxs_bs_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "64KB"),
	VALUE_ITEM(1, "256KB"),
);

static const struct spi_nor_reg_field_values s25fsxs_30h_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "Clear Status Command"),
	VALUE_ITEM(1, "Erase/Program Resume Command"),
);

static const struct spi_nor_reg_field_item s25fsxs_cr3_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(0, 1, "F0H", "F0h Software Reset (OTP)"),
	SNOR_REG_FIELD_FULL(1, 1, "BS", "Block Size (OTP)", &s25fsxs_bs_values),
	SNOR_REG_FIELD_FULL(2, 1, "30H", "30h Function (OTP)", &s25fsxs_30h_values),
	SNOR_REG_FIELD_ENABLED_DISABLED(3, 1, "4KE", "4KB Erase (OTP)"),
	SNOR_REG_FIELD_FULL(4, 1, "PS", "Page Size (OTP)", &s25fl127s_ps_values),
	SNOR_REG_FIELD_ENABLED_DISABLED(5, 1, "BC", "Blank Check (OTP)"),
};

static const struct spi_nor_reg_def s25fsxs_cr3 = SNOR_REG_DEF("CR3", "Configuration Register 3", &s25fxs_cr3nv,
							       s25fsxs_cr3_fields);

static const struct spi_nor_reg_field_item s25fsxs_cr4_fields[] = {
	SNOR_REG_FIELD_FULL(0, 3, "WL", "Wrap Length", &s25flxl_cr3_wl_values),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(4, 1, "WE", "Wrap Enable"),
	SNOR_REG_FIELD(5, 7, "OI", "Output Impedance"),
};

static const struct spi_nor_reg_def s25fsxs_cr4 = SNOR_REG_DEF("CR4", "Configuration Register 4", &s25fxs_cr4nv,
							       s25fsxs_cr4_fields);

static const struct snor_reg_info s25fsxs_regs = SNOR_REG_INFO(&s25fsxs_sr1, &s25fsxs_cr1, &s25fsxs_cr2, &s25fsxs_cr3,
							       &s25fsxs_cr4);

static const struct spi_nor_otp_info s25fl1xk_otp_3 = {
	.start_index = 1,
	.count = 3,
	.size = 0x100,
};

static const struct spi_nor_otp_info s25flxs_otp = {
	.start_index = 0,
	.count = 32,
	.size = 0x20,
};

static const struct spi_nor_otp_info s25flxl_otp = {
	.start_index = 0,
	.count = 4,
	.size = 0x100,
};

static const struct spi_nor_wp_info s25fl2xk_wpr_4bp = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_NONE(     0                                   ),	/* None */
	SNOR_WP_NONE(     SR_BP3                              ),	/* None */

	SNOR_WP_ALL(               SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_UP(                               SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                      SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                      SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(             SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(             SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(             SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */

	SNOR_WP_SP_CMP_LO(SR_BP3 |                   SR_BP0, 1),	/* Lower T - 8KB */
	SNOR_WP_SP_CMP_LO(SR_BP3 |          SR_BP1         , 2),	/* Lower T - 16KB */
	SNOR_WP_SP_CMP_LO(SR_BP3 |          SR_BP1 | SR_BP0, 3),	/* Lower T - 32KB */
	SNOR_WP_SP_CMP_LO(SR_BP3 | SR_BP2                  , 4),	/* Lower T - 64KB */
	SNOR_WP_SP_CMP_LO(SR_BP3 | SR_BP2 |          SR_BP0, 5),	/* Lower T - 128KB */
	SNOR_WP_SP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 6),	/* Lower T - 256KB */
);

static const struct spi_nor_wp_info s25fl216k_wpr_4bp = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_NONE(     0                                   ),	/* None */

	SNOR_WP_ALL(      SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP3 |                   SR_BP0   ),	/* All */
	SNOR_WP_ALL(      SR_BP3                              ),	/* All */
	SNOR_WP_ALL(               SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(               SR_BP2 | SR_BP1            ),	/* All */

	SNOR_WP_BP_UP(                               SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                      SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                      SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(             SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(             SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */

	SNOR_WP_BP_CMP_LO(SR_BP3 |          SR_BP1         , 4),	/* Lower T - 1MB */
	SNOR_WP_BP_CMP_LO(SR_BP3 |          SR_BP1 | SR_BP0, 3),	/* Lower T - 512KB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2                  , 2),	/* Lower T - 256KB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 |          SR_BP0, 1),	/* Lower T - 128KB */
	SNOR_WP_BP_CMP_LO(SR_BP3 | SR_BP2 | SR_BP1         , 0),	/* Lower T - 64KB */
);

static const struct spi_nor_wp_info s25fl128p00_wpr_4bp_up = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_NONE( 0                                   ),	/* None */
	SNOR_WP_ALL(  SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_UP(                           SR_BP0, 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                  SR_BP1         , 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(                  SR_BP1 | SR_BP0, 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(         SR_BP2                  , 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(         SR_BP2 |          SR_BP0, 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(         SR_BP2 | SR_BP1         , 6),	/* Upper 4MB */
	SNOR_WP_BP_UP(         SR_BP2 | SR_BP1 | SR_BP0, 7),	/* Upper 8MB */
	SNOR_WP_BP_UP(SR_BP3                           , 8),	/* Upper 16MB */
	SNOR_WP_BP_UP(SR_BP3 |                   SR_BP0, 9),	/* Upper 32MB */
	SNOR_WP_BP_UP(SR_BP3 |          SR_BP1         , 10),	/* Upper 64MB */
	SNOR_WP_BP_UP(SR_BP3 |          SR_BP1 | SR_BP0, 11),	/* Upper 128MB */
	SNOR_WP_BP_UP(SR_BP3 | SR_BP2                  , 12),	/* Upper 256MB */
	SNOR_WP_BP_UP(SR_BP3 | SR_BP2 |          SR_BP0, 13),	/* Upper 512MB */
	SNOR_WP_BP_UP(SR_BP3 | SR_BP2 | SR_BP1         , 14),	/* Upper 1GB */
);

static ufprog_status s25flx16k_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.data)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "S25FL116K");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "S25FL216K");
}

static const struct spi_nor_flash_part_fixup s25flx16k_fixups = {
	.pre_param_setup = s25flx16k_fixup_model,
};



static const uint8_t s25fl1xk_dc_1_4_4[] = { 8, 10, 12, 14 };
static const uint8_t s25fl1xk_dc_1_2_2[] = { 4, 8, 12 };

static ufprog_status s25fl1xk_part_select_dummy_cycles(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	const uint8_t *dcs;
	uint32_t ndcs, i;

	/* Test for 1-4-4 */
	dcs = s25fl1xk_dc_1_4_4;
	ndcs = ARRAY_SIZE(s25fl1xk_dc_1_4_4);

	bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;

	for (i = 0; i < ndcs; i++) {
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = dcs[i];

		if (spi_nor_test_io_opcode(snor, bp->read_opcodes_3b, SPI_MEM_IO_1_4_4, 3, SPI_DATA_IN))
			break;
	}

	/* No test for 1-1-4 */
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy = 8;
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].nmode = 0;

	/* Test for 1-2-2 */
	dcs = s25fl1xk_dc_1_2_2;
	ndcs = ARRAY_SIZE(s25fl1xk_dc_1_2_2);

	bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = 0;

	for (i = 0; i < ndcs; i++) {
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = dcs[i];

		if (spi_nor_test_io_opcode(snor, bp->read_opcodes_3b, SPI_MEM_IO_1_2_2, 3, SPI_DATA_IN))
			break;
	}

	/* No test for 1-1-2 */
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].ndummy = 8;
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].nmode = 0;

	/* No test for 1-1-1 */
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].ndummy = 8;
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].nmode = 0;

	return UFP_OK;
}

static ufprog_status s25fl1xk_part_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	static const struct spi_nor_vendor vendor_s25fl1xk = { 0 };

	/* Do not perform vendor fixups */
	vp->vendor_init = &vendor_s25fl1xk;

	spi_nor_blank_part_fill_default_opcodes(bp);

	snor->state.reg.sr_w = &s25fl1xk_srcr_acc;
	snor->state.reg.cr = &s25fl1xk_srcr_acc;
	snor->state.reg.cr_shift = 8;

	STATUS_CHECK_RET(s25fl1xk_part_select_dummy_cycles(snor, bp));

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup s25fl1xk_fixups = {
	.pre_param_setup = s25fl1xk_part_fixup,
};

static ufprog_status s25fl1xk_chip_setup(struct spi_nor *snor)
{
	uint32_t val, ndummy;

	ndummy = snor->state.read_ndummy * 8 / spi_mem_io_info_addr_bw(snor->state.read_io_info);
	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &s25fl1xk_srcr_acc, S25FL1xK_SRCR_LC_MASK, ndummy << S25FL1xK_SRCR_LC_SHIFT,
						true));
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &s25fl1xk_srcr_acc, &val));

	val = (val & S25FL1xK_SRCR_LC_MASK) >> S25FL1xK_SRCR_LC_SHIFT;

	if (val != ndummy) {
		logm_err("Failed to set read dummy cycles to %u\n", ndummy);
		return UFP_UNSUPPORTED;
	}

	return UFP_OK;
}

static ufprog_status s25fl1xk_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	if (retlen)
		*retlen = SPANSION_UID_LEN;

	if (!data)
		return UFP_OK;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));

	return spi_nor_read_sfdp(snor, 1, 0xf8, 8, data);
}

static ufprog_status s25fl1xk_quad_enable(struct spi_nor *snor)
{
	return spi_nor_quad_enable_any(snor, &s25fl1xk_srcr_acc, 9);
}

static const struct spi_nor_flash_part_ops s25fl1xk_part_ops = {
	.otp = &secr_otp_ops,

	.chip_setup = s25fl1xk_chip_setup,
	.read_uid = s25fl1xk_read_uid,
	.quad_enable = s25fl1xk_quad_enable,
};

static ufprog_status s25fl_read_otp_raw(struct spi_nor *snor, uint32_t addr, uint32_t len, void *data)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_READ_UNIQUE_ID, 1),
		SPI_MEM_OP_ADDR(3, addr, 1),
		SPI_MEM_OP_DUMMY(1, 1),
		SPI_MEM_OP_DATA_IN(len, data, 1)
	);

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_status s25fl_write_otp_raw(struct spi_nor *snor, uint32_t addr, uint32_t len, const void *data)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_PROG_OTP, 1),
		SPI_MEM_OP_ADDR(3, addr, 1),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_DATA_OUT(len, data, 1)
	);

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_status s25flxp_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	if (retlen)
		*retlen = SPANSION_UID_LEN;

	if (!data)
		return UFP_OK;

	return s25fl_read_otp_raw(snor, 0x102, SPANSION_UID_LEN, data);
}

static const struct spi_nor_erase_info s25flxp_erase_opcodes = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_4K, SNOR_CMD_SECTOR_ERASE), /* BIT(0) */
	SNOR_ERASE_SECTOR(SZ_8K, SNOR_CMD_SECTOR_ERASE_8K), /* BIT(1) */
	SNOR_ERASE_SECTOR(SZ_64K, SNOR_CMD_BLOCK_ERASE), /* BIT(2) */
);

static const struct spi_nor_erase_region s25fl_erase_regions_bottom[] = {
	SNOR_ERASE_REGION(SZ_128K, SZ_4K, SZ_64K, BITS(2, 0)),
	SNOR_ERASE_REGION(0, SZ_64K, SZ_64K, BIT(2)),
};

static const struct spi_nor_erase_region s25fl_erase_regions_top[] = {
	SNOR_ERASE_REGION(0, SZ_64K, SZ_64K, BIT(2)),
	SNOR_ERASE_REGION(SZ_128K, SZ_4K, SZ_64K, BITS(2, 0)),
};

static ufprog_status s25flxp_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				   struct spi_nor_flash_part_blank *bp)
{
	const struct spi_nor_erase_region *erase_regions;
	uint32_t srcr;

	spi_nor_blank_part_fill_default_opcodes(bp);

	snor->state.reg.cr = &srcr_acc;
	snor->state.reg.cr_shift = 8;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &srcr_acc, &srcr));

	if (srcr & SP_SRCR_TBPROT)
		bp->p.wp_ranges = &wpr_3bp_lo_ratio;
	else
		bp->p.wp_ranges = &wpr_3bp_up_ratio;

	if (!(bp->p.flags & SNOR_F_SECT_256K)) {
		if (srcr & SP_SRCR_TBPARM) {
			erase_regions = s25fl_erase_regions_top;
			snor->ext_param.num_erase_regions = ARRAY_SIZE(s25fl_erase_regions_top);
		} else {
			erase_regions = s25fl_erase_regions_bottom;
			snor->ext_param.num_erase_regions = ARRAY_SIZE(s25fl_erase_regions_bottom);
		}

		snor->ext_param.erase_regions = calloc(snor->ext_param.num_erase_regions, sizeof(*erase_regions));
		if (!snor->ext_param.erase_regions) {
			logm_err("No memory for erase regions\n");
			return UFP_NOMEM;
		}

		memcpy(snor->ext_param.erase_regions, erase_regions,
		       snor->ext_param.num_erase_regions * sizeof(*erase_regions));

		if (srcr & SP_SRCR_TBPARM)
			snor->ext_param.erase_regions[0].size = bp->p.size - SZ_128K;
		else
			snor->ext_param.erase_regions[1].size = bp->p.size - SZ_128K;
	}

	snor->ext_param.ops.read_uid = s25flxp_read_uid;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup s25flxp_fixups = {
	.pre_param_setup = s25flxp_fixup,
};

static ufprog_status s25flxs_otp_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	return s25fl_read_otp_raw(snor, snor->ext_param.otp->size * index + addr, len, data);
}

static ufprog_status s25flxs_otp_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, const void *data)
{
	if (!index)
		return UFP_FAIL;

	return s25fl_write_otp_raw(snor, snor->ext_param.otp->size * index + addr, len, data);
}

static ufprog_status s25flxs_otp_lock(struct spi_nor *snor, uint32_t index)
{
	uint32_t lock_bits;

	if (!index)
		return UFP_FAIL;

	STATUS_CHECK_RET(s25fl_read_otp_raw(snor, S25FLxS_UID_LEN, sizeof(lock_bits), &lock_bits));

	lock_bits = le32toh(lock_bits);

	if (!(lock_bits & BIT(index)))
		return UFP_OK;

	lock_bits &= ~BIT(index);
	lock_bits = htole32(lock_bits);

	STATUS_CHECK_RET(s25fl_write_otp_raw(snor, S25FLxS_UID_LEN, sizeof(lock_bits), &lock_bits));

	/* Check result */
	STATUS_CHECK_RET(s25fl_read_otp_raw(snor, S25FLxS_UID_LEN, sizeof(lock_bits), &lock_bits));

	lock_bits = le32toh(lock_bits);

	if (lock_bits & BIT(index))
		return UFP_FAIL;

	return UFP_OK;
}

static ufprog_status s25flxs_otp_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{
	uint32_t lock_bits;

	STATUS_CHECK_RET(s25fl_read_otp_raw(snor, S25FLxS_UID_LEN, sizeof(lock_bits), &lock_bits));

	lock_bits = le32toh(lock_bits);

	if (lock_bits & BIT(index))
		*retlocked = false;
	else
		*retlocked = true;

	return UFP_OK;
}

static const struct spi_nor_flash_part_otp_ops s25flxs_otp_ops = {
	.read = s25flxs_otp_read,
	.write = s25flxs_otp_write,
	.lock = s25flxs_otp_lock,
	.locked = s25flxs_otp_locked,
};

static ufprog_status s25flxs_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	if (retlen)
		*retlen = S25FLxS_UID_LEN;

	if (!data)
		return UFP_OK;

	return s25fl_read_otp_raw(snor, 0, S25FLxS_UID_LEN, data);
}

static ufprog_status s25flxs_chip_setup(struct spi_nor *snor)
{
	return spi_nor_update_reg_acc(snor, &srcr_acc, S25FLxS_SRCR_LC_MASK, 0, false);
}

static const struct spi_nor_flash_part_ops s25flxs_part_ops = {
	.otp = &s25flxs_otp_ops,

	.read_uid = s25flxs_read_uid,
	.chip_setup = s25flxs_chip_setup,
};

static const struct spi_nor_erase_info s25flxs_erase_opcodes = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_4K, SNOR_CMD_SECTOR_ERASE), /* BIT(0) */
	SNOR_ERASE_SECTOR(SZ_64K, SNOR_CMD_BLOCK_ERASE), /* BIT(1) */
);

static const struct spi_nor_erase_info s25flxs_erase_4b_opcodes = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_4K, SNOR_CMD_4B_SECTOR_ERASE), /* BIT(0) */
	SNOR_ERASE_SECTOR(SZ_64K, SNOR_CMD_4B_BLOCK_ERASE), /* BIT(1) */
);

static const struct spi_nor_erase_region s25flxs_erase_regions_bottom[] = {
	SNOR_ERASE_REGION(SZ_128K, SZ_4K, SZ_64K, BIT(1) | BIT(0)),
	SNOR_ERASE_REGION(0, SZ_64K, SZ_64K, BIT(1)),
};

static const struct spi_nor_erase_region s25flxs_erase_regions_top[] = {
	SNOR_ERASE_REGION(0, SZ_64K, SZ_64K, BIT(1)),
	SNOR_ERASE_REGION(SZ_128K, SZ_4K, SZ_64K, BIT(1) | BIT(0)),
};

static ufprog_status s25flxs_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				   struct spi_nor_flash_part_blank *bp)
{
	const struct spi_nor_erase_region *erase_regions;
	uint32_t srcr;

	spi_nor_blank_part_fill_default_opcodes(bp);

	snor->state.reg.cr = &srcr_acc;
	snor->state.reg.cr_shift = 8;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &srcr_acc, &srcr));

	if (srcr & SP_SRCR_TBPROT)
		bp->p.wp_ranges = &wpr_3bp_lo_ratio;
	else
		bp->p.wp_ranges = &wpr_3bp_up_ratio;

	if (!(bp->p.flags & SNOR_F_SECT_256K)) {
		if (srcr & SP_SRCR_TBPARM) {
			erase_regions = s25flxs_erase_regions_top;
			snor->ext_param.num_erase_regions = ARRAY_SIZE(s25flxs_erase_regions_top);
		} else {
			erase_regions = s25flxs_erase_regions_bottom;
			snor->ext_param.num_erase_regions = ARRAY_SIZE(s25flxs_erase_regions_bottom);
		}

		snor->ext_param.erase_regions = calloc(snor->ext_param.num_erase_regions, sizeof(*erase_regions));
		if (!snor->ext_param.erase_regions) {
			logm_err("No memory for erase regions\n");
			return UFP_NOMEM;
		}

		memcpy(snor->ext_param.erase_regions, erase_regions,
		       snor->ext_param.num_erase_regions * sizeof(*erase_regions));

		if (srcr & SP_SRCR_TBPARM)
			snor->ext_param.erase_regions[0].size = bp->p.size - SZ_128K;
		else
			snor->ext_param.erase_regions[1].size = bp->p.size - SZ_128K;
	}

	bp->p.max_pp_time_us = 5000;
	snor->state.max_nvcr_pp_time_ms = 500;

	/* Keep 3-Byte Address initially */
	spi_nor_write_reg_acc(snor, &br_acc, 0, true);
	snor->state.a4b_mode = false;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup s25flxs_fixups = {
	.pre_param_setup = s25flxs_fixup,
};

static ufprog_status s25fl127s_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				     struct spi_nor_flash_part_blank *bp)
{
	uint32_t srcr, sr2;

	spi_nor_blank_part_fill_default_opcodes(bp);

	snor->state.reg.cr = &srcr_acc;
	snor->state.reg.cr_shift = 8;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &srcr_acc, &srcr));
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &s25fl127s_sr2_acc, &sr2));

	if (srcr & SP_SRCR_TBPROT)
		bp->p.wp_ranges = &wpr_3bp_lo_ratio;
	else
		bp->p.wp_ranges = &wpr_3bp_up_ratio;

	/* Don't rely on SFDP. This can be changed */
	if (sr2 & S25FL127S_SR2_PS)
		bp->p.page_size = 512;
	else
		bp->p.page_size = 256;

	snor->state.max_nvcr_pp_time_ms = 500;

	/* Still need to keep 3-Byte Address initially */
	spi_nor_write_reg_acc(snor, &br_acc, 0, true);
	snor->state.a4b_mode = false;

	return UFP_OK;
}

static ufprog_status s25fl127s_fixup_erase_time(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	uint32_t i;

	/* Max erase time enlarge */
	for (i = 0; i < SPI_NOR_MAX_ERASE_INFO; i++) {
		if (snor->param.erase_info.info[i].max_erase_time_ms)
			snor->param.erase_info.info[i].max_erase_time_ms *= 10;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup s25fl127s_fixups = {
	.pre_param_setup = s25fl127s_fixup,
	.post_param_setup = s25fl127s_fixup_erase_time,
};

static ufprog_status s25flxs_model_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.data)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "S25FL127S");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "S25FL128Sxxxxxx0");
}

static const struct spi_nor_flash_part_fixup s25flxs_model_fixups = {
	.pre_param_setup = s25flxs_model_fixup,
};

static ufprog_status s25flxs_256k_model_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					      struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.data)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "S25FL127S");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "S25FL128Sxxxxxx1");
}

static const struct spi_nor_flash_part_fixup s25flxs_256k_model_fixups = {
	.pre_param_setup = s25flxs_256k_model_fixup,
};

static ufprog_status s25fl512s_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				     struct spi_nor_flash_part_blank *bp)
{
	STATUS_CHECK_RET(s25flxs_fixup(snor, vp, bp));

	bp->p.pp_io_caps |= BIT_SPI_MEM_IO_1_1_4;
	bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].opcode = SNOR_CMD_PAGE_PROG_QUAD_IN;
	bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].nmode = 0;
	bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].opcode = SNOR_CMD_4B_PAGE_PROG_QUAD_IN;
	bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].ndummy = bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].nmode = 0;

	/* SFDP fixup */
	if (!spi_nor_sfdp_make_copy(snor))
		return UFP_NOMEM;

	if (snor->sfdp.smpt && snor->sfdp.smpt[1] == 0x3ffffff4)
		snor->sfdp.smpt[1] = 0x3fffff4;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup s25fl512s_fixups = {
	.pre_param_setup = s25fl512s_fixup,
};

static ufprog_status s25fsxs_set_secor_map_configuration(struct spi_nor *snor)
{
	uint32_t val;

	val = S25FSxS_CR3_BC;

	STATUS_CHECK_RET(spi_nor_write_reg_acc(snor, &s25fxs_cr3v, val, false));
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &s25fxs_cr3v, &val));

	/* Set Block Size = 64KB, Page size = 256B */
	if ((val & (S25FSxS_CR3_BC | S25FSxS_CR3_PS | S25FSxS_CR3_BS)) != val) {
		logm_err("Failed to set Page size/Block size/Blank Check\n");
		return UFP_UNSUPPORTED;
	}

	return UFP_OK;
}

static ufprog_status s25fsxs_chip_setup(struct spi_nor *snor)
{
	uint32_t val;

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &s25fxs_cr2v, S25FSxS_CR2_DC_MASK,
						(8 << S25FSxS_CR2_DC_SHIFT), false));

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &s25fxs_cr2v, &val));

	val = (val & S25FSxS_CR2_DC_MASK) >> S25FSxS_CR2_DC_SHIFT;

	if (val != 8) {
		logm_err("Failed to set read dummy cycles to %u\n", 8);
		return UFP_UNSUPPORTED;
	}

	STATUS_CHECK_RET(s25fsxs_set_secor_map_configuration(snor));

	STATUS_CHECK_RET(spi_nor_write_reg_acc(snor, &s25fxs_cr4v, S25FSxS_CR4_WE, false));
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &s25fxs_cr4v, &val));

	if (!(val & S25FSxS_CR4_WE)) {
		logm_err("Failed to disable wrap\n");
		return UFP_UNSUPPORTED;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_ops s25fsxs_part_ops = {
	.otp = &s25flxs_otp_ops,

	.read_uid = s25flxs_read_uid,
	.chip_setup = s25fsxs_chip_setup,
	.qpi_dis = spi_nor_disable_qpi_800003h,
};

static ufprog_status s25fsxs_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				   struct spi_nor_flash_part_blank *bp)
{
	uint32_t i, dw, cid, opcode, val;

	bp->p.max_pp_time_us = 5000;
	snor->state.max_nvcr_pp_time_ms = 1000;

	spi_nor_blank_part_fill_default_opcodes(bp);

	snor->state.reg.cr = &srcr_acc;
	snor->state.reg.cr_shift = 8;

	/* Reset to 3-Byte Address and Single SPI */
	spi_nor_write_reg_acc(snor, &s25fxs_cr2v, 8, false);
	snor->state.a4b_mode = false;
	snor->state.cmd_buswidth_curr = 1;

	/* WP range selection */
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &s25fxs_cr1nv, &val));

	if (val & S25FSxS_CR1_TBPROT)
		bp->p.wp_ranges = &wpr_3bp_lo_ratio;
	else
		bp->p.wp_ranges = &wpr_3bp_up_ratio;

	bp->p.page_size = 256;

	bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].ndummy = 8;
	bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].nmode = 0;
	bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = 8;
	bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = 4;
	bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = 8;
	bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 2;
	bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 8;
	bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 2;

	bp->read_opcodes_4b[SPI_MEM_IO_1_1_1].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].ndummy;
	bp->read_opcodes_4b[SPI_MEM_IO_1_1_1].nmode = bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].nmode;
	bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy;
	bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].nmode = bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode;
	bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy;
	bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].nmode = bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode;
	bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy;
	bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].nmode = bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode;

	/* SFDP fixup */
	if (!spi_nor_sfdp_make_copy(snor))
		return UFP_NOMEM;

	if (snor->sfdp.smpt) {
		for (i = 1; i <= snor->sfdp.smpt_dw_num; i += 2) {
			dw = sfdp_dw(snor->sfdp.smpt, i);
			if (dw & SMPT_DW1_DESCRIPTOR_TYPE)
				break;

			opcode = FIELD_GET(SMPT_CMD_DW1_DETECTION_OPCODE, dw);
			if (opcode == SNOR_CMD_READ_AR) {
				dw = sfdp_dw(snor->sfdp.smpt, i + 1);
				if (!(dw & 0x800000)) {
					dw |= 0x800000;
					sfdp_set_dw(snor->sfdp.smpt, i + 1, dw);
				}
			}
		}

		if (bp->p.size >= SZ_64M) {
			STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &s25fxs_cr3nv, &val));

			while (i <= snor->sfdp.smpt_dw_num) {
				dw = sfdp_dw(snor->sfdp.smpt, i);
				cid = FIELD_GET(SMPT_MAP_DW1_CONFIGURATION_ID, dw);
				if (val & S25FSxS_CR3_BS)
					cid |= 1;
				else
					cid &= ~1;
				dw &= ~FIELD_SET(SMPT_MAP_DW1_CONFIGURATION_ID, FIELD_MAX(SMPT_MAP_DW1_CONFIGURATION_ID));
				dw |= FIELD_SET(SMPT_MAP_DW1_CONFIGURATION_ID, cid);
				sfdp_set_dw(snor->sfdp.smpt, i, dw);

				if (dw & SMPT_DW1_SEQ_END_INDICATOR)
					break;

				/* increment the table index to the next map */
				i += FIELD_GET(SMPT_MAP_DW1_REGION_COUNT, dw) + 2;
			}
		}
	}

	STATUS_CHECK_RET(s25fsxs_set_secor_map_configuration(snor));

	return UFP_OK;
}

static ufprog_status s25fsxs_a4b_dis(struct spi_nor *snor)
{
	uint32_t val;

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &s25fxs_cr2v, S25FSxS_CR2_AL, 0, false));
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &s25fxs_cr2v, &val));

	if (val & S25FSxS_CR2_AL) {
		logm_err("Failed to set clear AL bit in CR2V\n");
		return UFP_FAIL;
	}

	return UFP_OK;
}

static ufprog_status s25fsxs_post_fixup(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	snor->ext_param.ops.a4b_dis = s25fsxs_a4b_dis;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup s25fsxs_fixups = {
	.pre_param_setup = s25fsxs_fixup,
	.post_param_setup = s25fsxs_post_fixup,
};

static const uint8_t s25fxs_id_mask[] = { 0xff, 0xff, 0xff, 0x00, 0x00, 0xff };

static const struct spi_nor_erase_info s25flxxd_erase_32k_opcodes = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_32K, SNOR_CMD_BLOCK_ERASE),
);

static DEFINE_SNOR_ALIAS(s25fl004a_alias, SNOR_ALIAS_MODEL("S25FL040A0LxxI00"));

static const struct spi_nor_flash_part spansion_parts[] = {
	SNOR_PART("S25FL001D", SNOR_ID_NONE, SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO(&s25flxxd_erase_32k_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(25),
		  SNOR_REGS(&s25flxxd_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up_ratio),
	),

	SNOR_PART("S25FL002D", SNOR_ID_NONE, SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(25),
		  SNOR_REGS(&s25flxxd_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up_ratio),
	),

	SNOR_PART("S25FL004D", SNOR_ID_NONE, SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(25),
		  SNOR_REGS(&s25fl_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("S25FL004A", SNOR_ID(0x01, 0x02, 0x12), SZ_512K,
		  SNOR_ALIAS(&s25fl004a_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&s25fl_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("S25FL040A0LxxI01", SNOR_ID(0x01, 0x02, 0x25), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_OP), /* Unable to support 12KB sector now */
	),

	SNOR_PART("S25FL040A0LxxI02", SNOR_ID(0x01, 0x02, 0x26), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_OP), /* Unable to support 12KB sector now */
	),

	SNOR_PART("S25FL204K", SNOR_ID(0x01, 0x40, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&s25fl_4bp_regs),
		  SNOR_WP_RANGES(&s25fl2xk_wpr_4bp),
	),

	SNOR_PART("S25FL008A", SNOR_ID(0x01, 0x02, 0x13), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&s25fl_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("S25FL208K", SNOR_ID(0x01, 0x40, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(76),
		  SNOR_REGS(&s25fl_4bp_regs),
		  SNOR_WP_RANGES(&s25fl2xk_wpr_4bp),
	),

	SNOR_PART("S25FL016A", SNOR_ID(0x01, 0x02, 0x14), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&s25fl_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("S25FLx16K", SNOR_ID(0x01, 0x40, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(65),
		  SNOR_FIXUPS(&s25flx16k_fixups),
	),

	SNOR_PART("S25FL116K", SNOR_ID(0x01, 0x40, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&s25fl1xk_otp_3),
		  SNOR_REGS(&s25fl1xk_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&s25fl1xk_fixups),
		  SNOR_OPS(&s25fl1xk_part_ops),
	),

	SNOR_PART("S25FL216K", SNOR_ID(0x01, 0x40, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(65),
		  SNOR_REGS(&s25fl_4bp_regs),
		  SNOR_WP_RANGES(&s25fl216k_wpr_4bp),
	),

	SNOR_PART("S25FL032P", SNOR_ID(0x01, 0x02, 0x15, 0x4d), SZ_4M, /* CFI, must come before S25FL032A, Tested */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_ERASE_INFO(&s25flxp_erase_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&s25flxp_regs),
		  SNOR_FIXUPS(&s25flxp_fixups),
	),

	SNOR_PART("S25FL032A", SNOR_ID(0x01, 0x02, 0x15), SZ_4M, /* Tested */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&s25fl_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("S25FL132K", SNOR_ID(0x01, 0x40, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&s25fl1xk_otp_3),
		  SNOR_REGS(&s25fl1xk_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&s25fl1xk_fixups),
		  SNOR_OPS(&s25fl1xk_part_ops),
	),

	SNOR_PART("S25FL064P", SNOR_ID(0x01, 0x02, 0x16, 0x4d), SZ_8M, /* CFI, must come before S25FL064A */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_ERASE_INFO(&s25flxp_erase_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&s25flxp_regs),
		  SNOR_FIXUPS(&s25flxp_fixups),
	),

	SNOR_PART("S25FL064A", SNOR_ID(0x01, 0x02, 0x16), SZ_8M, /* Tested */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&s25fl_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up_ratio),
	),

	SNOR_PART("S25FL164K", SNOR_ID(0x01, 0x40, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&s25fl1xk_otp_3),
		  SNOR_REGS(&s25fl1xk_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_FIXUPS(&s25fl1xk_fixups),
		  SNOR_OPS(&s25fl1xk_part_ops),
	),

	SNOR_PART("S25FL064L", SNOR_ID(0x01, 0x60, 0x17), SZ_8M, /* SFDP 1.6, Tested */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(SP_F_SR2_PE_ERR_BITS | SP_F_DC_CR3_BIT3_0_SET_8),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&s25flxl_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&s25flxl_otp),
	),

	SNOR_PART("S25FS064S", SNOR_ID(0x01, 0x20, 0x17, 0x4d, 0x00, 0x81), SZ_8M, /* SFDP 1.6 */
		  SNOR_ID_MASK(s25fxs_id_mask),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_SPI_MAX_SPEED_MHZ(116),
		  SNOR_REGS(&s25fsxs_regs),
		  SNOR_OTP_INFO(&s25flxs_otp),
		  SNOR_FIXUPS(&s25fsxs_fixups),
		  SNOR_OPS(&s25fsxs_part_ops),
	),

	SNOR_PART("S25FL128P0XxFI01", SNOR_ID(0x01, 0x20, 0x18, 0x03, 0x00), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_256K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&s25fl_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up_ratio),
	),

	SNOR_PART("S25FL128P0XxFI00", SNOR_ID(0x01, 0x20, 0x18, 0x03, 0x01), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&s25fl_4bp_regs),
		  SNOR_WP_RANGES(&s25fl128p00_wpr_4bp_up),
	),

	SNOR_PART("S25FL-S 128Mb, 256KB sector", SNOR_ID(0x01, 0x20, 0x18, 0x4d, 0x00, 0x80), SZ_16M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_256K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_PAGE_SIZE(512),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_FIXUPS(&s25flxs_256k_model_fixups),
	),

	SNOR_PART("S25FL-S 128Mb, hybrid sectors", SNOR_ID(0x01, 0x20, 0x18, 0x4d, 0x01, 0x80), SZ_16M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_FIXUPS(&s25flxs_model_fixups),
	),

	SNOR_PART("S25FL128Sxxxxxx1", SNOR_ID(0x01, 0x20, 0x18, 0x4d, 0x00, 0x80), SZ_16M, /* CFI, must come before S25FL129P0Xxxx01, Tested */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_256K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_PAGE_SIZE(512),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80), /* Special dummy cycles for high performance are not supported */
		  SNOR_REGS(&s25flxs_256k_regs),
		  SNOR_OTP_INFO(&s25flxs_otp),
		  SNOR_OPS(&s25flxs_part_ops),
		  SNOR_FIXUPS(&s25flxs_fixups),
	),

	SNOR_PART("S25FL128Sxxxxxx0", SNOR_ID(0x01, 0x20, 0x18, 0x4d, 0x01, 0x80), SZ_16M, /* CFI, must come before S25FL129P0Xxxx01, Tested */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_ERASE_INFO(&s25flxs_erase_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(80), /* Special dummy cycles for high performance are not supported */
		  SNOR_REGS(&s25flxs_regs),
		  SNOR_OTP_INFO(&s25flxs_otp),
		  SNOR_OPS(&s25flxs_part_ops),
		  SNOR_FIXUPS(&s25flxs_fixups),
	),

	SNOR_PART("S25FS128S", SNOR_ID(0x01, 0x20, 0x18, 0x4d, 0x00, 0x81), SZ_16M, /* SFDP 1.6, must come before S25FL129P0Xxxx01 */
		  SNOR_ID_MASK(s25fxs_id_mask),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&s25fsxs_regs),
		  SNOR_OTP_INFO(&s25flxs_otp),
		  SNOR_FIXUPS(&s25fsxs_fixups),
		  SNOR_OPS(&s25fsxs_part_ops),
	),

	SNOR_PART("S25FL129P0Xxxx01", SNOR_ID(0x01, 0x20, 0x18, 0x4d, 0x00), SZ_16M, /* CFI */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_256K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&s25flxp_256k_regs),
		  SNOR_FIXUPS(&s25flxp_fixups),
	),

	SNOR_PART("S25FL129P0Xxxx00", SNOR_ID(0x01, 0x20, 0x18, 0x4d, 0x01), SZ_16M, /* CFI */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_ERASE_INFO(&s25flxs_erase_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&s25flxp_regs),
		  SNOR_FIXUPS(&s25flxp_fixups),
	),

	SNOR_PART("S25FL127S", SNOR_ID(0x01, 0x20, 0x18, 0x4d), SZ_16M, /* CFI, SFDP 1.6, Tested */
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_SPI_MAX_SPEED_MHZ(80), /* Special dummy cycles for high performance are not supported */
		  SNOR_REGS(&s25fl127s_regs),
		  SNOR_OTP_INFO(&s25flxs_otp),
		  SNOR_OPS(&s25flxs_part_ops),
		  SNOR_FIXUPS(&s25fl127s_fixups),
	),

	SNOR_PART("S25FL128L", SNOR_ID(0x01, 0x60, 0x18), SZ_16M, /* CFI, SFDP 1.6, Tested */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(SP_F_SR2_PE_ERR_BITS | SP_F_DC_CR3_BIT3_0_SET_8),
		  SNOR_SPI_MAX_SPEED_MHZ(108), /* 133MHz not supported now */
		  SNOR_REGS(&s25flxl_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&s25flxl_otp),
	),

	SNOR_PART("S25FL256Sxxxxxx1", SNOR_ID(0x01, 0x02, 0x19, 0x4d, 0x00, 0x80), SZ_32M, /* CFI, Tested */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_256K | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_PAGE_SIZE(512),
		  SNOR_4B_FLAGS(SNOR_4B_F_BANK | SNOR_4B_F_OPCODE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80), /* Special dummy cycles for high performance are not supported */
		  SNOR_REGS(&s25flxs_256k_regs),
		  SNOR_OTP_INFO(&s25flxs_otp),
		  SNOR_OPS(&s25flxs_part_ops),
		  SNOR_FIXUPS(&s25flxs_fixups),
	),

	SNOR_PART("S25FL256Sxxxxxx0", SNOR_ID(0x01, 0x02, 0x19, 0x4d, 0x01, 0x80), SZ_32M, /* CFI, Tested */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE | SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_4B_FLAGS(SNOR_4B_F_BANK | SNOR_4B_F_OPCODE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_ERASE_INFO(&s25flxs_erase_opcodes),
		  SNOR_ERASE_INFO_4B(&s25flxs_erase_4b_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(80), /* Special dummy cycles for high performance are not supported */
		  SNOR_REGS(&s25flxs_regs),
		  SNOR_OTP_INFO(&s25flxs_otp),
		  SNOR_OPS(&s25flxs_part_ops),
		  SNOR_FIXUPS(&s25flxs_fixups),
	),

	SNOR_PART("S25FL256L", SNOR_ID(0x01, 0x60, 0x19), SZ_32M, /* CFI, SFDP 1.6, Tested */
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(SP_F_SR2_PE_ERR_BITS | SP_F_DC_CR3_BIT3_0_SET_8),
		  SNOR_SPI_MAX_SPEED_MHZ(108), /* 133MHz not supported now */
		  SNOR_REGS(&s25fl256l_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
		  SNOR_OTP_INFO(&s25flxl_otp),
	),

	SNOR_PART("S25FS256S", SNOR_ID(0x01, 0x02, 0x19, 0x4d, 0x00, 0x81), SZ_32M, /* SFDP 1.6 */
		  SNOR_ID_MASK(s25fxs_id_mask),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&s25fsxs_regs),
		  SNOR_OTP_INFO(&s25flxs_otp),
		  SNOR_FIXUPS(&s25fsxs_fixups),
		  SNOR_OPS(&s25fsxs_part_ops),
	),

	SNOR_PART("S25FS512S", SNOR_ID(0x01, 0x02, 0x20, 0x4d, 0x00, 0x81), SZ_64M, /* SFDP 1.6, must come before S25FL512S */
		  SNOR_ID_MASK(s25fxs_id_mask),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&s25fsxs_regs),
		  SNOR_OTP_INFO(&s25flxs_otp),
		  SNOR_FIXUPS(&s25fsxs_fixups),
		  SNOR_OPS(&s25fsxs_part_ops),
	),

	SNOR_PART("S25FL512S", SNOR_ID(0x01, 0x02, 0x20, 0x00, 0x00, 0x80), SZ_64M, /* CFI, SFDP 1.6, Tested */
		  SNOR_ID_MASK(s25fxs_id_mask),
		  SNOR_FLAGS(SNOR_F_BYPASS_VENDOR_FIXUPS),
		  SNOR_VENDOR_FLAGS(SP_F_SR_PE_ERR_BITS),
		  SNOR_SPI_MAX_SPEED_MHZ(80), /* Special dummy cycles for high performance are not supported */
		  SNOR_REGS(&s25flxs_256k_regs),
		  SNOR_OTP_INFO(&s25flxs_otp),
		  SNOR_OPS(&s25flxs_part_ops),
		  SNOR_FIXUPS(&s25fl512s_fixups),
	),
};

static ufprog_status spansion_part_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	spi_nor_blank_part_fill_default_opcodes(bp);

	if (!snor->sfdp.bfpt || snor->sfdp.bfpt_hdr->minor_ver < SFDP_REV_MINOR_B)
		return UFP_OK;

	/* Set to a known address mode (3-Byte) */
	STATUS_CHECK_RET(spi_nor_disable_4b_addressing_e9h(snor));
	snor->state.a4b_mode = false;

	bp->p.flags |= SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK;

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

		if (bp->p.read_io_caps & BIT_SPI_MEM_IO_4_4_4) {
			bp->pp_opcodes_4b[SPI_MEM_IO_4_4_4].opcode = SNOR_CMD_4B_PAGE_PROG;
			bp->pp_opcodes_4b[SPI_MEM_IO_4_4_4].ndummy = bp->pp_opcodes_4b[SPI_MEM_IO_4_4_4].nmode = 0;
		}
	}

	if (bp->p.vendor_flags & SP_F_DC_CR3_BIT3_0_SET_8) {
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].nmode = 0;
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].nmode = 0;
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = 4;
		bp->read_opcodes_3b[SPI_MEM_IO_2_2_2].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_2_2_2].nmode = 0;
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].nmode = 0;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 2;
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 2;

		if (bp->p.size >= SZ_32M) {
			bp->read_opcodes_4b[SPI_MEM_IO_1_1_1].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].ndummy;
			bp->read_opcodes_4b[SPI_MEM_IO_1_1_1].nmode = bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].nmode;
			bp->read_opcodes_4b[SPI_MEM_IO_1_1_2].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].ndummy;
			bp->read_opcodes_4b[SPI_MEM_IO_1_1_2].nmode = bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].nmode;
			bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy;
			bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].nmode = bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode;
			bp->read_opcodes_4b[SPI_MEM_IO_2_2_2].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_2_2_2].ndummy;
			bp->read_opcodes_4b[SPI_MEM_IO_2_2_2].nmode = bp->read_opcodes_3b[SPI_MEM_IO_2_2_2].nmode;
			bp->read_opcodes_4b[SPI_MEM_IO_1_1_4].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy;
			bp->read_opcodes_4b[SPI_MEM_IO_1_1_4].nmode = bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].nmode;
			bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy;
			bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].nmode = bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode;
			bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].ndummy = bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy;
			bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].nmode = bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode;
		}
	}

	snor->state.reg.cr = &srcr_acc;
	snor->state.reg.cr_shift = 8;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup spansion_fixups = {
	.pre_param_setup = spansion_part_fixup,
};

static ufprog_status spansion_chip_setup(struct spi_nor *snor)
{
	uint32_t val;

	if (snor->param.vendor_flags & SP_F_DC_CR3_BIT3_0_SET_8) {
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &s25flxl_sr1cr123_acc,
							S25FLxL_SR3_DC_MASK,
							(8 << S25FLxL_SR3_DC_SHIFT) | S25FLxL_SR3_WE, true));

		STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &s25flxl_sr1cr123_acc, &val));

		val = (val & S25FLxL_SR3_DC_MASK) >> S25FLxL_SR3_DC_SHIFT;

		if (val != 8) {
			logm_err("Failed to set read dummy cycles to %u\n", 8);
			return UFP_UNSUPPORTED;
		}
	}

	return UFP_OK;
}

static ufprog_status spansion_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_READ_UNIQUE_ID, 1),
		SPI_MEM_OP_NO_ADDR,
		SPI_MEM_OP_DUMMY(4, 1),
		SPI_MEM_OP_DATA_IN(SPANSION_UID_LEN, data, 1)
	);

	if (retlen)
		*retlen = SPANSION_UID_LEN;

	if (!data)
		return UFP_OK;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static uint32_t spansion_secr_otp_addr(struct spi_nor *snor, uint32_t index, uint32_t addr)
{
	return (index << 8) | addr;
}

static const struct spi_nor_flash_secr_otp_ops spansion_secr_ops = {
	.otp_addr = spansion_secr_otp_addr,
};

static const struct spi_nor_flash_part_otp_ops spansion_secr_otp_ops = {
	.secr = &spansion_secr_ops,

	.read = secr_otp_read,
	.write = secr_otp_write,
	.erase = secr_otp_erase,
	.lock = secr_otp_lock,
	.locked = secr_otp_locked,
};

static const struct spi_nor_flash_part_ops spansion_default_part_ops = {
	.otp = &spansion_secr_otp_ops,

	.chip_setup = spansion_chip_setup,
	.read_uid = spansion_read_uid,
	.qpi_dis = spi_nor_disable_qpi_f5h,
};

const struct spi_nor_vendor vendor_spansion = {
	.mfr_id = SNOR_VENDOR_SPANSION,
	.id = "spansion",
	.name = "Infineon/Cypress/Spansion",
	.parts = spansion_parts,
	.nparts = ARRAY_SIZE(spansion_parts),
	.vendor_flag_names = spansion_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(spansion_vendor_flag_info),
	.default_part_ops = &spansion_default_part_ops,
	.default_part_fixups = &spansion_fixups,
};
