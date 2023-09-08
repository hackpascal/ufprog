// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * GigaDevice SPI-NOR flash parts
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
#include "ext_id.h"

#define GD_UID_LEN				16
#define GD25Q256C_UID_LEN			8

 /* QPI Read Parameters */
#define QPI_READ_DUMMY_CLOCKS_4			0x00
#define QPI_READ_DUMMY_CLOCKS_4_6		0x10
#define QPI_READ_DUMMY_CLOCKS_6_8		0x20
#define QPI_READ_DUMMY_CLOCKS_8_10		0x30

#define QPI_READ_WRAP_LENGTH_8			0x00
#define QPI_READ_WRAP_LENGTH_16			0x01
#define QPI_READ_WRAP_LENGTH_32			0x02
#define QPI_READ_WRAP_LENGTH_64			0x03

/* OTP lock bit */
#define GD_OTP_LOCK_BIT				6

/* Block-protection bits */
#define SR_TB					BIT(5)

/* >= 256Mbit */
#define SR_BP3					BIT(5)
#define SR_TB11					BIT(11)

#define BP_2_0					(SR_BP2 | SR_BP1 | SR_BP0)
#define BP_2_0_TB				(SR_TB | SR_BP2 | SR_BP1 | SR_BP0)
#define BP_3_0_TB				(SR_TB11 | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0)

/* GigaDevice vendor flags */
#define GD_F_OTP_1				BIT(0)
#define GD_F_QPI_DUMMY_10			BIT(1)
#define GD_F_QPI_4B_OPCODE			BIT(2)
#define GD_F_HPM				BIT(3)
#define GD_F_DC_SRCR_BIT12			BIT(4)
#define GD_F_DC_SR3_BIT0			BIT(5)
#define GD_F_DC_SR3_BIT0_1			BIT(6)
#define GD_F_DC_NVCR1				BIT(7)
#define GD_F_WPS_SR3_BIT2			BIT(8)
#define GD_F_WPS_SR3_BIT7			BIT(9)
#define GD_F_WPS_NVCR4_BIT2			BIT(10)
#define GD_F_ECC_NVCR4_BIT0_1			BIT(11)
#define GD_F_CRC_NVCR4_BIT4_5			BIT(12)
#define GD_F_OTP_LOCK_NVCR2_BIT1		BIT(13)
#define GD_F_OTP_LOCK_CR_BIT3			BIT(14)
#define GD_F_IOM_NVCR0				BIT(15)

static const struct spi_nor_part_flag_enum_info gigadevice_vendor_flag_info[] = {
	{ 0, "otp-1" },
	{ 1, "qpi-dummy-10" },
	{ 2, "qpi-4b-opcode" },
	{ 3, "hpm" },
	{ 4, "dc-srcr-bit12" },
	{ 5, "dc-sr3-bit0" },
	{ 6, "dc-sr3-bit0-1" },
	{ 7, "dc-nvcr1" },
	{ 8, "wps-sr3-bit2" },
	{ 9, "wps-sr3-bit7" },
	{ 10, "wps-nvcr4-bit2" },
	{ 11, "ecc-nvcr4-bit0-1" },
	{ 12, "crc-nvcr4-bit4-5" },
	{ 13, "otp-lock-nvcr2-bit1" },
	{ 14, "otp-lock-cr-bit3" },
	{ 15, "iom-nvcr0" },
};

#define GD_REG_ACC_NVCR(_addr)											\
	{ .type = SNOR_REG_NORMAL, .num = 1,									\
	  .desc[0] = { .read_opcode = SNOR_CMD_READ_NVCR, .write_opcode = SNOR_CMD_WRITE_NVCR,			\
		       .ndata = 1, .addr = (_addr), .ndummy_read = 1, .flags = SNOR_REGACC_F_ADDR_4B_MODE, }	\
	}

#define GD_REG_ACC_VCR(_addr)											\
	{ .type = SNOR_REG_NORMAL, .num = 1,									\
	  .desc[0] = { .read_opcode = SNOR_CMD_READ_VCR, .write_opcode = SNOR_CMD_WRITE_VCR,			\
		       .ndata = 1, .addr = (_addr), .ndummy_read = 1, .flags = SNOR_REGACC_F_ADDR_4B_MODE, }	\
	}

static const struct spi_nor_reg_access gd_nvcr_0_acc = GD_REG_ACC_NVCR(0);
static const struct spi_nor_reg_access gd_nvcr_1_acc = GD_REG_ACC_NVCR(1);
static const struct spi_nor_reg_access gd_nvcr_2_acc = GD_REG_ACC_NVCR(2);
static const struct spi_nor_reg_access gd_nvcr_3_acc = GD_REG_ACC_NVCR(3);
static const struct spi_nor_reg_access gd_nvcr_4_acc = GD_REG_ACC_NVCR(4);
static const struct spi_nor_reg_access gd_nvcr_5_acc = GD_REG_ACC_NVCR(5);
static const struct spi_nor_reg_access gd_nvcr_6_acc = GD_REG_ACC_NVCR(6);
static const struct spi_nor_reg_access gd_nvcr_7_acc = GD_REG_ACC_NVCR(7);
static const struct spi_nor_reg_access gd_vcr_0_acc = GD_REG_ACC_VCR(0);
static const struct spi_nor_reg_access gd_vcr_1_acc = GD_REG_ACC_VCR(1);
static const struct spi_nor_reg_access gd_vcr_4_acc = GD_REG_ACC_VCR(4);

static const struct spi_nor_reg_access gd_srcr_acc = {
	.type = SNOR_REG_NORMAL,
	.num = 2,
	.desc[0] = {
	.read_opcode = SNOR_CMD_READ_SR,
	.write_opcode = SNOR_CMD_WRITE_SR,
	},
	.desc[0] = {
		.read_opcode = SNOR_CMD_READ_CR,
		.write_opcode = SNOR_CMD_WRITE_CR,
	},
};

static const struct spi_nor_reg_field_item gd25dxc_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def gd25dxc_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, gd25dxc_sr_fields);

static const struct snor_reg_info gd25dxc_regs = SNOR_REG_INFO(&gd25dxc_sr);

static const struct spi_nor_reg_field_item gd25dxe_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "CMP", "Complement Protect"),
	SNOR_REG_FIELD(6, 1, "LB", "Security Register Lock Bit"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def gd25dxe_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, gd25dxe_sr_fields);

static const struct snor_reg_info gd25dxe_regs = SNOR_REG_INFO(&gd25dxe_sr);

static const struct spi_nor_reg_field_item gd25qxb_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(6, 1, "SEC", "Sector Protect"),
	SNOR_REG_FIELD(7, 1, "SRP0", "Status Register Protect 0"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(14, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def gd25qxb_sr = SNOR_REG_DEF("SR", "Status Register", &srcr_acc, gd25qxb_sr_fields);

static const struct snor_reg_info gd25qxb_regs = SNOR_REG_INFO(&gd25qxb_sr);

static const struct spi_nor_reg_field_item gd25qxb_lb_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(6, 1, "SEC", "Sector Protect"),
	SNOR_REG_FIELD(7, 1, "SRP0", "Status Register Protect 0"),
	SNOR_REG_FIELD(8, 1, "SRP1", "Status Register Protect 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(10, 1, "LB", "Security Register Lock Bit"),
	SNOR_REG_FIELD(14, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def gd25qxb_lb_sr = SNOR_REG_DEF("SR", "Status Register", &srcr_acc, gd25qxb_lb_sr_fields);

static const struct snor_reg_info gd25qxb_lb_regs = SNOR_REG_INFO(&gd25qxb_lb_sr);

static const struct spi_nor_reg_field_item gd25qxc_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(6, 1, "SEC", "Sector Protect"),
	SNOR_REG_FIELD(7, 1, "SRP0", "Status Register Protect 0"),
	SNOR_REG_FIELD(8, 1, "SRP1", "Status Register Protect 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(10, 1, "LB", "Security Register Lock Bit"),
	SNOR_REG_FIELD(13, 1, "HPF", "High Performance Flag"),
	SNOR_REG_FIELD(14, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def gd25qxc_sr = SNOR_REG_DEF("SR", "Status Register", &srcr_acc, gd25qxc_sr_fields);

static const struct snor_reg_info gd25qxc_regs = SNOR_REG_INFO(&gd25qxc_sr);

static const struct spi_nor_reg_field_item gd25qxe_sr_fields[] = {
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
	SNOR_REG_FIELD(12, 1, "DC", "Dummy Configuration"),
	SNOR_REG_FIELD(14, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def gd25qxe_sr = SNOR_REG_DEF("SR", "Status Register", &srcr_acc, gd25qxe_sr_fields);

static const struct snor_reg_info gd25qxe_regs = SNOR_REG_INFO(&gd25qxe_sr);

static const struct spi_nor_reg_field_item gd25qxc_sr3_fields[] = {
	SNOR_REG_FIELD(4, 1, "HPF", "High Performance Flag"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
};

static const struct spi_nor_reg_def gd25qxc_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
							       gd25qxc_sr3_fields);

static const struct snor_reg_info gd25qxc_3_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &gd25qxc_sr3);

static const struct spi_nor_reg_field_item gd25qxe_sr3_fields[] = {
	SNOR_REG_FIELD(0, 1, "DC", "Dummy Configuration"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
};

static const struct spi_nor_reg_def gd25qxe_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
							       gd25qxe_sr3_fields);

static const struct snor_reg_info gd25qxe_3_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &gd25qxe_sr3);

static const struct spi_nor_reg_field_item gd25b127d_sr3_fields[] = {
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
};

static const struct spi_nor_reg_def gd25b127d_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								 gd25b127d_sr3_fields);

static const struct snor_reg_info gd25b127d_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &gd25b127d_sr3);

static const struct spi_nor_reg_field_item gd25q_lpe_sr3_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(2, 1, "LPE", "Low Power Enable"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def gd25q127c_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								 gd25q_lpe_sr3_fields);

static const struct snor_reg_info gd25q127c_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &gd25q127c_sr3);

static const struct snor_reg_info gd25q128c_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &w25q_sr3);

static const struct spi_nor_reg_field_item gd25qxe_hold_rst_sr3_fields[] = {
	SNOR_REG_FIELD(0, 1, "DC", "Dummy Configuration"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def gd25q128e_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								 gd25qxe_hold_rst_sr3_fields);

static const struct snor_reg_info gd25q128e_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &gd25q128e_sr3);

static const struct spi_nor_reg_field_item gd25qxe_2_dc_hold_rst_sr3_fields[] = {
	SNOR_REG_FIELD(0, 3, "DC", "Dummy Configuration"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def gd25le128e_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								  gd25qxe_2_dc_hold_rst_sr3_fields);

static const struct snor_reg_info gd25le128e_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &gd25le128e_sr3);

static const struct spi_nor_reg_field_item gd25lfxe_2_dc_dlp_sr3_fields[] = {
	SNOR_REG_FIELD(0, 3, "DC", "Dummy Configuration"),
	SNOR_REG_FIELD_ENABLED_DISABLED(4, 1, "DLP", "Data Learning Pattern"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
};

static const struct spi_nor_reg_def gd25lf128e_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								  gd25lfxe_2_dc_dlp_sr3_fields);

static const struct snor_reg_info gd25lf128e_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &gd25lf128e_sr3);

static const struct spi_nor_reg_field_item gd25q256c_sr1_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD_ENABLED_DISABLED(6, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def gd25q256c_sr1 = SNOR_REG_DEF("SR1", "Status Register 1", &sr_acc,
								 gd25q256c_sr1_fields);

static const struct spi_nor_reg_field_item gd25q256c_sr2_fields[] = {
	SNOR_REG_FIELD_FULL(0, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(2, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
	SNOR_REG_FIELD(3, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD_FULL(4, 1, "ADP", "Power-up Address Mode", &w25q_sr3_adp_values),
	SNOR_REG_FIELD(6, 3, "LC", "Latency Code"),
};

static const struct spi_nor_reg_def gd25q256c_sr2 = SNOR_REG_DEF("SR2", "Status Register 2", &cr_acc,
								 gd25q256c_sr2_fields);

static const struct spi_nor_reg_field_item gd25q256c_sr3_fields[] = {
	SNOR_REG_FIELD(0, 1, "LB1", "Security Register Lock Bit 1"),
	SNOR_REG_FIELD(1, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(4, 1, "LB3", "Security Register Lock Bit 3"),
	SNOR_REG_FIELD_FULL(7, 1, "WPS", "Write Protection Selection", &w25q_sr3_wps_values),
};

static const struct spi_nor_reg_def gd25q256c_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								 gd25q256c_sr3_fields);

static const struct snor_reg_info gd25q256c_regs = SNOR_REG_INFO(&gd25q256c_sr1, &gd25q256c_sr2, &gd25q256c_sr3);

static const struct spi_nor_reg_field_item gd25b256d_sr1_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(6, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(7, 1, "SRP0", "Status Register Protect Bit 0"),
};

static const struct spi_nor_reg_def gd25b256d_sr1 = SNOR_REG_DEF("SR1", "Status Register 1", &sr_acc,
								 gd25b256d_sr1_fields);

static const struct spi_nor_reg_field_item gd25b256d_sr2_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(1, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(3, 1, "LB1", "Security Register Lock Bit 1"),
	SNOR_REG_FIELD(4, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(5, 1, "LB3", "Security Register Lock Bit 3"),
	SNOR_REG_FIELD(6, 1, "SRP1", "Status Register Protect Bit 1"),
};

static const struct spi_nor_reg_def gd25b256d_sr2 = SNOR_REG_DEF("SR2", "Status Register 2", &cr_acc,
								 gd25b256d_sr2_fields);

static const struct spi_nor_reg_field_item gd25b256d_sr3_fields[] = {
	SNOR_REG_FIELD_FULL(4, 1, "ADP", "Power-up Address Mode", &w25q_sr3_adp_values),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def gd25b256d_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								 gd25b256d_sr3_fields);

static const struct snor_reg_info gd25b256d_regs = SNOR_REG_INFO(&gd25b256d_sr1, &gd25b256d_sr2, &gd25b256d_sr3);

static const struct spi_nor_reg_field_item gd25b256e_sr3_fields[] = {
	SNOR_REG_FIELD(0, 3, "DC", "Dummy Configuration"),
	SNOR_REG_FIELD_FULL(4, 1, "ADP", "Power-up Address Mode", &w25q_sr3_adp_values),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def gd25b256e_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								 gd25b256e_sr3_fields);

static const struct snor_reg_info gd25b256e_regs = SNOR_REG_INFO(&gd25b256d_sr1, &gd25b256d_sr2, &gd25b256e_sr3);

static const struct spi_nor_reg_field_item gd25b257d_sr1_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(6, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def gd25b257d_sr1 = SNOR_REG_DEF("SR1", "Status Register 1", &sr_acc,
								 gd25b257d_sr1_fields);

static const struct spi_nor_reg_field_item gd25b257d_sr2_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(1, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(3, 1, "LB1", "Security Register Lock Bit 1"),
	SNOR_REG_FIELD(4, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(5, 1, "LB3", "Security Register Lock Bit 3"),
	SNOR_REG_FIELD_ENABLED_DISABLED(6, 1, "ECC", "ECC Enable"),
};

static const struct spi_nor_reg_def gd25b257d_sr2 = SNOR_REG_DEF("SR2", "Status Register 2", &cr_acc,
								 gd25b257d_sr2_fields);

static const struct spi_nor_reg_field_item gd25b257d_sr3_fields[] = {
	SNOR_REG_FIELD(0, 3, "LC", "Latency Code"),
	SNOR_REG_FIELD_FULL(4, 1, "ADP", "Power-up Address Mode", &w25q_sr3_adp_values),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def gd25b257d_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								 gd25b257d_sr3_fields);

static const struct snor_reg_info gd25b257d_regs = SNOR_REG_INFO(&gd25b257d_sr1, &gd25b257d_sr2, &gd25b257d_sr3);

static const struct spi_nor_reg_field_item gd25lb256d_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(6, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(7, 1, "SRP0", "Status Register Protect 0"),
	SNOR_REG_FIELD(8, 1, "SRP1", "Status Register Protect 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(12, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(13, 1, "LB3", "Security Register Lock Bit 3"),
	SNOR_REG_FIELD(14, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def gd25lb256d_sr = SNOR_REG_DEF("SR", "Status Register", &srcr_acc,
								 gd25lb256d_sr_fields);

static const struct snor_reg_info gd25lb256d_regs = SNOR_REG_INFO(&gd25lb256d_sr);

static const struct spi_nor_reg_field_item gd25lb256e_nvcr_1_fields[] = {
	SNOR_REG_FIELD(0, 0xff, "DC", "Dummy cycles"),
};

static const struct spi_nor_reg_def gd25lb256e_nvcr_1 = SNOR_REG_DEF("NVCR1", "Non-volatile Status Register 1",
								     &gd_nvcr_1_acc, gd25lb256e_nvcr_1_fields);

static const struct spi_nor_reg_field_item gd25lb256e_nvcr_2_fields[] = {
	SNOR_REG_FIELD(0, 1, "LB", "Security Register Lock"),
	SNOR_REG_FIELD(4, 1, "SRP1", "Status Register Protect 1"),
};

static const struct spi_nor_reg_def gd25lb256e_nvcr_2 = SNOR_REG_DEF("NVCR2", "Non-volatile Status Register 2",
								     &gd_nvcr_2_acc, gd25lb256e_nvcr_2_fields);

static const struct spi_nor_reg_field_values gd_nvcr_3_drv_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "18 Ohm"),
	VALUE_ITEM(1, "25 Ohm"),
	VALUE_ITEM(2, "35 Ohm"),
	VALUE_ITEM(3, "50 Ohm"),
);

static const struct spi_nor_reg_field_item gd25lb256e_nvcr_3_fields[] = {
	SNOR_REG_FIELD_FULL(0, 3, "DRV", "Driver Stringth", &gd_nvcr_3_drv_values),
};

static const struct spi_nor_reg_def gd25lb256e_nvcr_3 = SNOR_REG_DEF("NVCR3", "Non-volatile Status Register 3",
								     &gd_nvcr_3_acc, gd25lb256e_nvcr_3_fields);

static const struct spi_nor_reg_field_values gd_nvcr_4_wps_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "Individual Block Lock Bits"),
	VALUE_ITEM(1, "Legacy BP Bits"),
);

static const struct spi_nor_reg_field_values gd_nvcr_4_odt_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "50 Ohm ODT"),
	VALUE_ITEM(1, "100 Ohm ODT"),
	VALUE_ITEM(2, "150 Ohm ODT"),
	VALUE_ITEM(3, "ODT Disabled"),
);

static const struct spi_nor_reg_field_item gd25lb256e_nvcr_4_fields[] = {
	SNOR_REG_FIELD_FULL(2, 1, "WPS", "Write Protection Selection", &gd_nvcr_4_wps_values),
	SNOR_REG_FIELD_ENABLED_DISABLED(3, 1, "DLP", "Data Learning Pattern"),
	SNOR_REG_FIELD_FULL(4, 3, "ODT", "On Die Termination", &gd_nvcr_4_odt_values),
};

static const struct spi_nor_reg_def gd25lb256e_nvcr_4 = SNOR_REG_DEF("NVCR4", "Non-volatile Status Register 4",
								     &gd_nvcr_4_acc, gd25lb256e_nvcr_4_fields);

static const struct spi_nor_reg_field_values gd_nvcr_5_adp_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "4-Byte Address Mode"),
	VALUE_ITEM(1, "3-Byte Address Mode"),
);

static const struct spi_nor_reg_field_item gd25lb256e_nvcr_5_fields[] = {
	SNOR_REG_FIELD_FULL(0, 1, "ADP", "Power-up Address Mode", &gd_nvcr_5_adp_values),
};

static const struct spi_nor_reg_def gd25lb256e_nvcr_5 = SNOR_REG_DEF("NVCR5", "Non-volatile Status Register 5",
								     &gd_nvcr_5_acc, gd25lb256e_nvcr_5_fields);

static const struct spi_nor_reg_field_values gd_nvcr_6_xip_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "XIP Enabled"),
	VALUE_ITEM(1, "XIP Disabled"),
);

static const struct spi_nor_reg_field_item gd25lb256e_nvcr_6_fields[] = {
	SNOR_REG_FIELD_FULL(0, 1, "XIP", "XIP configuration", &gd_nvcr_6_xip_values),
};

static const struct spi_nor_reg_def gd25lb256e_nvcr_6 = SNOR_REG_DEF("NVCR6", "Non-volatile Status Register 6",
								     &gd_nvcr_6_acc, gd25lb256e_nvcr_6_fields);

static const struct spi_nor_reg_field_values gd_nvcr_7_wrap_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "16-Byte"),
	VALUE_ITEM(1, "32-Byte"),
	VALUE_ITEM(2, "64-Byte"),
	VALUE_ITEM(3, "Disabled"),
);

static const struct spi_nor_reg_field_item gd25lb256e_nvcr_7_fields[] = {
	SNOR_REG_FIELD_FULL(0, 2, "WRAP", "Wrap configuration", &gd_nvcr_7_wrap_values),
};

static const struct spi_nor_reg_def gd25lb256e_nvcr_7 = SNOR_REG_DEF("NVCR7", "Non-volatile Status Register 7",
								     &gd_nvcr_7_acc, gd25lb256e_nvcr_7_fields);

static const struct snor_reg_info gd25lb256e_regs = SNOR_REG_INFO(&gd25b256d_sr1, &gd25lb256e_nvcr_1,
								  &gd25lb256e_nvcr_2, &gd25lb256e_nvcr_3,
								  &gd25lb256e_nvcr_4, &gd25lb256e_nvcr_5,
								  &gd25lb256e_nvcr_6, &gd25lb256e_nvcr_7);

static const struct spi_nor_reg_field_item gd25le255e_sr2_fields[] = {
	SNOR_REG_FIELD(0, 1, "SRP1", "Status Register Protect Bit 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(1, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(4, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(5, 1, "LB3", "Security Register Lock Bit 3"),
};

static const struct spi_nor_reg_def  gd25le255e_sr2 = SNOR_REG_DEF("SR2", "Status Register 2", &cr_acc,
								   gd25le255e_sr2_fields);

static const struct snor_reg_info gd25le255e_regs = SNOR_REG_INFO(&gd25lb256d_sr, &gd25le255e_sr2, &gd25b256e_sr3);

static const struct snor_reg_info gd55f512mf_regs = SNOR_REG_INFO(&gd25b257d_sr1, &gd25b257d_sr2, &gd25b256e_sr3);

static const struct spi_nor_reg_field_item gd55t512me_sr2_fields[] = {
	SNOR_REG_FIELD(3, 1, "LB", "Security Register Lock Bit"),
	SNOR_REG_FIELD(6, 1, "SRP1", "Status Register Protect Bit 1"),
};

static const struct spi_nor_reg_def  gd55t512me_sr2 = SNOR_REG_DEF("SR2", "Status Register 2", &cr_acc,
								   gd55t512me_sr2_fields);

static const struct spi_nor_reg_field_values gd55t512me_nvcr_0_iom_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0xff, "STR with DQS"),
	VALUE_ITEM(0xdf, "STR without DQS"),
	VALUE_ITEM(0xe7, "Quad DTR with DQS"),
	VALUE_ITEM(0xc7, "Quad DTR without DQS"),
);

static const struct spi_nor_reg_field_item gd55t512me_nvcr_0_fields[] = {
	SNOR_REG_FIELD_FULL(0, 0xff, "IOM", "I/O Mode", &gd55t512me_nvcr_0_iom_values),
};

static const struct spi_nor_reg_def gd55t512me_nvcr_0 = SNOR_REG_DEF("NVCR0", "Non-volatile Status Register 0",
								     &gd_nvcr_0_acc, gd55t512me_nvcr_0_fields);

static const struct spi_nor_reg_field_values gd55t512me_nvcr_3_odt_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0xc, "100 Ohm ODT"),
	VALUE_ITEM(0xd, "150 Ohm ODT"),
	VALUE_ITEM(0xe, "300 Ohm ODT"),
	VALUE_ITEM(0xf, "ODT Disabled"),
);

static const struct spi_nor_reg_field_values gd55t512me_nvcr_3_drv_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0xc, "18 Ohm"),
	VALUE_ITEM(0xd, "25 Ohm"),
	VALUE_ITEM(0xe, "35 Ohm"),
	VALUE_ITEM(0xf, "50 Ohm"),
);

static const struct spi_nor_reg_field_item gd55t512me_nvcr_3_fields[] = {
	SNOR_REG_FIELD_FULL(0, 0xf, "DRV", "Driver Stringth", &gd55t512me_nvcr_3_drv_values),
	SNOR_REG_FIELD_FULL(4, 0xf, "ODT", "On Die Termination", &gd55t512me_nvcr_3_odt_values),
};

static const struct spi_nor_reg_def gd55t512me_nvcr_3 = SNOR_REG_DEF("NVCR3", "Non-volatile Status Register 3",
								     &gd_nvcr_3_acc, gd55t512me_nvcr_3_fields);

static const struct spi_nor_reg_field_values gd55t512me_nvcr_4_ecs_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "ECC Disabled"),
	VALUE_ITEM(1, "1- or 2-bit Error"),
	VALUE_ITEM(2, "2-bit Error Only"),
	VALUE_ITEM(3, "2-bit Error or Double Programmed"),
);

static const struct spi_nor_reg_field_values gd55t512me_nvcr_4_crc_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "128-Byte CRC"),
	VALUE_ITEM(1, "64-Byte CRC"),
	VALUE_ITEM(2, "32-Byte CRC"),
	VALUE_ITEM(3, "16-Byte CRC"),
);

static const struct spi_nor_reg_field_item gd55t512me_nvcr_4_fields[] = {
	SNOR_REG_FIELD_FULL(0, 3, "ECS", "ECS# Configuration", &gd55t512me_nvcr_4_ecs_values),
	SNOR_REG_FIELD_FULL(2, 1, "WPS", "Write Protection Selection", &gd_nvcr_4_wps_values),
	SNOR_REG_FIELD_ENABLED_DISABLED(3, 1, "DLP", "Data Learning Pattern"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(4, 1, "CRCIN", "CRC Input Enable"),
	SNOR_REG_FIELD_ENABLED_DISABLED_REV(5, 1, "CRCOUT", "CRC Output Enable"),
	SNOR_REG_FIELD_FULL(0, 3, "CRC", "CRC Configuration", &gd55t512me_nvcr_4_crc_values),
};

static const struct spi_nor_reg_def gd55t512me_nvcr_4 = SNOR_REG_DEF("NVCR4", "Non-volatile Status Register 4",
								     &gd_nvcr_4_acc, gd55t512me_nvcr_4_fields);

static const struct snor_reg_info gd55t512me_regs = SNOR_REG_INFO(&gd25b256d_sr1, &gd55t512me_sr2,
								  &gd55t512me_nvcr_0, &gd25lb256e_nvcr_1,
								  &gd55t512me_nvcr_3, &gd55t512me_nvcr_4,
								  &gd25lb256e_nvcr_5, &gd25lb256e_nvcr_6,
								  &gd25lb256e_nvcr_7);

static const struct spi_nor_reg_field_item gd25b512me_nvcr_4_fields[] = {
	SNOR_REG_FIELD_FULL(2, 1, "WPS", "Write Protection Selection", &gd_nvcr_4_wps_values),
	SNOR_REG_FIELD_ENABLED_DISABLED(3, 1, "DLP", "Data Learning Pattern"),
};

static const struct spi_nor_reg_def gd25b512me_nvcr_4 = SNOR_REG_DEF("NVCR4", "Non-volatile Status Register 4",
								     &gd_nvcr_4_acc, gd25b512me_nvcr_4_fields);

static const struct snor_reg_info gd25b512me_regs = SNOR_REG_INFO(&gd25b256d_sr1, &gd55t512me_sr2,
								  &gd25lb256e_nvcr_1, &gd55t512me_nvcr_3,
								  &gd25b512me_nvcr_4, &gd25lb256e_nvcr_5,
								  &gd25lb256e_nvcr_6, &gd25lb256e_nvcr_7);

#define GD_WP_BP_SEC(_mask, _val, _lower, _cmp, _lshift)						\
	{ .sr_mask = (_mask), .sr_val = (_val), .lower = (_lower), .cmp = (_cmp), .shift = (_lshift),	\
	  .type = SNOR_WPR_SECTOR, .whole_cmp = true }

#define GD_WP_SP_CMP_LO(_mask, _val, _lshift)	GD_WP_BP_SEC(_mask, _val, true, true, _lshift)
#define GD_WP_SP_UP(_mask, _val, _lshift)	GD_WP_BP_SEC(_mask, _val, false, false, _lshift)

static const struct spi_nor_wp_info gd25dxc_wpr = SNOR_WP_BP(&sr_acc,
	SNOR_WP_BP_LO(BP_2_0,     0                       , -1),	/* None */

	SNOR_WP_BP_LO(BP_2_0,     SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	GD_WP_SP_CMP_LO(BP_2_0,                    SR_BP0, 1),		/* Lower T - 8KB */
	GD_WP_SP_CMP_LO(BP_2_0,           SR_BP1         , 2),		/* Lower T - 16KB */
	GD_WP_SP_CMP_LO(BP_2_0,           SR_BP1 | SR_BP0, 3),		/* Lower T - 32KB */
	GD_WP_SP_CMP_LO(BP_2_0,  SR_BP2                  , 4),		/* Lower T - 64KB */
	GD_WP_SP_CMP_LO(BP_2_0,  SR_BP2 |          SR_BP0, 5),		/* Lower T - 128KB */
	GD_WP_SP_CMP_LO(BP_2_0,  SR_BP2 | SR_BP1         , 6),		/* Lower T - 256KB */
);

static const struct spi_nor_wp_info gd25dxe_wpr = SNOR_WP_BP(&sr_acc,
	SNOR_WP_BP_LO(BP_2_0_TB,   0                               , -1),	/* None */
	SNOR_WP_BP_LO(BP_2_0_TB,   SR_TB | SR_BP2 | SR_BP1 | SR_BP0, -1),	/* None */

	SNOR_WP_BP_LO(BP_2_0_TB,           SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */
	SNOR_WP_BP_LO(BP_2_0_TB,   SR_TB                           , -2),	/* All */

	GD_WP_SP_CMP_LO(BP_2_0_TB,                           SR_BP0, 1),	/* Lower T - 8KB */
	GD_WP_SP_CMP_LO(BP_2_0_TB,                  SR_BP1         , 2),	/* Lower T - 16KB */
	GD_WP_SP_CMP_LO(BP_2_0_TB,                  SR_BP1 | SR_BP0, 3),	/* Lower T - 32KB */
	GD_WP_SP_CMP_LO(BP_2_0_TB,         SR_BP2                  , 4),	/* Lower T - 64KB */
	GD_WP_SP_CMP_LO(BP_2_0_TB,         SR_BP2 |          SR_BP0, 5),	/* Lower T - 128KB */
	GD_WP_SP_CMP_LO(BP_2_0_TB,         SR_BP2 | SR_BP1         , 6),	/* Lower T - 256KB */

	GD_WP_SP_UP(BP_2_0_TB,     SR_TB |                   SR_BP0, 1),	/* Upper 8KB */
	GD_WP_SP_UP(BP_2_0_TB,     SR_TB |          SR_BP1         , 2),	/* Upper 16KB */
	GD_WP_SP_UP(BP_2_0_TB,     SR_TB |          SR_BP1 | SR_BP0, 3),	/* Upper 32KB */
	GD_WP_SP_UP(BP_2_0_TB,     SR_TB | SR_BP2                  , 4),	/* Upper 64KB */
	GD_WP_SP_UP(BP_2_0_TB,     SR_TB | SR_BP2 |          SR_BP0, 5),	/* Upper 128KB */
	GD_WP_SP_UP(BP_2_0_TB,     SR_TB | SR_BP2 | SR_BP1         , 6),	/* Upper 256KB */
);

static const struct spi_nor_wp_info gd_wpr_4bp_tb = SNOR_WP_BP(&gd_srcr_acc,
	SNOR_WP_BP_UP(BP_3_0_TB, 0                                          , -1),	/* None */
	SNOR_WP_BP_UP(BP_3_0_TB, SR_TB11                                    , -1),	/* None */

	SNOR_WP_BP_UP(BP_3_0_TB,           SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */
	SNOR_WP_BP_UP(BP_3_0_TB, SR_TB11 | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0, -2),	/* All */

	SNOR_WP_BP_UP(BP_3_0_TB,                                      SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(BP_3_0_TB,                             SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(BP_3_0_TB,                             SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(BP_3_0_TB,                    SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(BP_3_0_TB,                    SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(BP_3_0_TB,                    SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(BP_3_0_TB,                    SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_UP(BP_3_0_TB,           SR_BP3                           , 7),	/* Upper 8MB */
	SNOR_WP_BP_UP(BP_3_0_TB,           SR_BP3 |                   SR_BP0, 8),	/* Upper 16MB */
	SNOR_WP_BP_UP(BP_3_0_TB,           SR_BP3 |          SR_BP1         , 9),	/* Upper 32MB */
	SNOR_WP_BP_UP(BP_3_0_TB,           SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Upper 64MB */
	SNOR_WP_BP_UP(BP_3_0_TB,           SR_BP3 | SR_BP2                  , 11),	/* Upper 128MB */
	SNOR_WP_BP_UP(BP_3_0_TB,           SR_BP3 | SR_BP2 | SR_BP1         , 12),	/* Upper 256MB */
	SNOR_WP_BP_UP(BP_3_0_TB,           SR_BP3 | SR_BP2 | SR_BP1         , 13),	/* Upper 512MB */

	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 |                            SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 |                   SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 |                   SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 |          SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 |          SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 |          SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 |          SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Lower 4MB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 | SR_BP3                           , 7),	/* Lower 8MB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 | SR_BP3 |                   SR_BP0, 8),	/* Lower 16MB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 | SR_BP3 |          SR_BP1         , 9),	/* Lower 32MB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 | SR_BP3 |          SR_BP1 | SR_BP0, 10),	/* Lower 64MB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 | SR_BP3 | SR_BP2                  , 11),	/* Lower 128MB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 | SR_BP3 | SR_BP2 | SR_BP1         , 12),	/* Lower 256MB */
	SNOR_WP_BP_LO(BP_3_0_TB, SR_TB11 | SR_BP3 | SR_BP2 | SR_BP1         , 13),	/* Lower 512MB */
);

static const struct spi_nor_otp_info gd25_otp_1_512b = {
	.start_index = 0,
	.count = 1,
	.size = 0x200,
};

static const struct spi_nor_otp_info gd25_otp_1_4k = {
	.start_index = 0,
	.count = 1,
	.size = 0x1000,
};

static const struct spi_nor_otp_info gd25_otp_2_1k = {
	.start_index = 0,
	.count = 2,
	.size = 0x400,
};

static const struct spi_nor_otp_info gd25_otp_2_1k_index_2 = {
	.start_index = 2,
	.count = 2,
	.size = 0x400,
};

static const struct spi_nor_otp_info gd25_otp_3 = {
	.start_index = 1,
	.count = 3,
	.size = 0x100,
};

static const struct spi_nor_otp_info gd25_otp_3_512b = {
	.start_index = 1,
	.count = 3,
	.size = 0x200,
};

static const struct spi_nor_otp_info gd25_otp_3_1k = {
	.start_index = 1,
	.count = 3,
	.size = 0x400,
};

static const struct spi_nor_otp_info gd25_otp_3_2k = {
	.start_index = 1,
	.count = 3,
	.size = 0x800,
};


static const struct spi_nor_otp_info gd25_otp_4_in_1 = {
	.start_index = 0,
	.count = 1,
	.size = 0x400,
};

static const struct spi_nor_erase_info gd_erase_opcodes_4b = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_4K, SNOR_CMD_4B_SECTOR_ERASE),
	SNOR_ERASE_SECTOR(SZ_32K, SNOR_CMD_GD_4B_SECTOR_ERASE_32K),
	SNOR_ERASE_SECTOR(SZ_64K, SNOR_CMD_4B_BLOCK_ERASE)
);

static DEFINE_SNOR_ALIAS(gd25le05c_alias, SNOR_ALIAS_MODEL("GD25LQ05C"));
static DEFINE_SNOR_ALIAS(gd25le10c_alias, SNOR_ALIAS_MODEL("GD25LQ10C"));
static DEFINE_SNOR_ALIAS(gd25ve20c_alias, SNOR_ALIAS_MODEL("GD25VQ20C"));
static DEFINE_SNOR_ALIAS(gd25le20c_alias, SNOR_ALIAS_MODEL("GD25LQ20C"));
static DEFINE_SNOR_ALIAS(gd25le20e_alias, SNOR_ALIAS_MODEL("GD25LQ20E"));
static DEFINE_SNOR_ALIAS(gd25b40c_alias, SNOR_ALIAS_MODEL("GD25Q40C"));
static DEFINE_SNOR_ALIAS(gd25le40c_alias, SNOR_ALIAS_MODEL("GD25LQ40C"));
static DEFINE_SNOR_ALIAS(gd25le40e_alias, SNOR_ALIAS_MODEL("GD25LQ40E"));
static DEFINE_SNOR_ALIAS(gd25ve40c_alias, SNOR_ALIAS_MODEL("GD25VQ40C"));
static DEFINE_SNOR_ALIAS(gd25ve16c_alias, SNOR_ALIAS_MODEL("GD25VQ16C"));
static DEFINE_SNOR_ALIAS(gd25le80c_alias, SNOR_ALIAS_MODEL("GD25LQ80C"));
static DEFINE_SNOR_ALIAS(gd25le80e_alias, SNOR_ALIAS_MODEL("GD25LQ80E"));
static DEFINE_SNOR_ALIAS(gd25b16c_alias, SNOR_ALIAS_MODEL("GD25Q16C"));
static DEFINE_SNOR_ALIAS(gd25b16e_alias, SNOR_ALIAS_MODEL("GD25Q16E"));
static DEFINE_SNOR_ALIAS(gd25le16c_alias, SNOR_ALIAS_MODEL("GD25LQ16C"));
static DEFINE_SNOR_ALIAS(gd25le16e_alias, SNOR_ALIAS_MODEL("GD25LQ16E"));
static DEFINE_SNOR_ALIAS(gd25b32c_alias, SNOR_ALIAS_MODEL("GD25Q32C"));
static DEFINE_SNOR_ALIAS(gd25b32e_alias, SNOR_ALIAS_MODEL("GD25Q32E"));
static DEFINE_SNOR_ALIAS(gd25ve32c_alias, SNOR_ALIAS_MODEL("GD25VQ32C"));
static DEFINE_SNOR_ALIAS(gd25le32d_alias, SNOR_ALIAS_MODEL("GD25LQ32D"));
static DEFINE_SNOR_ALIAS(gd25le32e_alias, SNOR_ALIAS_MODEL("GD25LQ32E"));
static DEFINE_SNOR_ALIAS(gd25b64c_alias, SNOR_ALIAS_MODEL("GD25Q64C"));
static DEFINE_SNOR_ALIAS(gd25b64e_alias, SNOR_ALIAS_MODEL("GD25Q64E"));
static DEFINE_SNOR_ALIAS(gd25ve64c_alias, SNOR_ALIAS_MODEL("GD25VQ64C"));
static DEFINE_SNOR_ALIAS(gd25le64c_alias, SNOR_ALIAS_MODEL("GD25LQ64C"));
static DEFINE_SNOR_ALIAS(gd25le64e_alias, SNOR_ALIAS_MODEL("GD25LQ64E"));
static DEFINE_SNOR_ALIAS(gd25le128d_alias, SNOR_ALIAS_MODEL("GD25LQ128D"));
static DEFINE_SNOR_ALIAS(gd25b256d_alias, SNOR_ALIAS_MODEL("GD25Q256D"));
static DEFINE_SNOR_ALIAS(gd25b256e_alias, SNOR_ALIAS_MODEL("GD25Q256E"));
static DEFINE_SNOR_ALIAS(gd25b257d_alias, SNOR_ALIAS_MODEL("GD25Q257D"));
static DEFINE_SNOR_ALIAS(gd25le256d_alias, SNOR_ALIAS_MODEL("GD25LQ256D"));
static DEFINE_SNOR_ALIAS(gd25wb256e_alias, SNOR_ALIAS_MODEL("GD25WQ256D"));
static DEFINE_SNOR_ALIAS(gd55wb512me_alias, SNOR_ALIAS_MODEL("GD55WR512ME"));

static ufprog_status gd_pre_param_setup(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp);
static ufprog_status gd_read_uid_len(struct spi_nor *snor, void *data, uint32_t len);

static ufprog_status gd25lx05_fixup_model(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	bp->p.model = bp->model;

	if (snor->sfdp.bfpt) {
		snprintf(bp->model, sizeof(bp->model), "GD25LE05C");

		bp->p.qe_type = QE_SR2_BIT1_WR_SR1;
		bp->p.max_speed_spi_mhz = 90;
		bp->p.pp_io_caps |= BIT_SPI_MEM_IO_1_1_4;
		bp->p.read_io_caps |= BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_X4;
		bp->p.soft_reset_flags = SNOR_SOFT_RESET_OPCODE_66H_99H;
		bp->p.flags |= SNOR_F_SR_VOLATILE_WREN_50H;
		bp->p.regs = &w25q_regs;
		bp->p.wp_ranges = &wpr_3bp_tb_sec_cmp;
		bp->p.otp = &gd25_otp_3_512b;
	} else {
		snprintf(bp->model, sizeof(bp->model), "GD25LD05C");
		bp->p.regs = &gd25dxc_regs;
		bp->p.wp_ranges = &gd25dxc_wpr;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup gd25lx05_fixups = {
	.pre_param_setup = gd25lx05_fixup_model,
};

static ufprog_status gd25lx10_fixup_model(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	bp->p.model = bp->model;

	if (snor->sfdp.bfpt) {
		snprintf(bp->model, sizeof(bp->model), "GD25LE10C");

		bp->p.qe_type = QE_SR2_BIT1_WR_SR1;
		bp->p.max_speed_spi_mhz = 90;
		bp->p.pp_io_caps |= BIT_SPI_MEM_IO_1_1_4;
		bp->p.read_io_caps |= BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_X4;
		bp->p.soft_reset_flags = SNOR_SOFT_RESET_OPCODE_66H_99H;
		bp->p.flags |= SNOR_F_SR_VOLATILE_WREN_50H;
		bp->p.regs = &w25q_regs;
		bp->p.wp_ranges = &wpr_3bp_tb_sec_cmp;
		bp->p.otp = &gd25_otp_3_512b;
	} else {
		snprintf(bp->model, sizeof(bp->model), "GD25LD10C");
		bp->p.regs = &gd25dxc_regs;
		bp->p.wp_ranges = &gd25dxc_wpr;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup gd25lx10_fixups = {
	.pre_param_setup = gd25lx10_fixup_model,
};

static ufprog_status gd25lb128e_setup_qpi(struct spi_nor *snor, bool enabled)
{
	if (enabled) {
		/* Set QPI read dummy cycles to 8 for maximum speed */
		return spi_nor_write_reg(snor, SNOR_CMD_SET_READ_PARAMETERS, QPI_READ_DUMMY_CLOCKS_6_8);
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_ops gd25lb128e_ops = {
	.setup_qpi = gd25lb128e_setup_qpi,
};

static ufprog_status gd25le128e_fixup(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval, dc;

	STATUS_CHECK_RET(gd_pre_param_setup(snor, bp));

	/* Handle GD_F_DC_SR3_BIT0_1 but not the same as others */

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &sr3_acc, 0, BITS(1, 0), true));
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &sr3_acc, &regval));

	dc = regval & BITS(1, 0);
	switch (dc) {
	case 0:
	case 1:
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = 6;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;
		break;

	case 2:
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;
		break;

	case 3:
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = 10;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup gd25le128e_fixups = {
	.pre_param_setup = gd25le128e_fixup,
};

static ufprog_status gd25q256c_otp_lock_bit(struct spi_nor *snor, uint32_t index, uint32_t *retbit,
					    const struct spi_nor_reg_access **retacc)
{
	*(retacc) = &sr3_acc;

	switch (index) {
	case 1:
		*retbit = 0;
		break;

	case 2:
		*retbit = 1;
		break;

	case 3:
		*retbit = 4;
		break;

	default:
		return UFP_INVALID_PARAMETER;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_secr_otp_ops gd25q256c_secr_otp_ops = {
	.otp_lock_bit = gd25q256c_otp_lock_bit,
};

static const struct spi_nor_flash_part_otp_ops gd25q256c_otp_ops = {
	.read = secr_otp_read_paged,
	.write = secr_otp_write_paged,
	.erase = secr_otp_erase,
	.lock = secr_otp_lock,
	.locked = secr_otp_locked,
	.secr = &gd25q256c_secr_otp_ops,
};

static ufprog_status gd25q256c_fixup(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	uint32_t cr, lc;

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &cr_acc, BITS(7, 6), 0, true));
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &cr_acc, &cr));

	spi_nor_blank_part_fill_default_opcodes(bp);

	lc = (cr & BITS(7, 6)) >> 6;

	switch (lc) {
	case 0:
		break;

	case 1:
	case 2:
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = 4;
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = 2;
		bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].nmode = 4;
		bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].ndummy = 2;

		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 2;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = 6;
		bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].nmode = 2;
		bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].ndummy = 6;

	case 3:
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_1].ndummy = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_1_1_1].ndummy = 0;

		bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].nmode = 0;
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].ndummy = 6;
		bp->read_opcodes_4b[SPI_MEM_IO_1_1_2].nmode = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_1_1_2].ndummy = 6;

		bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].nmode = 0;
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy = 6;
		bp->read_opcodes_4b[SPI_MEM_IO_1_1_4].nmode = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_1_1_4].ndummy = 6;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup gd25q256c_fixups = {
	.pre_param_setup = gd25q256c_fixup,
};

static ufprog_status gd25q256c_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	if (retlen)
		*retlen = GD25Q256C_UID_LEN;

	if (!data)
		return UFP_OK;

	return gd_read_uid_len(snor, data, GD25Q256C_UID_LEN);
}

static const struct spi_nor_flash_part_ops gd25q256c_part_ops = {
	.otp = &gd25q256c_otp_ops,

	.read_uid = gd25q256c_read_uid,
};

static ufprog_status gd25b257d_fixup(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &cr_acc, BITS(1, 0), 0, true));

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup gd25b257d_fixups = {
	.pre_param_setup = gd25b257d_fixup,
};

static ufprog_status gd_otp_read_paged_3b(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	return secr_otp_read_paged_naddr(snor, SNOR_CMD_READ_OTP, index, addr, 3, len, data);
}

static ufprog_status gd_otp_write_paged_3b(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len,
					   const void *data)
{
	return secr_otp_write_paged_naddr(snor, SNOR_CMD_PROG_OTP, index, addr, snor->state.a4b_mode ? 4 : 3, len,
					  data);
}

static ufprog_status gd_otp_erase_3b(struct spi_nor *snor, uint32_t index)
{
	return secr_otp_erase_naddr(snor, SNOR_CMD_ERASE_OTP, index, 3);
}

static const struct spi_nor_flash_part_otp_ops gd_otp_3b_ops = {
	.read = gd_otp_read_paged_3b,
	.write = gd_otp_write_paged_3b,
	.erase = gd_otp_erase_3b,
	.lock = secr_otp_lock,
	.locked = secr_otp_locked,
};

static ufprog_status gd25lx256d_otp_fixup(struct spi_nor *snor)
{
	snor->ext_param.ops.otp = &gd_otp_3b_ops;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup gd25lx256d_fixups = {
	.pre_chip_setup = gd25lx256d_otp_fixup,
};

static ufprog_status gd25s513md_fixup(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	STATUS_CHECK_RET(spi_nor_select_die(snor, 1));
	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &cr_acc, BITS(1, 0), 0, true));

	STATUS_CHECK_RET(spi_nor_select_die(snor, 0));
	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &cr_acc, BITS(1, 0), 0, true));

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup gd25s513md_fixups = {
	.pre_param_setup = gd25s513md_fixup,
};

static struct spi_nor_wp_info *gd_3bp_tb_sec_cmp, gd_3bp_tb_sec_cmp_dummy;

static const struct spi_nor_flash_part gigadevice_parts[] = {
	SNOR_PART("GD25 512Kb", SNOR_ID(0xc8, 0x40, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
	),

	SNOR_PART("GD25Q512", SNOR_ID(0xc8, 0x40, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25q_no_lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec),
	),

	SNOR_PART("GD25D05C", SNOR_ID(0xc8, 0x40, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25L*05", SNOR_ID(0xc8, 0x60, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_FIXUPS(&gd25lx05_fixups),
	),

	SNOR_PART("GD25LD05C", SNOR_ID(0xc8, 0x60, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25LE05C", SNOR_ID(0xc8, 0x60, 0x10), SZ_64K, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25le05c_alias), /* GD25LQ05C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(90),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_512b),
	),

	SNOR_PART("GD25WD05C", SNOR_ID(0xc8, 0x64, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50), SNOR_QUAD_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25*10", SNOR_ID(0xc8, 0x40, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
	),

	SNOR_PART("GD25Q10", SNOR_ID(0xc8, 0x40, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25q_no_lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec),
	),

	SNOR_PART("GD25D10C", SNOR_ID(0xc8, 0x40, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25L*10C", SNOR_ID(0xc8, 0x60, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_FIXUPS(&gd25lx10_fixups),
	),

	SNOR_PART("GD25LD10C", SNOR_ID(0xc8, 0x60, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25LE10C", SNOR_ID(0xc8, 0x60, 0x11), SZ_128K, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25le10c_alias), /* GD25LQ10C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(90),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_512b),
	),

	SNOR_PART("GD25WD10C", SNOR_ID(0xc8, 0x64, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50), SNOR_QUAD_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25*20", SNOR_ID(0xc8, 0x40, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
	),

	SNOR_PART("GD25Q20B", SNOR_ID(0xc8, 0x40, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25qxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("GD25Q20C", SNOR_ID(0xc8, 0x40, 0x12), SZ_256K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25qxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25Q20E", SNOR_ID(0xc8, 0x40, 0x12), SZ_256K,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SRCR_BIT12),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_2_1k),
	),

	SNOR_PART("GD25D20C", SNOR_ID(0xc8, 0x40, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25D20E", SNOR_ID(0xc8, 0x40, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_OTP_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25dxe_regs),
		  SNOR_WP_RANGES(&gd25dxe_wpr),
		  SNOR_OTP_INFO(&gd25_otp_1_512b),
	),

	SNOR_PART("GD25VE20C", SNOR_ID(0xc8, 0x42, 0x12), SZ_256K, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25ve20c_alias), /* GD25VQ20C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25L*20", SNOR_ID(0xc8, 0x60, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("GD25LD20C", SNOR_ID(0xc8, 0x60, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25LD20E", SNOR_ID(0xc8, 0x60, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_OTP_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxe_regs),
		  SNOR_WP_RANGES(&gd25dxe_wpr),
		  SNOR_OTP_INFO(&gd25_otp_1_512b),
	),

	SNOR_PART("GD25LE20C", SNOR_ID(0xc8, 0x60, 0x12), SZ_256K, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25le20c_alias), /* GD25LQ20C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(90),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_512b),
	),

	SNOR_PART("GD25LE20E", SNOR_ID(0xc8, 0x60, 0x12), SZ_256K,
		  SNOR_ALIAS(&gd25le20e_alias), /* GD25LQ20E */
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_512b),
	),

	SNOR_PART("GD25W*20", SNOR_ID(0xc8, 0x64, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("GD25WD20C", SNOR_ID(0xc8, 0x64, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50), SNOR_QUAD_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25WD20E", SNOR_ID(0xc8, 0x64, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_OTP_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50), SNOR_QUAD_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxe_regs),
		  SNOR_WP_RANGES(&gd25dxe_wpr),
		  SNOR_OTP_INFO(&gd25_otp_1_512b),
	),

	SNOR_PART("GD25WQ20E", SNOR_ID(0xc8, 0x65, 0x12), SZ_256K,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SRCR_BIT12),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&gd25qxe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_2_1k),
	),

	SNOR_PART("GD25*40", SNOR_ID(0xc8, 0x40, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
	),

	SNOR_PART("GD25B40C", SNOR_ID(0xc8, 0x40, 0x13), SZ_512K, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25b40c_alias), /* GD25Q40C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25qxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25Q40B", SNOR_ID(0xc8, 0x40, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25qxb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("GD25Q40E", SNOR_ID(0xc8, 0x40, 0x13), SZ_512K,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SRCR_BIT12),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_2_1k),
	),

	SNOR_PART("GD25D40C", SNOR_ID(0xc8, 0x40, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25D40E", SNOR_ID(0xc8, 0x40, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_OTP_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25dxe_regs),
		  SNOR_WP_RANGES(&gd25dxe_wpr),
		  SNOR_OTP_INFO(&gd25_otp_1_512b),
	),

	SNOR_PART("GD25L*40", SNOR_ID(0xc8, 0x60, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("GD25LD40C", SNOR_ID(0xc8, 0x60, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25LD40E", SNOR_ID(0xc8, 0x60, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_OTP_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxe_regs),
		  SNOR_WP_RANGES(&gd25dxe_wpr),
		  SNOR_OTP_INFO(&gd25_otp_1_512b),
	),

	SNOR_PART("GD25LE40C", SNOR_ID(0xc8, 0x60, 0x13), SZ_512K, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25le40c_alias), /* GD25LQ40C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(90),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_512b),
	),

	SNOR_PART("GD25LE40E", SNOR_ID(0xc8, 0x60, 0x13), SZ_512K,
		  SNOR_ALIAS(&gd25le40e_alias), /* GD25LQ40E */
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_512b),
	),

	SNOR_PART("GD25W*40", SNOR_ID(0xc8, 0x64, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("GD25WD40C", SNOR_ID(0xc8, 0x64, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50), SNOR_QUAD_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25WD40E", SNOR_ID(0xc8, 0x64, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_OTP_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50), SNOR_QUAD_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxe_regs),
		  SNOR_WP_RANGES(&gd25dxe_wpr),
		  SNOR_OTP_INFO(&gd25_otp_1_512b),
	),

	SNOR_PART("GD25WQ40E", SNOR_ID(0xc8, 0x65, 0x13), SZ_512K,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SRCR_BIT12),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&gd25qxe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_2_1k),
	),

	SNOR_PART("GD25VE40C", SNOR_ID(0xc8, 0x42, 0x13), SZ_512K, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25ve40c_alias), /* GD25VQ40C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25*80", SNOR_ID(0xc8, 0x40, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
	),

	SNOR_PART("GD25D80C", SNOR_ID(0xc8, 0x40, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25D80E", SNOR_ID(0xc8, 0x40, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_OTP_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25dxe_regs),
		  SNOR_WP_RANGES(&gd25dxe_wpr),
		  SNOR_OTP_INFO(&gd25_otp_1_512b),
	),

	SNOR_PART("GD25Q80", SNOR_ID(0xc8, 0x40, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&w25q_no_lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec),
	),

	SNOR_PART("GD25Q80B", SNOR_ID(0xc8, 0x40, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25qxb_lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25Q80C", SNOR_ID(0xc8, 0x40, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25qxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25Q80E", SNOR_ID(0xc8, 0x40, 0x14), SZ_1M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SRCR_BIT12),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_2_1k),
	),

	SNOR_PART("GD25VQ80C", SNOR_ID(0xc8, 0x42, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25L*80", SNOR_ID(0xc8, 0x60, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("GD25LD80C", SNOR_ID(0xc8, 0x60, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25LD80E", SNOR_ID(0xc8, 0x60, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_OTP_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxe_regs),
		  SNOR_WP_RANGES(&gd25dxe_wpr),
		  SNOR_OTP_INFO(&gd25_otp_1_512b),
	),

	SNOR_PART("GD25LE80C", SNOR_ID(0xc8, 0x60, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25le80c_alias), /* GD25LQ80C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(90),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_512b),
	),

	SNOR_PART("GD25LE80E", SNOR_ID(0xc8, 0x60, 0x14), SZ_1M,
		  SNOR_ALIAS(&gd25le80e_alias), /* GD25LQ80E*/
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LF80E", SNOR_ID(0xc8, 0x63, 0x14), SZ_1M,
		  SNOR_VENDOR_FLAGS(GD_F_QPI_DUMMY_10),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25W*80", SNOR_ID(0xc8, 0x64, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("GD25WD80C", SNOR_ID(0xc8, 0x64, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50), SNOR_QUAD_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxc_regs),
		  SNOR_WP_RANGES(&gd25dxc_wpr),
	),

	SNOR_PART("GD25WD80E", SNOR_ID(0xc8, 0x64, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_OTP_1),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50), SNOR_QUAD_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&gd25dxe_regs),
		  SNOR_WP_RANGES(&gd25dxe_wpr),
		  SNOR_OTP_INFO(&gd25_otp_1_512b),
	),

	SNOR_PART("GD25WQ80E", SNOR_ID(0xc8, 0x65, 0x14), SZ_1M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SRCR_BIT12),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&gd25qxe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_2_1k),
	),

	SNOR_PART("GD25*16", SNOR_ID(0xc8, 0x40, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("GD25Q16B", SNOR_ID(0xc8, 0x40, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25qxb_lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25B16C", SNOR_ID(0xc8, 0x40, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25b16c_alias), /* GD25Q16C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25qxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25B16E", SNOR_ID(0xc8, 0x40, 0x15), SZ_2M,
		  SNOR_ALIAS(&gd25b16e_alias), /* GD25Q16E */
		  SNOR_VENDOR_FLAGS(GD_F_DC_SRCR_BIT12),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_2_1k),
	),

	SNOR_PART("GD25VE16C", SNOR_ID(0xc8, 0x42, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25ve16c_alias), /* GD25VQ16C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25L*16", SNOR_ID(0xc8, 0x60, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(90),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("GD25LB16C", SNOR_ID(0xc8, 0x60, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_512b),
	),

	SNOR_PART("GD25LB16E", SNOR_ID(0xc8, 0x60, 0x15), SZ_2M,
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LE16C", SNOR_ID(0xc8, 0x60, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25le16c_alias), /* GD25LQ16C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(90),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_512b),
	),

	SNOR_PART("GD25LE16E", SNOR_ID(0xc8, 0x60, 0x15), SZ_2M,
		  SNOR_ALIAS(&gd25le16e_alias), /* GD25LQ16E*/
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LF16E", SNOR_ID(0xc8, 0x63, 0x15), SZ_2M,
		  SNOR_VENDOR_FLAGS(GD_F_QPI_DUMMY_10),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25WQ16E", SNOR_ID(0xc8, 0x65, 0x15), SZ_2M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SRCR_BIT12),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&gd25qxe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_2_1k),
	),

	SNOR_PART("GD25*32", SNOR_ID(0xc8, 0x40, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
	),

	SNOR_PART("GD25Q32B", SNOR_ID(0xc8, 0x40, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25qxb_lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25B32C", SNOR_ID(0xc8, 0x40, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25b32c_alias), /* GD25Q32C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25qxc_3_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25B32E", SNOR_ID(0xc8, 0x40, 0x16), SZ_4M,
		  SNOR_ALIAS(&gd25b32e_alias), /* GD25Q32E */
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxe_3_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25VE32C", SNOR_ID(0xc8, 0x42, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25ve32c_alias), /* GD25VQ32C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxc_3_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25L*32", SNOR_ID(0xc8, 0x60, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25q_regs),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LB32D", SNOR_ID(0xc8, 0x60, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LB32E", SNOR_ID(0xc8, 0x60, 0x16), SZ_4M,
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LE32D", SNOR_ID(0xc8, 0x60, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25le32d_alias), /* GD25LQ32D */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LE32E", SNOR_ID(0xc8, 0x60, 0x16), SZ_4M,
		  SNOR_ALIAS(&gd25le32e_alias), /* GD25LQ32E*/
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LF32E", SNOR_ID(0xc8, 0x63, 0x16), SZ_4M,
		  SNOR_VENDOR_FLAGS(GD_F_QPI_DUMMY_10),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25WQ32E", SNOR_ID(0xc8, 0x65, 0x16), SZ_4M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&gd25qxe_3_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25*64", SNOR_ID(0xc8, 0x40, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
	),

	SNOR_PART("GD25Q64B", SNOR_ID(0xc8, 0x40, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25qxb_lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25B64C", SNOR_ID(0xc8, 0x40, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25b64c_alias), /* GD25Q64C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25qxc_3_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25B64E", SNOR_ID(0xc8, 0x40, 0x17), SZ_8M,
		  SNOR_ALIAS(&gd25b64e_alias), /* GD25Q64E */
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxe_3_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25VE64C", SNOR_ID(0xc8, 0x42, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25ve64c_alias), /* GD25VQ64C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(GD_F_HPM),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxc_3_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25L*64", SNOR_ID(0xc8, 0x60, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_regs),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LB64C", SNOR_ID(0xc8, 0x60, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120), SNOR_DUAL_MAX_SPEED_MHZ(104),  SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LB64E", SNOR_ID(0xc8, 0x60, 0x17), SZ_8M,
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LE64C", SNOR_ID(0xc8, 0x60, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25le64c_alias), /* GD25LQ64C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LE64E", SNOR_ID(0xc8, 0x60, 0x17), SZ_8M,
		  SNOR_ALIAS(&gd25le64e_alias), /* GD25LQ64E*/
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LF64E", SNOR_ID(0xc8, 0x63, 0x17), SZ_8M,
		  SNOR_VENDOR_FLAGS(GD_F_QPI_DUMMY_10),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25WQ64E", SNOR_ID(0xc8, 0x65, 0x17), SZ_8M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&gd25qxe_3_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25*128", SNOR_ID(0xc8, 0x40, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_META),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(70),
	),

	SNOR_PART("GD25Q128B", SNOR_ID(0xc8, 0x40, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxb_lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_4_in_1),
	),

	SNOR_PART("GD25Q128C", SNOR_ID(0xc8, 0x40, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_WPS_SR3_BIT2),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25q128c_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_512b),
	),

	SNOR_PART("GD25B127D", SNOR_ID(0xc8, 0x40, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25b127d_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25Q127C", SNOR_ID(0xc8, 0x40, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&gd25q127c_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25B128E", SNOR_ID(0xc8, 0x40, 0x18), SZ_16M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25qxe_3_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25Q128E", SNOR_ID(0xc8, 0x40, 0x18), SZ_16M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25q128e_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25VQ127C", SNOR_ID(0xc8, 0x42, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25q127c_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25L*128", SNOR_ID(0xc8, 0x60, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LB128D", SNOR_ID(0xc8, 0x60, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LB128E", SNOR_ID(0xc8, 0x60, 0x18), SZ_16M,
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
		  SNOR_OPS(&gd25lb128e_ops),
	),

	SNOR_PART("GD25LE128D", SNOR_ID(0xc8, 0x60, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25le128d_alias), /* GD25LQ128D */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LE128E", SNOR_ID(0xc8, 0x60, 0x18), SZ_16M,
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25le128e_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
		  SNOR_FIXUPS(&gd25le128e_fixups),
	),

	SNOR_PART("GD25LQ128E", SNOR_ID(0xc8, 0x60, 0x18), SZ_16M,
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25LF128E", SNOR_ID(0xc8, 0x63, 0x18), SZ_16M,
		  SNOR_VENDOR_FLAGS(GD_F_QPI_DUMMY_10),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25lf128e_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
		  SNOR_FIXUPS(&gd25le128e_fixups),
	),

	SNOR_PART("GD25WQ128E", SNOR_ID(0xc8, 0x65, 0x18), SZ_16M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&gd25q128e_regs),
		  SNOR_WP_RANGES(&gd_3bp_tb_sec_cmp_dummy),
		  SNOR_OTP_INFO(&gd25_otp_3_1k),
	),

	SNOR_PART("GD25*256", SNOR_ID(0xc8, 0x40, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H | SNOR_4B_F_OPCODE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_ERASE_INFO_4B(&gd_erase_opcodes_4b),
		  SNOR_SPI_MAX_SPEED_MHZ(70),
	),

	SNOR_PART("GD25Q256C", SNOR_ID(0xc8, 0x40, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_SFDP_4B_MODE),
		  SNOR_VENDOR_FLAGS(GD_F_WPS_SR3_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H | SNOR_4B_F_OPCODE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_ERASE_INFO_4B(&gd_erase_opcodes_4b),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25q256c_regs),
		  SNOR_WP_RANGES(&gd_wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3),
		  SNOR_OPS(&gd25q256c_part_ops),
		  SNOR_FIXUPS(&gd25q256c_fixups),
	),

	SNOR_PART("GD25B256D", SNOR_ID(0xc8, 0x40, 0x19), SZ_32M,
		  SNOR_ALIAS(&gd25b256d_alias), /* GD25Q256D */
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25b256d_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3_2k),
	),

	SNOR_PART("GD25B256E", SNOR_ID(0xc8, 0x40, 0x19), SZ_32M,
		  SNOR_ALIAS(&gd25b256e_alias), /* GD25Q256E */
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0_1),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd25b256e_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3_2k),
	),

	SNOR_PART("GD25B257D", SNOR_ID(0xc8, 0x40, 0x19), SZ_32M,
		  SNOR_ALIAS(&gd25b257d_alias), /* GD25Q257D */
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25b257d_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3_2k),
		  SNOR_FIXUPS(&gd25b257d_fixups),
	),

	SNOR_PART("GD25R256D", SNOR_ID(0xc8, 0x40, 0x19), SZ_32M,
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25b256d_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3_2k),
	),

	SNOR_PART("GD25L*256", SNOR_ID(0xc8, 0x60, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID | SNOR_F_META),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
	),

	SNOR_PART("GD25LB256D", SNOR_ID(0xc8, 0x60, 0x19), SZ_32M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&gd25lb256d_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
		  SNOR_OTP_INFO(&gd25_otp_2_1k_index_2),
		  SNOR_FIXUPS(&gd25lx256d_fixups),
	),

	SNOR_PART("GD25LE256D", SNOR_ID(0xc8, 0x60, 0x19), SZ_32M, /* SFDP 1.0 */
		  SNOR_ALIAS(&gd25le256d_alias), /* GD25LQ256D */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_UNIQUE_ID),
		  SNOR_VENDOR_FLAGS(GD_F_QPI_4B_OPCODE),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&gd25lb256d_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
		  SNOR_OTP_INFO(&gd25_otp_2_1k_index_2),
		  SNOR_FIXUPS(&gd25lx256d_fixups),
	),

	SNOR_PART("GD25LE255E", SNOR_ID(0xc8, 0x60, 0x19), SZ_32M,
		  SNOR_VENDOR_FLAGS(GD_F_QPI_4B_OPCODE),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd25le255e_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_2_1k_index_2),
		  SNOR_FIXUPS(&gd25le128e_fixups),
	),

	SNOR_PART("GD25LQ255E", SNOR_ID(0xc8, 0x60, 0x19), SZ_32M,
		  SNOR_VENDOR_FLAGS(GD_F_QPI_4B_OPCODE),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd25lb256d_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
		  SNOR_OTP_INFO(&gd25_otp_2_1k_index_2),
	),

	SNOR_PART("GD25LF255E", SNOR_ID(0xc8, 0x63, 0x19), SZ_32M,
		  SNOR_VENDOR_FLAGS(GD_F_QPI_DUMMY_10 | GD_F_QPI_4B_OPCODE),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd25le255e_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_2_1k_index_2),
		  SNOR_FIXUPS(&gd25le128e_fixups),
	),

	SNOR_PART("GD25WB256E", SNOR_ID(0xc8, 0x65, 0x19), SZ_32M,
		  SNOR_ALIAS(&gd25wb256e_alias), /* GD25WQ256D */
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&gd25b256e_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3_2k),
	),

	SNOR_PART("GD25WR256E", SNOR_ID(0xc8, 0x65, 0x19), SZ_32M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25b256e_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3_2k),
	),

	SNOR_PART("GD25LB256E", SNOR_ID(0xc8, 0x67, 0x19), SZ_32M, /* Flag Register */
		  SNOR_VENDOR_FLAGS(GD_F_DC_NVCR1 | GD_F_OTP_LOCK_NVCR2_BIT1 | GD_F_WPS_NVCR4_BIT2),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd25lb256e_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_1_4k),
	),

	SNOR_PART("GD25Q512MC", SNOR_ID(0xc8, 0x40, 0x20), SZ_64M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_SFDP_4B_MODE),
		  SNOR_VENDOR_FLAGS(GD_F_WPS_SR3_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H | SNOR_4B_F_OPCODE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_ERASE_INFO_4B(&gd_erase_opcodes_4b),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25q256c_regs),
		  SNOR_WP_RANGES(&gd_wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3),
		  SNOR_OPS(&gd25q256c_part_ops),
		  SNOR_FIXUPS(&gd25q256c_fixups),
	),

	SNOR_PART("GD25S512MD", SNOR_ID(0xc8, 0x40, 0x19), SZ_32M,
		  SNOR_NDIES(2), /* GD25B256D */
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25b256d_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3_2k),
	),

	SNOR_PART("GD25S513MD", SNOR_ID(0xc8, 0x40, 0x19), SZ_32M,
		  SNOR_NDIES(2), /* GD25B257D */
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25b257d_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3_2k),
		  SNOR_FIXUPS(&gd25s513md_fixups),
	),

	SNOR_PART("GD55B512ME", SNOR_ID(0xc8, 0x40, 0x1a), SZ_64M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0_1),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd25b256e_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3_2k),
	),

	SNOR_PART("GD55F512MF", SNOR_ID(0xc8, 0x43, 0x1a), SZ_64M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0_1),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd55f512mf_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3_2k),
	),

	SNOR_PART("GD25T512ME", SNOR_ID(0xc8, 0x46, 0x1a), SZ_64M, /* SFDP 1.7 */
		  SNOR_VENDOR_FLAGS(GD_F_DC_NVCR1 | GD_F_WPS_NVCR4_BIT2 | GD_F_ECC_NVCR4_BIT0_1 |
				    GD_F_CRC_NVCR4_BIT4_5 | GD_F_IOM_NVCR0 | GD_F_OTP_LOCK_CR_BIT3),
		  SNOR_SPI_MAX_SPEED_MHZ(166),
		  SNOR_REGS(&gd55t512me_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_1_4k),
	),

	SNOR_PART("GD25B512ME", SNOR_ID(0xc8, 0x47, 0x1a), SZ_64M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_NVCR1 | GD_F_WPS_NVCR4_BIT2 | GD_F_OTP_LOCK_CR_BIT3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd25b512me_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_1_4k),
	),

	SNOR_PART("GD55WB512ME", SNOR_ID(0xc8, 0x65, 0x1a), SZ_64M,
		  SNOR_ALIAS(&gd55wb512me_alias), /* GD55WR512ME */
		  SNOR_VENDOR_FLAGS(GD_F_DC_SR3_BIT0_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&gd25b256e_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_3_2k),
	),

	SNOR_PART("GD25LB512ME", SNOR_ID(0xc8, 0x67, 0x1a), SZ_64M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_NVCR1 | GD_F_OTP_LOCK_NVCR2_BIT1 | GD_F_WPS_NVCR4_BIT2),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd25lb256e_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_1_4k),
	),

	SNOR_PART("GD55T01GE", SNOR_ID(0xc8, 0x46, 0x1b), SZ_128M, /* SFDP 1.7 */
		  SNOR_VENDOR_FLAGS(GD_F_DC_NVCR1 | GD_F_WPS_NVCR4_BIT2 | GD_F_ECC_NVCR4_BIT0_1 |
				    GD_F_CRC_NVCR4_BIT4_5 | GD_F_IOM_NVCR0 | GD_F_OTP_LOCK_CR_BIT3),
		  SNOR_SPI_MAX_SPEED_MHZ(166),
		  SNOR_REGS(&gd55t512me_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_1_4k),
	),

	SNOR_PART("GD55B01GE", SNOR_ID(0xc8, 0x47, 0x1b), SZ_128M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_NVCR1 | GD_F_WPS_NVCR4_BIT2 | GD_F_OTP_LOCK_CR_BIT3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd25b512me_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_1_4k),
	),

	SNOR_PART("GD55LB01GE", SNOR_ID(0xc8, 0x67, 0x1b), SZ_128M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_NVCR1 | GD_F_OTP_LOCK_NVCR2_BIT1 | GD_F_WPS_NVCR4_BIT2),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd25lb256e_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_1_4k),
	),

	SNOR_PART("GD55T02GE", SNOR_ID(0xc8, 0x46, 0x1c), SZ_256M, /* SFDP 1.7 */
		  SNOR_VENDOR_FLAGS(GD_F_DC_NVCR1 | GD_F_WPS_NVCR4_BIT2 | GD_F_ECC_NVCR4_BIT0_1 |
				    GD_F_CRC_NVCR4_BIT4_5 | GD_F_IOM_NVCR0 | GD_F_OTP_LOCK_CR_BIT3),
		  SNOR_SPI_MAX_SPEED_MHZ(166),
		  SNOR_REGS(&gd55t512me_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_1_4k),
	),

	SNOR_PART("GD55B02GE", SNOR_ID(0xc8, 0x47, 0x1c), SZ_256M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_NVCR1 | GD_F_WPS_NVCR4_BIT2 | GD_F_OTP_LOCK_CR_BIT3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd25b512me_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_1_4k),
	),

	SNOR_PART("GD55LB02GE", SNOR_ID(0xc8, 0x67, 0x1c), SZ_256M,
		  SNOR_VENDOR_FLAGS(GD_F_DC_NVCR1 | GD_F_OTP_LOCK_NVCR2_BIT1 | GD_F_WPS_NVCR4_BIT2),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&gd25lb256e_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb),
		  SNOR_OTP_INFO(&gd25_otp_1_4k),
	),
};

static ufprog_status gd_part_setup_dummy_cycles_1bit(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp,
						     const struct spi_nor_reg_access *regacc, uint32_t mask)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, regacc, 0, mask, true));
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, regacc, &regval));

	if (regval & mask) {
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = 0;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = 10;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;
	} else {
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = 4;
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = 0;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = 6;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;
	}

	return UFP_OK;
}

static ufprog_status gd_part_setup_dummy_cycles_2bits(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp,
						      const struct spi_nor_reg_access *regacc, uint32_t mask,
						      uint32_t shift)
{
	uint32_t regval, dc;

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, regacc, 0, mask, true));
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, regacc, &regval));
	dc = (regval & mask) >> shift;

	switch (dc) {
	case 0:
	case 2:
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = 4;
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = 0;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = 6;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].ndummy = 4;
		bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].nmode = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].ndummy = 6;
		bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].nmode = 0;
		break;

	case 1:
	case 3:
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = 0;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = 10;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].ndummy = 8;
		bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].nmode = 0;
		bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].ndummy = 10;
		bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].nmode = 0;
	}

	return UFP_OK;
}

static ufprog_status gd_part_setup_dummy_cycles_nvcr(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp,
						     const struct spi_nor_reg_access *regacc)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, regacc, 0xff, 10, true));
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, regacc, &regval));

	bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = regval & 0xff;
	bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;
	bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].ndummy = regval & 0xff;
	bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].nmode = 0;

	bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = regval & 0xff;
	bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;
	bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].ndummy = regval & 0xff;
	bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].nmode = 0;

	return UFP_OK;
}

static ufprog_status gd_pre_param_setup(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	spi_nor_blank_part_fill_default_opcodes(bp);

	if (bp->p.size > SZ_16M) {
		/* Set to a known address mode (3-Byte) */
		STATUS_CHECK_RET(spi_nor_disable_4b_addressing_e9h(snor));
		snor->state.a4b_mode = false;

		if (bp->p.qe_type == QE_UNKNOWN)
			bp->p.qe_type = QE_DONT_CARE;
	}

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
	}

	/* 8/10 dummy cycles will be used for QPI read */
	if (bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].opcode) {
		if ((bp->p.vendor_flags & GD_F_QPI_DUMMY_10) || (bp->p.id.id[1] == 0x63))
			bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 10;
		else
			bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 8;

		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;
	}

	if (bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].opcode) {
		if ((bp->p.vendor_flags & GD_F_QPI_DUMMY_10) || (bp->p.id.id[1] == 0x63))
			bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].ndummy = 10;
		else
			bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].ndummy = 8;

		bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].nmode = 0;
	}

	if (bp->p.vendor_flags & GD_F_DC_SRCR_BIT12)
		STATUS_CHECK_RET(gd_part_setup_dummy_cycles_1bit(snor, bp, &srcr_acc, BIT(12)));
	else if (bp->p.vendor_flags & GD_F_DC_SR3_BIT0)
		STATUS_CHECK_RET(gd_part_setup_dummy_cycles_1bit(snor, bp, &sr3_acc, BIT(0)));
	else if (bp->p.vendor_flags & GD_F_DC_SR3_BIT0_1)
		STATUS_CHECK_RET(gd_part_setup_dummy_cycles_2bits(snor, bp, &sr3_acc, BITS(1, 0), 0));
	else if (bp->p.vendor_flags & GD_F_DC_NVCR1)
		STATUS_CHECK_RET(gd_part_setup_dummy_cycles_nvcr(snor, bp, &gd_vcr_1_acc));

	if ((bp->p.a4b_flags & SNOR_4B_F_OPCODE) && !(bp->p.vendor_flags & GD_F_QPI_4B_OPCODE) &&
	    ((bp->p.read_io_caps & BIT_SPI_MEM_IO_4_4_4) || (bp->p.pp_io_caps & BIT_SPI_MEM_IO_4_4_4))) {
		if (spi_nor_test_io_opcode(snor, bp->read_opcodes_4b, SPI_MEM_IO_4_4_4, 4, SPI_DATA_IN) ||
		    spi_nor_test_io_opcode(snor, bp->pp_opcodes_4b, SPI_MEM_IO_4_4_4, 4, SPI_DATA_OUT)) {
			/* 4B opcodes are not supported in QPI mode */
			bp->p.a4b_flags &= ~SNOR_4B_F_OPCODE;
		}
	}

	/* QE bit requires non-volatile write */
	if ((bp->p.qe_type == QE_SR2_BIT1_WR_SR1 || bp->p.qe_type == QE_SR2_BIT1 ||
	     (bp->p.qe_type == QE_DONT_CARE && (bp->p.read_io_caps & BIT_SPI_MEM_IO_X4))) &&
	    (bp->p.flags & SNOR_F_SR_VOLATILE_WREN_50H)) {
		bp->p.flags &= ~SNOR_F_SR_VOLATILE_WREN_50H;
		bp->p.flags |= SNOR_F_SR_VOLATILE | SNOR_F_SR_NON_VOLATILE;
	}

	if (bp->p.qe_type == QE_SR2_BIT1 || bp->p.regs == &gd25qxc_3_regs || bp->p.regs == &gd25qxe_3_regs) {
		snor->state.reg.cr = &cr_acc;
		snor->state.reg.cr_shift = 0;
	} else {
		snor->state.reg.sr_w = &srcr_acc;
		snor->state.reg.cr = &srcr_acc;
		snor->state.reg.cr_shift = 8;
	}

	if (bp->p.wp_ranges == &gd_3bp_tb_sec_cmp_dummy)
		bp->p.wp_ranges = gd_3bp_tb_sec_cmp;

	return UFP_OK;
}

static ufprog_status gd_secr_otp_1_lock_bit(struct spi_nor *snor, uint32_t index, uint32_t *retbit,
					    const struct spi_nor_reg_access **retacc)
{
	*retbit = GD_OTP_LOCK_BIT;
	*(retacc) = &sr_acc;

	return UFP_OK;
}

static const struct spi_nor_flash_secr_otp_ops gd_secr_otp_1_ops = {
	.otp_lock_bit = gd_secr_otp_1_lock_bit,
};

static const struct spi_nor_flash_part_otp_ops gd_otp_1_ops = {
	.read = secr_otp_read,
	.write = secr_otp_write,
	.erase = secr_otp_erase,
	.lock = secr_otp_lock,
	.locked = secr_otp_locked,
	.secr = &gd_secr_otp_1_ops,
};

static const struct spi_nor_flash_part_otp_ops gd_otp_paged_ops = {
	.read = secr_otp_read_paged,
	.write = secr_otp_write_paged,
	.erase = secr_otp_erase,
	.lock = secr_otp_lock,
	.locked = secr_otp_locked,
};

static ufprog_status gd_otp_lock_nvcr(struct spi_nor *snor, uint32_t index)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &gd_nvcr_2_acc, 0, BIT(0), false));

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &gd_nvcr_2_acc, &regval));
	if (regval & BIT(0))
		return UFP_OK;

	return UFP_FAIL;
}

static ufprog_status gd_otp_locked_nvcr(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &gd_nvcr_2_acc, &regval));
	if (regval & BIT(0))
		*retlocked = true;
	else
		*retlocked = false;

	return UFP_FAIL;
}

static const struct spi_nor_flash_part_otp_ops gd_otp_nvcr_ops = {
	.read = secr_otp_read_paged,
	.write = secr_otp_write_paged,
	.erase = secr_otp_erase,
	.lock = gd_otp_lock_nvcr,
	.locked = gd_otp_locked_nvcr,
};

static ufprog_status gd_otp_lock_cr3_bit1_bit(struct spi_nor *snor, uint32_t index, uint32_t *retbit,
					      const struct spi_nor_reg_access **retacc)
{
	*(retacc) = &cr_acc;

	switch (index) {
	case 0:
		*retbit = 3;
		break;

	default:
		return UFP_INVALID_PARAMETER;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_secr_otp_ops gd_secr_lock_cr3_bit1_ops = {
	.otp_lock_bit = gd_otp_lock_cr3_bit1_bit,
};

static const struct spi_nor_flash_part_otp_ops gd_otp_lock_cr3_bit1_otp_ops = {
	.read = secr_otp_read_paged,
	.write = secr_otp_write_paged,
	.erase = secr_otp_erase,
	.lock = secr_otp_lock,
	.locked = secr_otp_locked,
	.secr = &gd_secr_lock_cr3_bit1_ops,
};

static ufprog_status gd_enter_hpm(struct spi_nor *snor)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_GD_HPM, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_ADDR(3, 0, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_NO_DATA
	);

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_status gd_pre_chip_setup(struct spi_nor *snor)
{
	if (snor->param.vendor_flags & GD_F_OTP_1)
		snor->ext_param.ops.otp = &gd_otp_1_ops;

	if (snor->param.vendor_flags & GD_F_OTP_LOCK_NVCR2_BIT1)
		snor->ext_param.ops.otp = &gd_otp_nvcr_ops;

	if (snor->param.vendor_flags & GD_F_OTP_LOCK_CR_BIT3)
		snor->ext_param.ops.otp = &gd_otp_lock_cr3_bit1_otp_ops;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup gd_default_part_fixups = {
	.pre_param_setup = gd_pre_param_setup,
	.pre_chip_setup = gd_pre_chip_setup,
};

static ufprog_status gd_chip_setup(struct spi_nor *snor)
{
	uint32_t regval;

	if (snor->param.vendor_flags & (GD_F_WPS_SR3_BIT2 | GD_F_WPS_SR3_BIT7)) {
		/* Write-protect selection */
		STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &sr3_acc, &regval));

		if (snor->param.vendor_flags & GD_F_WPS_SR3_BIT2) {
			if (regval & BIT(2))
				snor->state.flags |= SNOR_F_GLOBAL_UNLOCK;
			else
				snor->state.flags &= ~SNOR_F_GLOBAL_UNLOCK;
		} else if (snor->param.vendor_flags & GD_F_WPS_SR3_BIT7) {
			if (regval & BIT(7))
				snor->state.flags |= SNOR_F_GLOBAL_UNLOCK;
			else
				snor->state.flags &= ~SNOR_F_GLOBAL_UNLOCK;
		}
	}

	if (snor->param.vendor_flags & GD_F_WPS_NVCR4_BIT2) {
		/* Write-protect selection */
		STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &gd_vcr_4_acc, &regval));

		if (!(regval & BIT(2)))
			snor->state.flags |= SNOR_F_GLOBAL_UNLOCK;
		else
			snor->state.flags &= ~SNOR_F_GLOBAL_UNLOCK;
	}

	if (snor->param.vendor_flags & GD_F_ECC_NVCR4_BIT0_1)
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &gd_vcr_4_acc, 3, 0, true));

	if (snor->param.vendor_flags & GD_F_CRC_NVCR4_BIT4_5)
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &gd_vcr_4_acc, 0, BITS(5, 4), true));

	if (snor->param.vendor_flags & GD_F_IOM_NVCR0)
		STATUS_CHECK_RET(spi_nor_write_reg_acc(snor, &gd_vcr_0_acc, 0xdf, true));

	if (snor->param.vendor_flags & GD_F_HPM)
		gd_enter_hpm(snor);

	return UFP_OK;
}

static ufprog_status gd_setup_qpi(struct spi_nor *snor, bool enabled)
{
	if (enabled) {
		/* Set QPI read dummy cycles to 8/10 for maximum speed */
		return spi_nor_write_reg(snor, SNOR_CMD_SET_READ_PARAMETERS, QPI_READ_DUMMY_CLOCKS_8_10);
	}

	return UFP_OK;
}

static ufprog_status gd_read_uid_len(struct spi_nor *snor, void *data, uint32_t len)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_READ_UNIQUE_ID, 1),
		SPI_MEM_OP_ADDR(snor->state.a4b_mode ? 4 : 3, 0, 1),
		SPI_MEM_OP_DUMMY(1, 1),
		SPI_MEM_OP_DATA_IN(len, data, 1)
	);

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_status gd_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	if (retlen)
		*retlen = GD_UID_LEN;

	if (!data)
		return UFP_OK;

	return gd_read_uid_len(snor, data, GD_UID_LEN);
}

static const struct spi_nor_flash_part_ops gd_default_part_ops = {
	.otp = &gd_otp_paged_ops,

	.select_die = spi_nor_select_die,
	.chip_setup = gd_chip_setup,
	.read_uid = gd_read_uid,
	.setup_qpi = gd_setup_qpi,
};

static ufprog_status gd_init(void)
{
	gd_3bp_tb_sec_cmp = wp_bp_info_copy(&wpr_3bp_tb_sec_cmp);
	if (!gd_3bp_tb_sec_cmp)
		return UFP_NOMEM;

	gd_3bp_tb_sec_cmp->access = &gd_srcr_acc;

	return UFP_OK;
}

static const struct spi_nor_vendor_ops gd_ops = {
	.init = gd_init,
};

const struct spi_nor_vendor vendor_gigadevice = {
	.mfr_id = SNOR_VENDOR_GIGADEVICE,
	.id = "gigadevice",
	.name = "GigaDevice",
	.parts = gigadevice_parts,
	.nparts = ARRAY_SIZE(gigadevice_parts),
	.vendor_flag_names = gigadevice_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(gigadevice_vendor_flag_info),
	.ops = &gd_ops,
	.default_part_ops = &gd_default_part_ops,
	.default_part_fixups = &gd_default_part_fixups,
};
