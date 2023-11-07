// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Winbond SPI-NOR flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include <ufprog/spi-nor-opcode.h>
#include "core.h"
#include "part.h"
#include "regs.h"
#include "otp.h"
#include "vendor-winbond.h"

#define WINBOND_UID_LEN				8

/* Winbond vendor flags */
#define WINBOND_F_MULTI_DIE				BIT(0)

static const struct spi_nor_part_flag_enum_info winbond_vendor_flag_info[] = {
	{ 0, "multi-die" },
};

static const struct spi_nor_otp_info w25q_otp_3 = {
	.start_index = 1,
	.count = 3,
	.size = 0x100,
};

static const struct spi_nor_otp_info w25q_otp_4 = {
	.start_index = 0,
	.count = 4,
	.size = 0x100,
};

static const struct spi_nor_reg_field_item w25x_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def w25x_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, w25x_sr_fields);

static const struct snor_reg_info w25x_regs = SNOR_REG_INFO(&w25x_sr);

static const struct spi_nor_reg_field_item w25xc_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(7, 1, "SRP", "Status Register Protect"),
};

static const struct spi_nor_reg_def w25xc_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, w25xc_sr_fields);

static const struct snor_reg_info w25xc_regs = SNOR_REG_INFO(&w25xc_sr);

static const struct spi_nor_reg_field_item w25q_sr_4lb_fields[] = {
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
};

static const struct spi_nor_reg_def w25q_sr_4lb = SNOR_REG_DEF("SR", "Status Register", &srcr_acc, w25q_sr_4lb_fields);

static const struct snor_reg_info w25q_4lb_regs = SNOR_REG_INFO(&w25q_sr_4lb);

static const struct spi_nor_reg_field_item w25q_sr3_4b_fields[] = {
	SNOR_REG_FIELD_FULL(1, 1, "ADP", "Power-up Address Mode", &w25q_sr3_adp_values),
	SNOR_REG_FIELD_FULL(2, 1, "WPS", "Write Protection Selection", &w25q_sr3_wps_values),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def w25q_sr3_4b = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
							    w25q_sr3_4b_fields);

static const struct snor_reg_info w25q_2_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2);
static const struct snor_reg_info w25q_3_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &w25q_sr3);
static const struct snor_reg_info w25q_4b_3_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &w25q_sr3_4b);

static ufprog_status w25q16xv_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q16JV");
	}

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q16BV");
}

static const struct spi_nor_flash_part_fixup w25q16xv_fixups = {
	.pre_param_setup = w25q16xv_fixup_model,
};

static ufprog_status w25q16xw_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q16JW");
		else if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q16FW");
	}

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q16DW");
}

static const struct spi_nor_flash_part_fixup w25q16xw_fixups = {
	.pre_param_setup = w25q16xw_fixup_model,
};

static ufprog_status w25q32xv_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	uint32_t dw;

	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A) {
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q32JV");
		} else {
			dw = sfdp_dw(snor->sfdp.bfpt, 5);
			if (dw & BFPT_DW5_SUPPORT_4S_4S_4S_FAST_READ)
				return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q32FV");
		}
	}

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q32BV");
}

static const struct spi_nor_flash_part_fixup w25q32xv_fixups = {
	.pre_param_setup = w25q32xv_fixup_model,
};

static ufprog_status w25q32xw_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q32JW");
		else if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q32FW");
	}

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q32DW");
}

static const struct spi_nor_flash_part_fixup w25q32xw_fixups = {
	.pre_param_setup = w25q32xw_fixup_model,
};

static ufprog_status w25q64xv_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	uint32_t dw;

	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A) {
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q64JV");
		} else {
			dw = sfdp_dw(snor->sfdp.bfpt, 5);
			if (dw & BFPT_DW5_SUPPORT_4S_4S_4S_FAST_READ)
				return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q64FV");
			else
				return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q64CV");
		}
	}

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q64BV");
}

static const struct spi_nor_flash_part_fixup w25q64xv_fixups = {
	.pre_param_setup = w25q64xv_fixup_model,
};

static ufprog_status w25q64xw_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q64JW");
		else if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q64FW");
	}

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q64DW");
}

static const struct spi_nor_flash_part_fixup w25q64xw_fixups = {
	.pre_param_setup = w25q64xw_fixup_model,
};

static ufprog_status w25q128xv_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	uint32_t dw;

	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A) {
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q128JV");
		} else {
			dw = sfdp_dw(snor->sfdp.bfpt, 5);
			if (dw & BFPT_DW5_SUPPORT_4S_4S_4S_FAST_READ)
				return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q128FV");
		}
	}

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q128BV");
}

static const struct spi_nor_flash_part_fixup w25q128xv_fixups = {
	.pre_param_setup = w25q128xv_fixup_model,
};

static ufprog_status w25q128xw_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_B)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q128JW");
		else if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q128FW");
	}
	return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q128DW");
}

static const struct spi_nor_flash_part_fixup w25q128xw_fixups = {
	.pre_param_setup = w25q128xw_fixup_model,
};

static ufprog_status w25q256xv_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	uint32_t dw;

	if (!snor->sfdp.bfpt)
		return UFP_OK;

	if (snor->sfdp.bfpt_hdr->minor_ver == SFDP_REV_MINOR_A) {
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q256JV");
	} else {
		dw = sfdp_dw(snor->sfdp.bfpt, 5);
		if (dw & BFPT_DW5_SUPPORT_4S_4S_4S_FAST_READ)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q256FV");
	}

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "W25Q256BV");
}

static const struct spi_nor_flash_part_fixup w25q256xv_fixups = {
	.pre_param_setup = w25q256xv_fixup_model,
};

static DEFINE_SNOR_ALIAS(w25q80_alias, SNOR_ALIAS_MODEL("W25Q80BV"), SNOR_ALIAS_MODEL("W25Q80DV"));
static DEFINE_SNOR_ALIAS(w25q16bv_alias, SNOR_ALIAS_MODEL("W25Q16CV"), SNOR_ALIAS_MODEL("W25Q16DV"));

static const struct spi_nor_flash_part winbond_parts[] = {
	SNOR_PART("W25X05", SNOR_ID(0xef, 0x30, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&w25x_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("W25X05C", SNOR_ID(0xef, 0x30, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25xc_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("W25X10", SNOR_ID(0xef, 0x30, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&w25x_regs),
		  SNOR_WP_RANGES(&wpr_2bp_tb),
	),

	SNOR_PART("W25X10CL", SNOR_ID(0xef, 0x30, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25xc_regs),
		  SNOR_WP_RANGES(&wpr_2bp_tb),
	),

	SNOR_PART("W25Q10EW", SNOR_ID(0xef, 0x60, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_2_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec),
	),

	SNOR_PART("W25X20", SNOR_ID(0xef, 0x30, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&w25x_regs),
		  SNOR_WP_RANGES(&wpr_2bp_tb),
	),

	SNOR_PART("W25X20C", SNOR_ID(0xef, 0x30, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25xc_regs),
		  SNOR_WP_RANGES(&wpr_2bp_tb),
	),

	SNOR_PART("W25Q20CL", SNOR_ID(0xef, 0x40, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25q_4lb_regs),
		  SNOR_OTP_INFO(&w25q_otp_4),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q20BW", SNOR_ID(0xef, 0x50, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25q_4lb_regs),
		  SNOR_OTP_INFO(&w25q_otp_4),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q20EW", SNOR_ID(0xef, 0x60, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_2_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25X40", SNOR_ID(0xef, 0x30, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&w25x_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("W25X40CL", SNOR_ID(0xef, 0x30, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25x_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("W25Q40", SNOR_ID(0xef, 0x40, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&w25q_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q40CL", SNOR_ID(0xef, 0x40, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25q_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q40BW", SNOR_ID(0xef, 0x50, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25q_4lb_regs),
		  SNOR_OTP_INFO(&w25q_otp_4),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q40EW", SNOR_ID(0xef, 0x60, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_2_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25X80", SNOR_ID(0xef, 0x30, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&w25x_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("W25Q80", SNOR_ID(0xef, 0x40, 0x14), SZ_1M,
		  SNOR_ALIAS(&w25q80_alias), /* W25Q80BV/W25Q80DV */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25q_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q80BW", SNOR_ID(0xef, 0x50, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25q_4lb_regs),
		  SNOR_OTP_INFO(&w25q_otp_4),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q80EW", SNOR_ID(0xef, 0x60, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_DPI | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_2_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25X16", SNOR_ID(0xef, 0x30, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&w25x_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("W25Q16", SNOR_ID(0xef, 0x40, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&w25q16xv_fixups),
	),

	SNOR_PART("W25Q16BV", SNOR_ID(0xef, 0x40, 0x15), SZ_2M,
		  SNOR_ALIAS(&w25q16bv_alias), /* W25Q16CV/W25Q16DV */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q16CL", SNOR_ID(0xef, 0x40, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q16JV", SNOR_ID(0xef, 0x40, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_2_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q16JV-DTR", SNOR_ID(0xef, 0x70, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q16*W", SNOR_ID(0xef, 0x60, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_META),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&w25q16xw_fixups),
	),

	SNOR_PART("W25Q16DW", SNOR_ID(0xef, 0x60, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&w25q_otp_4),
		  SNOR_REGS(&w25q_4lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q16FW", SNOR_ID(0xef, 0x60, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q16JW", SNOR_ID(0xef, 0x60, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q16JW-DTR", SNOR_ID(0xef, 0x80, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25X32", SNOR_ID(0xef, 0x30, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&w25x_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("W25X32BV", SNOR_ID(0xef, 0x30, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25x_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("W25Q32", SNOR_ID(0xef, 0x40, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&w25q32xv_fixups),
	),

	SNOR_PART("W25Q32BV", SNOR_ID(0xef, 0x40, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q32FV", SNOR_ID(0xef, 0x40, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q32JV", SNOR_ID(0xef, 0x40, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q32JV-DTR", SNOR_ID(0xef, 0x70, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q32*W", SNOR_ID(0xef, 0x60, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_META),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&w25q32xw_fixups),
	),

	SNOR_PART("W25Q32DW", SNOR_ID(0xef, 0x60, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&w25q_otp_4),
		  SNOR_REGS(&w25q_4lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q32FW", SNOR_ID(0xef, 0x60, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q32JW", SNOR_ID(0xef, 0x60, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25Q32JW-DTR", SNOR_ID(0xef, 0x80, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("W25X64", SNOR_ID(0xef, 0x30, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&w25x_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_ratio),
	),

	SNOR_PART("W25X64BV", SNOR_ID(0xef, 0x30, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25x_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_ratio),
	),

	SNOR_PART("W25Q64", SNOR_ID(0xef, 0x40, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_FIXUPS(&w25q64xv_fixups),
	),

	SNOR_PART("W25Q64BV", SNOR_ID(0xef, 0x40, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&w25q_no_lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_ratio),
	),

	SNOR_PART("W25Q64CV", SNOR_ID(0xef, 0x40, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q64FV", SNOR_ID(0xef, 0x40, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q64JV", SNOR_ID(0xef, 0x40, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q64JV-DTR", SNOR_ID(0xef, 0x70, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q64*W", SNOR_ID(0xef, 0x60, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_META),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_FIXUPS(&w25q64xw_fixups),
	),

	SNOR_PART("W25Q64DW", SNOR_ID(0xef, 0x60, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&w25q_otp_4),
		  SNOR_REGS(&w25q_4lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_ratio),
	),

	SNOR_PART("W25Q64FW", SNOR_ID(0xef, 0x60, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q64JW", SNOR_ID(0xef, 0x60, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q64JW-DTR", SNOR_ID(0xef, 0x80, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q128", SNOR_ID(0xef, 0x40, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_FIXUPS(&w25q128xv_fixups),
	),

	SNOR_PART("W25Q128BV", SNOR_ID(0xef, 0x40, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q128FV", SNOR_ID(0xef, 0x40, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q128JV", SNOR_ID(0xef, 0x40, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q128JV-DTR", SNOR_ID(0xef, 0x70, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q128*W", SNOR_ID(0xef, 0x60, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_META),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_FIXUPS(&w25q128xw_fixups),
	),

	SNOR_PART("W25Q128DW", SNOR_ID(0xef, 0x60, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&w25q_otp_4),
		  SNOR_REGS(&w25q_4lb_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q128FW", SNOR_ID(0xef, 0x60, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q128JW", SNOR_ID(0xef, 0x60, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q128JW-DTR", SNOR_ID(0xef, 0x80, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
	),

	SNOR_PART("W25Q256", SNOR_ID(0xef, 0x40, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
		  SNOR_FIXUPS(&w25q256xv_fixups),
	),

	SNOR_PART("W25Q256BV", SNOR_ID(0xef, 0x40, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q256FV", SNOR_ID(0xef, 0x40, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_SFDP_4B_MODE |
			     SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q256JV", SNOR_ID(0xef, 0x40, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_UNIQUE_ID |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR2_BIT1,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H | SNOR_4B_F_OPCODE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q256JV-DTR", SNOR_ID(0xef, 0x70, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q256JW", SNOR_ID(0xef, 0x60, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q256JW-DTR", SNOR_ID(0xef, 0x80, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q512JV", SNOR_ID(0xef, 0x40, 0x20), SZ_64M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(103), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q512JV-DTR", SNOR_ID(0xef, 0x70, 0x20), SZ_64M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(103), SNOR_DUAL_MAX_SPEED_MHZ(90), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q512NW", SNOR_ID(0xef, 0x60, 0x20), SZ_64M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q512NW-DTR", SNOR_ID(0xef, 0x80, 0x20), SZ_64M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25M512JV", SNOR_ID(0xef, 0x71, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_NDIES(2), /* W25Q256JV */
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25M512JW", SNOR_ID(0xef, 0x61, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_NDIES(2), /* W25Q256JW */
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q01JV", SNOR_ID(0xef, 0x40, 0x21), SZ_128M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(WINBOND_F_MULTI_DIE),
		  SNOR_SPI_MAX_SPEED_MHZ(103), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q01JV-DTR", SNOR_ID(0xef, 0x70, 0x21), SZ_128M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(WINBOND_F_MULTI_DIE),
		  SNOR_SPI_MAX_SPEED_MHZ(103), SNOR_DUAL_MAX_SPEED_MHZ(90), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q01NW", SNOR_ID(0xef, 0x60, 0x21), SZ_128M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(WINBOND_F_MULTI_DIE),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q01NW-DTR", SNOR_ID(0xef, 0x80, 0x21), SZ_128M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(WINBOND_F_MULTI_DIE),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
	),

	SNOR_PART("W25Q02JV-DTR", SNOR_ID(0xef, 0x70, 0x22), SZ_256M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(WINBOND_F_MULTI_DIE),
		  SNOR_SPI_MAX_SPEED_MHZ(103), SNOR_DUAL_MAX_SPEED_MHZ(90), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  /* SNOR_WP_RANGES(&wpr_4bp_tb_cmp), // FIXME: not supported now */
	),

	SNOR_PART("W25Q02NW-DTR", SNOR_ID(0xef, 0x80, 0x22), SZ_256M,
		  SNOR_FLAGS(SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_VENDOR_FLAGS(WINBOND_F_MULTI_DIE),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&w25q_4b_3_regs),
		  SNOR_OTP_INFO(&w25q_otp_3),
		  /* SNOR_WP_RANGES(&wpr_4bp_tb_cmp), // FIXME: not supported now */
	),
};

static ufprog_status winbond_part_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					struct spi_nor_flash_part_blank *bp)
{
	uint8_t sr3;

	spi_nor_blank_part_fill_default_opcodes(bp);

	if (bp->p.size > SZ_16M) {
		/* Set to a known address mode (3-Byte) */
		STATUS_CHECK_RET(spi_nor_disable_4b_addressing_e9h(snor));
		snor->state.a4b_mode = false;
	}

	if (snor->sfdp.bfpt && snor->sfdp.bfpt_hdr->minor_ver >= SFDP_REV_MINOR_A) {
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
			bp->p.otp = &w25q_otp_3;

		if (snor->sfdp.bfpt_hdr->minor_ver >= SFDP_REV_MINOR_B) {
			if (!bp->p.regs) {
				if (bp->p.size >= SZ_32M)
					bp->p.regs = &w25q_4b_3_regs;
				else
					bp->p.regs = &w25q_3_regs;
			}
		}
	}

	/* 8 dummy cycles will be used for QPI read */
	if (bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].opcode) {
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;
	}

	if (bp->p.size > SZ_16M && (bp->p.a4b_flags & SNOR_4B_F_OPCODE)) {
		if (spi_nor_test_io_opcode(snor, bp->read_opcodes_4b, SPI_MEM_IO_4_4_4, 4, SPI_DATA_IN) ||
		    spi_nor_test_io_opcode(snor, bp->pp_opcodes_4b, SPI_MEM_IO_4_4_4, 4, SPI_DATA_OUT)) {
			/* 4B opcodes are not supported in QPI mode */
			bp->p.a4b_flags &= ~SNOR_4B_F_OPCODE;
		}
	}

	if (bp->p.size > SZ_32M && !bp->p.wp_ranges)
		bp->p.wp_ranges = &wpr_4bp_tb_cmp;

	if (bp->p.regs == &w25q_3_regs || bp->p.regs == &w25q_4b_3_regs) {
		STATUS_CHECK_RET(spi_nor_read_reg(snor, SNOR_CMD_READ_SR3, &sr3));

		if (sr3 & SR3_WPS)
			bp->p.flags |= SNOR_F_GLOBAL_UNLOCK;
		else
			bp->p.flags &= ~SNOR_F_GLOBAL_UNLOCK;
	}

	if (bp->p.vendor_flags & WINBOND_F_MULTI_DIE)
		snor->state.die_read_granularity = SZ_64M;

	if (bp->p.regs == &w25x_regs || bp->p.regs == &w25xc_regs || bp->p.regs == &w25q_2_regs ||
	    bp->p.regs == &w25q_3_regs || bp->p.regs == &w25q_4b_3_regs) {
		snor->state.reg.cr = &cr_acc;
		snor->state.reg.cr_shift = 0;
	} else {
		snor->state.reg.sr_w = &srcr_acc;
		snor->state.reg.cr = &srcr_acc;
		snor->state.reg.cr_shift = 8;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup winbond_fixups = {
	.pre_param_setup = winbond_part_fixup,
};

static ufprog_status winbond_setup_qpi(struct spi_nor *snor, bool enabled)
{
	if (enabled) {
		/* Set QPI read dummy cycles to 8 for maximum speed */
		return spi_nor_write_reg(snor, SNOR_CMD_SET_READ_PARAMETERS, QPI_READ_DUMMY_CLOCKS_8);
	}

	return UFP_OK;
}

static ufprog_status winbond_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_READ_UNIQUE_ID, 1),
		SPI_MEM_OP_NO_ADDR,
		SPI_MEM_OP_DUMMY(snor->state.a4b_mode ? 5 : 4, 1),
		SPI_MEM_OP_DATA_IN(WINBOND_UID_LEN, data, 1)
	);

	if (retlen)
		*retlen = WINBOND_UID_LEN;

	if (!data)
		return UFP_OK;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static const struct spi_nor_flash_part_ops winbond_ops = {
	.otp = &secr_otp_ops,

	.read_uid = winbond_read_uid,
	.select_die = spi_nor_select_die,
	.setup_qpi = winbond_setup_qpi,
	.qpi_dis = spi_nor_disable_qpi_ffh,
};

const struct spi_nor_vendor vendor_winbond = {
	.mfr_id = SNOR_VENDOR_WINBOND,
	.id = "winbond",
	.name = "Winbond",
	.parts = winbond_parts,
	.nparts = ARRAY_SIZE(winbond_parts),
	.default_part_ops = &winbond_ops,
	.default_part_fixups = &winbond_fixups,
	.vendor_flag_names = winbond_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(winbond_vendor_flag_info),
};
