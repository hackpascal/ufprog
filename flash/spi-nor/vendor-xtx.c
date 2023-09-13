// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * XTX SPI-NOR flash parts
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
#include "vendor-winbond.h"

#define XTX_UID_LEN				16

/* QPI Read Parameters */
#define QPI_READ_NO_WRAP			0x04

/* BP Bits */
#define SR_BP3					BIT(5)
#define SR_CMP					BIT(14)

/* BP Masks */
#define BP_1_0					(SR_BP1 | SR_BP0)
#define BP_2_0					(SR_BP2 | SR_BP1 | SR_BP0)
#define BP_3_0_CMP_AS_TB			(SR_CMP | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0)

 /* XTX vendor flags */
#define XTX_F_HPM				BIT(0)
#define XTX_F_WPS_SR3_BIT2			BIT(1)
#define XTX_F_OTP_LOCK_SR1_BIT6			BIT(2)
#define XTX_F_LC_SR3_BIT0			BIT(3)	/* 0 = 4/6 (Default), 1 = 8/10 */
#define XTX_F_LC_SR3_BIT1			BIT(4)	/* DTR: 0 = 8 (Default), 1 = 6 */

static const struct spi_nor_part_flag_enum_info xtx_vendor_flag_info[] = {
	{ 0, "hs-mode" },
	{ 1, "wps-sr3-bit2" },
	{ 2, "otp-lock-sr-bit6" },
	{ 3, "lc-sr3-bit0" },
	{ 4, "lc-sr3-bit1" },
};

static const struct spi_nor_reg_access xtx_srcr_acc = {
	.type = SNOR_REG_NORMAL,
	.num = 2,
	.desc[0] = {
		.read_opcode = SNOR_CMD_READ_SR,
		.write_opcode = SNOR_CMD_WRITE_SR,
		.ndata = 1,
	},
	.desc[1] = {
		.read_opcode = SNOR_CMD_READ_CR,
		.write_opcode = SNOR_CMD_WRITE_CR,
		.ndata = 1,
	},
};

static const struct spi_nor_otp_info xtx_otp_512b = {
	.start_index = 0,
	.count = 1,
	.size = 0x200,
};

static const struct spi_nor_otp_info xtx_otp_2x256b = {
	.start_index = 1,
	.count = 2,
	.size = 0x100,
};

static const struct spi_nor_otp_info xtx_otp_3x256b = {
	.start_index = 1,
	.count = 3,
	.size = 0x100,
};

static const struct spi_nor_otp_info xtx_otp_1k = {
	.start_index = 0,
	.count = 1,
	.size = 0x400,
};

static const struct spi_nor_reg_field_item xtx_2bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
};

static const struct spi_nor_reg_def xtx_2bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, xtx_2bp_sr_fields);

static const struct snor_reg_info xtx_2bp_regs = SNOR_REG_INFO(&xtx_2bp_sr);

static const struct spi_nor_reg_field_item xtx_3bp_srp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def xtx_3bp_srp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc,
								  xtx_3bp_srp_sr_fields);

static const struct snor_reg_info xtx_3bp_srp_regs = SNOR_REG_INFO(&xtx_3bp_srp_sr);

static const struct spi_nor_reg_field_item xtx_3bp_lb_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(6, 1, "LB", "Security Register Lock Bit (OTP)"),
};

static const struct spi_nor_reg_def xtx_3bp_lb_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc,
								 xtx_3bp_lb_sr_fields);

static const struct snor_reg_info xtx_3bp_lb_regs = SNOR_REG_INFO(&xtx_3bp_lb_sr);

static const struct spi_nor_reg_field_item xtx_4bp_srp_qe_lb_cmp_srcr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(10, 1, "LB", "Security Register Lock Bit"),
	SNOR_REG_FIELD(14, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def xtx_4bp_srp_qe_lb_cmp_srcr = SNOR_REG_DEF("SR", "Status Register", &xtx_srcr_acc,
									      xtx_4bp_srp_qe_lb_cmp_srcr_fields);

static const struct snor_reg_info xtx_4bp_srp_qe_lb_cmp_regs = SNOR_REG_INFO(&xtx_4bp_srp_qe_lb_cmp_srcr);

static const struct spi_nor_reg_field_item xtx_5bp_srp_qe_lb_cmp_srcr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "CMP", "Complement Protect"),
	SNOR_REG_FIELD(6, 1, "LB", "Security Register Lock Bit"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(10, 1, "LB", "Security Register Lock Bit"),
	SNOR_REG_FIELD(14, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def xtx_5bp_srp_qe_lb_cmp_srcr = SNOR_REG_DEF("SRCR", "Status & Configuration Register",
									      &xtx_srcr_acc,
									      xtx_5bp_srp_qe_lb_cmp_srcr_fields);

static const struct snor_reg_info xtx_5bp_srp_qe_lb_cmp_regs = SNOR_REG_INFO(&xtx_5bp_srp_qe_lb_cmp_srcr);

static const struct spi_nor_reg_field_item xtx_5bp_srp2_qe_lb_cmp_srcr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "CMP", "Complement Protect"),
	SNOR_REG_FIELD(6, 1, "LB", "Security Register Lock Bit"),
	SNOR_REG_FIELD(7, 1, "SRP0", "Status Register Protect Bit 0"),
	SNOR_REG_FIELD(8, 1, "SRP1", "Status Register Protect Bit 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(10, 1, "LB", "Security Register Lock Bit"),
	SNOR_REG_FIELD(14, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def xtx_5bp_srp2_qe_lb_cmp_srcr = SNOR_REG_DEF("SRCR",
									       "Status & Configuration Register",
									       &xtx_srcr_acc,
									       xtx_5bp_srp2_qe_lb_cmp_srcr_fields);

static const struct snor_reg_info xtx_5bp_srp2_qe_lb_cmp_regs = SNOR_REG_INFO(&xtx_5bp_srp2_qe_lb_cmp_srcr);

static const struct spi_nor_reg_field_item xtx_cr_srp1_qe_lb12_cmp_fields[] = {
	SNOR_REG_FIELD(0, 1, "SRP1", "Status Register Protect Bit 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(1, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(3, 1, "LB1", "Security Register Lock Bit 1"),
	SNOR_REG_FIELD(4, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(6, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def xtx_srp1_qe_lb12_cmp_cr = SNOR_REG_DEF("CR", "Configuration Register", &cr_acc,
									      xtx_cr_srp1_qe_lb12_cmp_fields);

static const struct spi_nor_reg_field_item xtx_lc1_wps_drv56_hold_rst_sr3_fields[] = {
	SNOR_REG_FIELD(1, 1, "LC", "Latency Code"),
	SNOR_REG_FIELD_FULL(2, 1, "WPS", "Write Protection Selection", &w25q_sr3_wps_values),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def xtx_lc1_wps_drv56_hold_rst_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
										  xtx_lc1_wps_drv56_hold_rst_sr3_fields);

static const struct snor_reg_info xtx_sr_cr_sr3_regs = SNOR_REG_INFO(&w25q_sr1, &xtx_srp1_qe_lb12_cmp_cr,
								     &xtx_lc1_wps_drv56_hold_rst_sr3);

static const struct snor_reg_info xtx_sr_sr2_sr3_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2,
								     &xtx_lc1_wps_drv56_hold_rst_sr3);

static const struct spi_nor_reg_field_item xtx_cr_srp1_qe_lb23_cmp_fields[] = {
	SNOR_REG_FIELD(0, 1, "SRP1", "Status Register Protect Bit 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(1, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(4, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(5, 1, "LB3", "Security Register Lock Bit 3"),
	SNOR_REG_FIELD(6, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def xtx_srp1_qe_lb23_cmp_cr = SNOR_REG_DEF("CR", "Configuration Register", &cr_acc,
									   xtx_cr_srp1_qe_lb23_cmp_fields);

static const struct spi_nor_reg_field_item xtx_lc0_drv56_hold_rst_sr3_fields[] = {
	SNOR_REG_FIELD(0, 1, "LC", "Latency Code"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def xtx_lc0_drv56_hold_rst_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
										  xtx_lc0_drv56_hold_rst_sr3_fields);

static const struct snor_reg_info xtx_sr_cr_lb23_sr3_lc0_regs = SNOR_REG_INFO(&w25q_sr1, &xtx_srp1_qe_lb23_cmp_cr,
								     &xtx_lc0_drv56_hold_rst_sr3);

static const struct spi_nor_reg_field_item xtx_lc0_drv56_sr3_fields[] = {
	SNOR_REG_FIELD(0, 1, "LC", "Latency Code"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
};

static const struct spi_nor_reg_def xtx_lc0_drv56_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								     xtx_lc0_drv56_sr3_fields);

static const struct snor_reg_info xtx_sr1_sr2_sr3_lc0_drv_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &xtx_lc0_drv56_sr3);

static const struct spi_nor_reg_field_item xtx_dc01_drv56_hold_rst_sr3_fields[] = {
	SNOR_REG_FIELD(0, 1, "DC0", "Latency Code"),
	SNOR_REG_FIELD(1, 1, "DC1", "Latency Code"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def xtx_dc01_drv56_hold_rst_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
									       xtx_dc01_drv56_hold_rst_sr3_fields);

static const struct snor_reg_info xtx_sr1_sr2_sr3_dc01_drv_hold_rst_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2,
											 &xtx_dc01_drv56_hold_rst_sr3);

static const struct spi_nor_reg_field_item xtx_cr_qe_lb12_wps_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(1, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(3, 1, "LB1", "Security Register Lock Bit 1"),
	SNOR_REG_FIELD(4, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD_FULL(2, 1, "WPS", "Write Protection Selection", &w25q_sr3_wps_values),
};

static const struct spi_nor_reg_def xtx_cr_qe_lb12_wps_cr = SNOR_REG_DEF("CR", "Configuration Register", &cr_acc,
									 xtx_cr_qe_lb12_wps_fields);

static const struct spi_nor_reg_field_item xtx_lc1_adp_drv56_hold_rst_sr3_fields[] = { /* PE/EE */
	SNOR_REG_FIELD(0, 1, "LC", "Latency Code"),
	SNOR_REG_FIELD_FULL(4, 1, "ADP", "Power-up Address Mode", &w25q_sr3_adp_values),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def xtx_lc1_adp_drv56_hold_rst_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
										  xtx_lc1_adp_drv56_hold_rst_sr3_fields);

static const struct snor_reg_info xtx_sr1_cr_sr3_lc1_adp_regs = SNOR_REG_INFO(&w25q_sr1, &xtx_cr_qe_lb12_wps_cr,
									      &xtx_lc1_adp_drv56_hold_rst_sr3);


static const struct spi_nor_wp_info xtx_wpr_2bp = SNOR_WP_BP(&sr_acc,
	SNOR_WP_BP_UP(BP_1_0, 0              , -1),	/* None */
	SNOR_WP_BP_UP(BP_1_0, SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_LO(BP_1_0,          SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(BP_1_0, SR_BP1         , 1),	/* Lower 128KB */
);

static const struct spi_nor_wp_info xtx_wpr_3bp_up = SNOR_WP_BP(&sr_acc,
	SNOR_WP_BP_UP(BP_2_0, 0                       , -1),	/* None */
	SNOR_WP_BP_UP(BP_2_0, SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_UP(BP_2_0,                   SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(BP_2_0,          SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(BP_2_0,          SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(BP_2_0, SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(BP_2_0, SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(BP_2_0, SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
);

static const struct spi_nor_wp_info xtx_wpr_3bp_lo_cmp_sec = SNOR_WP_BP(&sr_acc,
	SNOR_WP_BP_UP(BP_2_0, 0                       , -1),	/* None */
	SNOR_WP_BP_UP(BP_2_0, SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_SP_CMP_LO(BP_2_0,                   SR_BP0, 1),	/* Lower T - 8KB */
	SNOR_WP_SP_CMP_LO(BP_2_0,          SR_BP1         , 2),	/* Lower T - 16KB */
	SNOR_WP_SP_CMP_LO(BP_2_0,          SR_BP1 | SR_BP0, 3),	/* Lower T - 32KB */
	SNOR_WP_SP_CMP_LO(BP_2_0, SR_BP2                  , 4),	/* Lower T - 64KB */
	SNOR_WP_SP_CMP_LO(BP_2_0, SR_BP2 |          SR_BP0, 5),	/* Lower T - 128KB */
	SNOR_WP_SP_CMP_LO(BP_2_0, SR_BP2 | SR_BP1         , 6),	/* Lower T - 256KB */
);

const struct spi_nor_wp_info xtx_wpr_4bp_cmp_as_tb = SNOR_WP_BP(&srcr_acc,
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB, 0                                         , -1),	/* None */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB, SR_CMP                                    , -1),	/* None */

	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,          SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB, SR_CMP | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,                                     SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,                            SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,                            SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,                   SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,                   SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,                   SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,                   SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,          SR_BP3                           , 7),	/* Upper 8MB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,          SR_BP3 |                   SR_BP0, 8),	/* Upper 16MB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,          SR_BP3 |          SR_BP1         , 9),	/* Upper 32MB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,          SR_BP3 |          SR_BP1 | SR_BP0, 10),/* Upper 64MB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,          SR_BP3 | SR_BP2                  , 11),/* Upper 128MB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,          SR_BP3 | SR_BP2 | SR_BP1         , 12),/* Upper 256MB */
	SNOR_WP_BP_UP(BP_3_0_CMP_AS_TB,          SR_BP3 | SR_BP2 | SR_BP1         , 13),/* Upper 512MB */

	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP |                            SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP |                   SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP |                   SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP |          SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP |          SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP |          SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP |          SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Lower 4MB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP | SR_BP3                           , 7),	/* Lower 8MB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP | SR_BP3 |                   SR_BP0, 8),	/* Lower 16MB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP | SR_BP3 |          SR_BP1         , 9),	/* Lower 32MB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP | SR_BP3 |          SR_BP1 | SR_BP0, 10),/* Lower 64MB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP | SR_BP3 | SR_BP2                  , 11),/* Lower 128MB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP | SR_BP3 | SR_BP2 | SR_BP1         , 12),/* Lower 256MB */
	SNOR_WP_BP_LO(BP_3_0_CMP_AS_TB, SR_CMP | SR_BP3 | SR_BP2 | SR_BP1         , 13),/* Lower 512MB */
);

static ufprog_status xf25f04x_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "XT25F04D");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "XT25F04B");
}

static const struct spi_nor_flash_part_fixup xf25f04x_fixups = {
	.pre_param_setup = xf25f04x_fixup_model,
};

static DEFINE_SNOR_ALIAS(xt25q64d_alias, SNOR_ALIAS_MODEL("XT25BQ64D"));
static DEFINE_SNOR_ALIAS(xt25q128d_alias, SNOR_ALIAS_MODEL("XT25BQ128D"));
static DEFINE_SNOR_ALIAS(xt25f256b_alias, SNOR_ALIAS_MODEL("XT25BF256B"));

static struct spi_nor_wp_info *xtx_3bp_tb_sec_cmp, xtx_3bp_tb_sec_cmp_dummy;
static struct spi_nor_wp_info *xtx_3bp_tb_sec_cmp_ratio, xtx_3bp_tb_sec_cmp_ratio_dummy;

static const struct spi_nor_flash_part xtx_parts[] = {

	SNOR_PART("XT25F02E", SNOR_ID(0x0b, 0x40, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(120), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&xtx_2bp_regs),
		  SNOR_WP_RANGES(&xtx_wpr_2bp),
	),

	SNOR_PART("XT25W02E", SNOR_ID(0x0b, 0x60, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(60), SNOR_DUAL_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&xtx_2bp_regs),
		  SNOR_WP_RANGES(&xtx_wpr_2bp),
	),

	SNOR_PART("XT25F04*", SNOR_ID(0x0b, 0x40, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_FIXUPS(&xf25f04x_fixups),
	),

	SNOR_PART("XT25F04B", SNOR_ID(0x0b, 0x40, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE |
			     SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&xtx_3bp_srp_regs),
		  SNOR_WP_RANGES(&xtx_wpr_3bp_up),
	),

	SNOR_PART("XT25F04D", SNOR_ID(0x0b, 0x40, 0x13), SZ_512K, /* SFDP 1.2 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XTX_F_HPM | XTX_F_OTP_LOCK_SR1_BIT6),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(120), SNOR_DUAL_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xtx_3bp_lb_regs),
		  SNOR_WP_RANGES(&xtx_wpr_3bp_lo_cmp_sec),
		  SNOR_OTP_INFO(&xtx_otp_512b),
	),

	SNOR_PART("XT25W04D", SNOR_ID(0x0b, 0x60, 0x13), SZ_512K, /* SFDP 1.2 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XTX_F_HPM | XTX_F_OTP_LOCK_SR1_BIT6),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50), SNOR_DUAL_MAX_SPEED_MHZ(30),
		  SNOR_REGS(&xtx_3bp_lb_regs),
		  SNOR_WP_RANGES(&xtx_wpr_3bp_lo_cmp_sec),
		  SNOR_OTP_INFO(&xtx_otp_512b),
	),

	SNOR_PART("XT25F08B", SNOR_ID(0x0b, 0x40, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&xtx_4bp_srp_qe_lb_cmp_regs),
		  SNOR_WP_RANGES(&xtx_wpr_4bp_cmp_as_tb),
		  SNOR_OTP_INFO(&xtx_otp_1k),
	),

	SNOR_PART("XT25Q08D", SNOR_ID(0x0b, 0x60, 0x14), SZ_1M, /* SFDP 1.6, DTR */
		  SNOR_VENDOR_FLAGS(XTX_F_LC_SR3_BIT1 | XTX_F_WPS_SR3_BIT2),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&xtx_sr_cr_sr3_regs),
		  SNOR_WP_RANGES(&xtx_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&xtx_otp_2x256b), /* DS said 2x1k. Tested to be 2x256b */
	),

	SNOR_PART("XT25F16B", SNOR_ID(0x0b, 0x40, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XTX_F_HPM | XTX_F_OTP_LOCK_SR1_BIT6),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&xtx_5bp_srp_qe_lb_cmp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xtx_otp_1k),
	),

	SNOR_PART("XT25Q16D", SNOR_ID(0x0b, 0x60, 0x15), SZ_2M, /* SFDP 1.6, DTR */
		  SNOR_VENDOR_FLAGS(XTX_F_LC_SR3_BIT1 | XTX_F_WPS_SR3_BIT2),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&xtx_sr_cr_sr3_regs),
		  SNOR_WP_RANGES(&xtx_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&xtx_otp_2x256b), /* DS said 2x1k. Tested to be 2x256b */
	),

	SNOR_PART("XT25F32*", SNOR_ID(0x0b, 0x40, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(60),
	),

	SNOR_PART("XT25F32B", SNOR_ID(0x0b, 0x40, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(72),
		  SNOR_REGS(&xtx_5bp_srp2_qe_lb_cmp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xtx_otp_1k),
	),

	SNOR_PART("XT25F32F", SNOR_ID(0x0b, 0x40, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XTX_F_HPM | XTX_F_LC_SR3_BIT0),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xtx_sr_cr_lb23_sr3_lc0_regs),
		  SNOR_WP_RANGES(&xtx_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&xtx_otp_2x256b), /* DS said 2x1k. Tested to be 2x256b */
	),

	SNOR_PART("XT25F64*", SNOR_ID(0x0b, 0x40, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(60),
	),

	SNOR_PART("XT25F64B", SNOR_ID(0x0b, 0x40, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(72),
		  SNOR_REGS(&xtx_5bp_srp2_qe_lb_cmp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&xtx_otp_1k),
	),

	SNOR_PART("XT25F64F", SNOR_ID(0x0b, 0x40, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XTX_F_HPM | XTX_F_LC_SR3_BIT0),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xtx_sr1_sr2_sr3_lc0_drv_regs),
		  SNOR_WP_RANGES(&xtx_3bp_tb_sec_cmp_ratio_dummy),
		  SNOR_OTP_INFO(&xtx_otp_3x256b), /* DS said 3x1k. Tested to be 3x256b */
	),

	SNOR_PART("XT25Q64D", SNOR_ID(0x0b, 0x60, 0x17), SZ_8M, /* SFDP 1.6, DTR */
		  SNOR_ALIAS(&xt25q64d_alias),
		  SNOR_VENDOR_FLAGS(XTX_F_LC_SR3_BIT1 | XTX_F_WPS_SR3_BIT2),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&xtx_sr_sr2_sr3_regs),
		  SNOR_WP_RANGES(&xtx_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&xtx_otp_3x256b), /* DS said 3x1k. Tested to be 3x256b */
	),

	SNOR_PART("XT25F128*", SNOR_ID(0x0b, 0x40, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(60),
		  SNOR_WP_RANGES(&xtx_3bp_tb_sec_cmp_ratio_dummy),
	),

	SNOR_PART("XT25F128B", SNOR_ID(0x0b, 0x40, 0x18), SZ_16M, /* SFDP 1.? */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(72),
		  SNOR_REGS(&xtx_5bp_srp2_qe_lb_cmp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&xtx_otp_1k),
	),

	SNOR_PART("XT25F128F", SNOR_ID(0x0b, 0x40, 0x18), SZ_16M, /* SFDP 1.? */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XTX_F_HPM | XTX_F_LC_SR3_BIT0 | XTX_F_LC_SR3_BIT1),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xtx_sr1_sr2_sr3_dc01_drv_hold_rst_regs),
		  SNOR_WP_RANGES(&xtx_3bp_tb_sec_cmp_ratio_dummy),
		  SNOR_OTP_INFO(&xtx_otp_3x256b), /* DS said 3x1k. Tested to be 3x256b */
	),

	SNOR_PART("XT25Q128D", SNOR_ID(0x0b, 0x60, 0x18), SZ_16M, /* SFDP 1.6, DTR */
		  SNOR_ALIAS(&xt25q128d_alias),
		  SNOR_VENDOR_FLAGS(XTX_F_LC_SR3_BIT1 | XTX_F_WPS_SR3_BIT2),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(76), SNOR_QUAD_MAX_SPEED_MHZ(76),
		  SNOR_REGS(&xtx_sr_sr2_sr3_regs),
		  SNOR_WP_RANGES(&xtx_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&xtx_otp_3x256b), /* DS said 3x1k. Tested to be 3x256b */
	),

	SNOR_PART("XT25F256B", SNOR_ID(0x0b, 0x40, 0x19), SZ_32M, /* SFDP 1.6 */
		  SNOR_ALIAS(&xt25f256b_alias),
		  SNOR_SPI_MAX_SPEED_MHZ(120), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&xtx_sr1_cr_sr3_lc1_adp_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&xtx_otp_2x256b), /* DS said 2x1k. Tested to be 2x256b */
	),
};

static ufprog_status xtx_part_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				    struct spi_nor_flash_part_blank *bp)
{
	uint8_t sr3;

	spi_nor_blank_part_fill_default_opcodes(bp);

	/* Some detected SFDP 1.1 are actually 1.6. So check the num of DWORDs instead */
	if (snor->sfdp.bfpt && snor->sfdp.bfpt_dw_num >= 16) {
		bp->p.pp_io_caps |= BIT_SPI_MEM_IO_1_1_4;
		bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].opcode = SNOR_CMD_PAGE_PROG_QUAD_IN;
		bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].nmode = 0;

		if (bp->p.read_io_caps & BIT_SPI_MEM_IO_4_4_4) {
			bp->p.pp_io_caps |= BIT_SPI_MEM_IO_4_4_4;
			bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].opcode = SNOR_CMD_PAGE_PROG;
			bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;
		}

		if (bp->p.size >= SZ_32M) {
			bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].opcode = SNOR_CMD_PAGE_PROG_QUAD_IN;
			bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].ndummy = bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].nmode = 0;

			if (bp->p.read_io_caps & BIT_SPI_MEM_IO_4_4_4) {
				bp->pp_opcodes_4b[SPI_MEM_IO_4_4_4].opcode = SNOR_CMD_4B_PAGE_PROG;
				bp->pp_opcodes_4b[SPI_MEM_IO_4_4_4].ndummy = 0;
				bp->pp_opcodes_4b[SPI_MEM_IO_4_4_4].nmode = 0;
			}
		}
	}

	if (bp->p.pp_io_caps & BIT_SPI_MEM_IO_1_4_4) {
		bp->pp_opcodes_3b[SPI_MEM_IO_1_4_4].opcode = SNOR_CMD_PAGE_PROG_QUAD_IO;
		bp->pp_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;

		if (bp->p.size >= SZ_32M) {
			bp->pp_opcodes_4b[SPI_MEM_IO_1_4_4].opcode = SNOR_CMD_4B_PAGE_PROG_QUAD_IO;
			bp->pp_opcodes_4b[SPI_MEM_IO_1_4_4].ndummy = bp->pp_opcodes_4b[SPI_MEM_IO_1_4_4].nmode = 0;
		}
	}

	/* 8 dummy cycles will be used for QPI read */
	if (bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].opcode) {
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;

		if (bp->p.size >= SZ_32M) {
			bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].ndummy = 8;
			bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].nmode = 0;
		}
	}

	if (bp->p.vendor_flags & XTX_F_LC_SR3_BIT0) {
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = 0;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = 10;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;

		if (bp->p.size >= SZ_32M) {
			bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].ndummy = 8;
			bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].nmode = 0;
			bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].ndummy = 10;
			bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].nmode = 0;
		}
	}

	if (bp->p.vendor_flags & XTX_F_WPS_SR3_BIT2) {
		STATUS_CHECK_RET(spi_nor_read_reg(snor, SNOR_CMD_READ_SR3, &sr3));

		if (sr3 & SR3_WPS)
			bp->p.flags |= SNOR_F_GLOBAL_UNLOCK;
		else
			bp->p.flags &= ~SNOR_F_GLOBAL_UNLOCK;
	}

	if (bp->p.wp_ranges == &xtx_3bp_tb_sec_cmp_dummy)
		bp->p.wp_ranges = xtx_3bp_tb_sec_cmp;
	else if (bp->p.wp_ranges == &xtx_3bp_tb_sec_cmp_ratio_dummy)
			bp->p.wp_ranges = xtx_3bp_tb_sec_cmp_ratio;

	return UFP_OK;
}

static ufprog_status xtx_enter_hpm_pre_read(struct spi_nor *snor, uint64_t addr, size_t len, void *data)
{
	ufprog_status ret;

	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_GD_HPM, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_ADDR(3, 0, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_NO_DATA
	);

	ret = ufprog_spi_mem_exec_op(snor->spi, &op);
	if (ret)
		logm_err("Failed to enter high-performance mode\n");

	return ret;
}

static ufprog_status xtx_otp_lock_sr1_bit6_bit(struct spi_nor *snor, uint32_t index, uint32_t *retbit,
					       const struct spi_nor_reg_access **retacc)
{
	*(retacc) = &sr_acc;
	*retbit = 6;

	return UFP_OK;
}

static const struct spi_nor_flash_secr_otp_ops xtx_otp_lock_sr1_bit6_ops = {
	.otp_lock_bit = xtx_otp_lock_sr1_bit6_bit,
};

static const struct spi_nor_flash_part_otp_ops xtx_otp_lock_sr1_bit6_otp_ops = {
	.read = secr_otp_read_paged,
	.write = secr_otp_write_paged,
	.erase = secr_otp_erase,
	.lock = secr_otp_lock,
	.locked = secr_otp_locked,
	.secr = &xtx_otp_lock_sr1_bit6_ops,
};

static ufprog_status xtx_setup_qpi(struct spi_nor *snor, bool enabled)
{
	if (enabled) {
		/* Set QPI read dummy cycles to 8 for maximum speed */
		return spi_nor_write_reg(snor, SNOR_CMD_SET_READ_PARAMETERS,
					 QPI_READ_DUMMY_CLOCKS_8 | QPI_READ_NO_WRAP);
	}

	return UFP_OK;
}

static ufprog_status xtx_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_READ_UNIQUE_ID, 1),
		SPI_MEM_OP_NO_ADDR,
		SPI_MEM_OP_DUMMY(snor->state.a4b_mode ? 5 : 4, 1),
		SPI_MEM_OP_DATA_IN(XTX_UID_LEN, data, 1)
	);

	if (retlen)
		*retlen = XTX_UID_LEN;

	if (!data)
		return UFP_OK;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_status xtx_part_set_ops(struct spi_nor *snor)
{
	if (snor->param.vendor_flags & XTX_F_HPM)
		snor->ext_param.pre_read_hook = xtx_enter_hpm_pre_read;

	if (snor->param.vendor_flags & XTX_F_OTP_LOCK_SR1_BIT6)
		snor->ext_param.ops.otp = &xtx_otp_lock_sr1_bit6_otp_ops;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup xtx_fixups = {
	.pre_param_setup = xtx_part_fixup,
	.pre_chip_setup = xtx_part_set_ops,
};

static ufprog_status xtx_chip_setup(struct spi_nor *snor)
{
	if (snor->param.vendor_flags & XTX_F_LC_SR3_BIT0)
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &sr3_acc, 0, 1, false));

	return UFP_OK;
}

static const struct spi_nor_flash_part_ops xtx_default_part_ops = {
	.otp = &secr_otp_ops,

	.chip_setup = xtx_chip_setup,
	.setup_qpi = xtx_setup_qpi,
	.qpi_dis = spi_nor_disable_qpi_ffh,
	.read_uid = xtx_read_uid,
};

static ufprog_status xtx_init(void)
{
	xtx_3bp_tb_sec_cmp = wp_bp_info_copy(&wpr_3bp_tb_sec_cmp);
	if (!xtx_3bp_tb_sec_cmp)
		return UFP_NOMEM;

	xtx_3bp_tb_sec_cmp->access = &xtx_srcr_acc;

	xtx_3bp_tb_sec_cmp_ratio = wp_bp_info_copy(&wpr_3bp_tb_sec_cmp_ratio);
	if (!xtx_3bp_tb_sec_cmp_ratio) {
		free(xtx_3bp_tb_sec_cmp);
		return UFP_NOMEM;
	}

	xtx_3bp_tb_sec_cmp_ratio->access = &xtx_srcr_acc;

	return UFP_OK;
}

static const struct spi_nor_vendor_ops xtx_ops = {
	.init = xtx_init,
};

const struct spi_nor_vendor vendor_xtx = {
	.mfr_id = SNOR_VENDOR_XTX,
	.id = "xtx",
	.name = "XTX",
	.parts = xtx_parts,
	.nparts = ARRAY_SIZE(xtx_parts),
	.ops = &xtx_ops,
	.default_part_ops = &xtx_default_part_ops,
	.default_part_fixups = &xtx_fixups,
	.vendor_flag_names = xtx_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(xtx_vendor_flag_info),
};
