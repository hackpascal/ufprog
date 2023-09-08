// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SST/Microchip SPI-NOR flash parts
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <ufprog/log.h>
#include <ufprog/sizes.h>
#include <ufprog/spi-nor-opcode.h>
#include "core.h"
#include "part.h"
#include "regs.h"
#include "ext_id.h"

/* SST SR bits */
#define SST_SR_SEC_LOCKED			BIT(5)

/* SST vendor flags */
#define SST_F_PWRON_BLK_LOCKS			BIT(0)

static const struct spi_nor_part_flag_enum_info sst_vendor_flag_info[] = {
	{ 0, "power-on-block-locks" },
};

static const struct spi_nor_reg_access sst_qpi_read_sr_acc = {
	.type = SNOR_REG_NORMAL,
	.read_opcode = SNOR_CMD_READ_SR,
	.write_opcode = SNOR_CMD_WRITE_SR,
	.ndummy_read = 1,
	.ndata = 1,
};

static const struct spi_nor_otp_info sst_otp_800h_16b = {
	.start_index = 0x10,
	.count = 1,
	.size = 0x7f0,
};

static const struct spi_nor_otp_info sst_otp_800h_8b = {
	.start_index = 8,
	.count = 1,
	.size = 0x7f8,
};

static const struct spi_nor_otp_info sst_otp_20h_8b = {
	.start_index = 8,
	.count = 1,
	.size = 0x18,
};

static const struct spi_nor_reg_field_values sst_cr_rsthld_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "RST# pin enabled"),
	VALUE_ITEM(1, "HOLD# pin enabled"),
);

static const struct spi_nor_reg_field_values sst_cr_wpen_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "WP# enabled"),
	VALUE_ITEM(1, "WP# disabled"),
);

static const struct spi_nor_reg_field_item sst_srcr_only_wpen_rst_hold_fields[] = {
	SNOR_REG_FIELD_FULL(14, 1, "RSTHLD", "RST# pin or HOLD# Pin Enable", &sst_cr_rsthld_values),
	SNOR_REG_FIELD_FULL(15, 1, "WPEN", "Write Protection Pin (WP#) Enable", &sst_cr_wpen_values),
};

static const struct spi_nor_reg_def sst_srcr_only_wp_rst_hold = SNOR_REG_DEF("SR", "Status Register", &srcr_acc,
									     sst_srcr_only_wpen_rst_hold_fields);

static const struct snor_reg_info sst_srcr_only_wp_rst_hold_regs = SNOR_REG_INFO(&sst_srcr_only_wp_rst_hold);

static const struct spi_nor_reg_field_item sst_srcr_only_wpen_fields[] = {
	SNOR_REG_FIELD_FULL(15, 1, "WPEN", "Write Protection Pin (WP#) Enable", &sst_cr_wpen_values),
};

static const struct spi_nor_reg_def sst_srcr_only_wpen = SNOR_REG_DEF("SR", "Status Register", &srcr_acc,
								      sst_srcr_only_wpen_fields);

static const struct snor_reg_info sst_srcr_only_wpen_regs = SNOR_REG_INFO(&sst_srcr_only_wpen);

static const struct spi_nor_io_opcode sst25vf512_read_opcodes[__SPI_MEM_IO_MAX] = {
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_1, SNOR_CMD_READ, 0, 0),
};

static const struct spi_nor_io_opcode sst25vf064c_pp_opcodes[__SPI_MEM_IO_MAX] = {
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_1, SNOR_CMD_PAGE_PROG, 0, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_2, SNOR_CMD_PAGE_PROG_DUAL_IN, 0, 0),
};

static const struct spi_nor_flash_part sst_parts[] = {
	SNOR_PART("SST25VF512", SNOR_ID_NONE, SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_VOLATILE_WREN_50H |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_READ_OPCODES(sst25vf512_read_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(20),
	),

	SNOR_PART("SST25VF512A", SNOR_ID_NONE, SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_VOLATILE_WREN_50H |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(33),
	),

	SNOR_PART("SST25WF512", SNOR_ID(0xbf, 0x25, 0x01), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("SST25VF010A", SNOR_ID_NONE, SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_VOLATILE_WREN_50H |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(33),
	),

	SNOR_PART("SST25WF010", SNOR_ID(0xbf, 0x25, 0x02), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("SST25LF020A", SNOR_ID_NONE, SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_VOLATILE_WREN_50H |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(33),
	),

	SNOR_PART("SST25PF020B", SNOR_ID(0xbf, 0x25, 0x8c), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("SST25VF020B", SNOR_ID(0xbf, 0x25, 0x8c), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
	),

	SNOR_PART("SST25WF020", SNOR_ID(0xbf, 0x25, 0x03), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("SST25WF020A", SNOR_ID(0x62, 0x16, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("SST26VF020A", SNOR_ID(0xbf, 0x26, 0x12), SZ_256K,
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&sst_srcr_only_wp_rst_hold_regs),
		  SNOR_OTP_INFO(&sst_otp_800h_16b),
	),

	SNOR_PART("SST25PF040C", SNOR_ID(0x62, 0x06, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("SST25VF040B", SNOR_ID(0xbf, 0x25, 0x8d), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("SST25WF040", SNOR_ID(0xbf, 0x25, 0x04), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("SST25WF040B", SNOR_ID(0x62, 0x16, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("SST26VF040A", SNOR_ID(0xbf, 0x26, 0x14), SZ_512K,
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&sst_srcr_only_wp_rst_hold_regs),
		  SNOR_OTP_INFO(&sst_otp_800h_16b),
	),

	SNOR_PART("SST26WF040B", SNOR_ID(0xbf, 0x26, 0x54), SZ_512K,
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&sst_srcr_only_wpen_regs),
		  SNOR_OTP_INFO(&sst_otp_800h_8b),
	),

	SNOR_PART("SST25VF080B", SNOR_ID(0xbf, 0x25, 0x84), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("SST25WF080", SNOR_ID(0xbf, 0x25, 0x05), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75),
	),

	SNOR_PART("SST25WF080B", SNOR_ID(0x62, 0x16, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("SST26VF080A", SNOR_ID(0xbf, 0x26, 0x18), SZ_1M,
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&sst_srcr_only_wp_rst_hold_regs),
		  SNOR_OTP_INFO(&sst_otp_800h_16b),
	),

	SNOR_PART("SST26WF080B", SNOR_ID(0xbf, 0x26, 0x58), SZ_1M,
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&sst_srcr_only_wpen_regs),
		  SNOR_OTP_INFO(&sst_otp_800h_8b),
	),

	SNOR_PART("SST25VF016B", SNOR_ID(0xbf, 0x25, 0x41), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("SST26VF016", SNOR_ID(0xbf, 0x26, 0x01), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K),
		  SNOR_VENDOR_FLAGS(SST_F_PWRON_BLK_LOCKS),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&sst_otp_20h_8b),
	),

	SNOR_PART("SST26VF016B", SNOR_ID(0xbf, 0x26, 0x41), SZ_2M,
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&sst_otp_800h_8b),
	),

	SNOR_PART("SST26WF016B", SNOR_ID(0xbf, 0x26, 0x51), SZ_2M,
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&sst_srcr_only_wpen_regs),
		  SNOR_OTP_INFO(&sst_otp_800h_8b),
	),

	SNOR_PART("SST25VF032B", SNOR_ID(0xbf, 0x25, 0x4a), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(66),
	),

	SNOR_PART("SST26VF032", SNOR_ID(0xbf, 0x26, 0x02), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K),
		  SNOR_VENDOR_FLAGS(SST_F_PWRON_BLK_LOCKS),
		  SNOR_QE_DONT_CARE, SNOR_QPI_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&sst_otp_20h_8b),
	),

	SNOR_PART("SST26VF032B", SNOR_ID(0xbf, 0x26, 0x42), SZ_4M,
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&sst_otp_800h_8b),
	),

	SNOR_PART("SST26WF032", SNOR_ID(0xbf, 0x26, 0x22), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SR_VOLATILE),
		  SNOR_VENDOR_FLAGS(SST_F_PWRON_BLK_LOCKS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&sst_otp_20h_8b),
	),

	SNOR_PART("SST25VF064C", SNOR_ID(0xbf, 0x25, 0x4b), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_OPCODES(sst25vf064c_pp_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(80), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_OTP_INFO(&sst_otp_20h_8b),
	),

	SNOR_PART("SST26VF064B", SNOR_ID(0xbf, 0x26, 0x43), SZ_8M,
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_OTP_INFO(&sst_otp_800h_8b),
	),

	SNOR_PART("SST26WF064C", SNOR_ID(0xbf, 0x26, 0x53), SZ_8M,
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&sst_srcr_only_wp_rst_hold_regs),
		  SNOR_OTP_INFO(&sst_otp_800h_8b),
	),
};

static ufprog_status sst_part_fixup(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
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
	}

	if (bp->p.otp)
		bp->p.flags |= SNOR_F_UNIQUE_ID;

	return UFP_OK;
}

static ufprog_status sst_post_param_setup(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	if (snor->state.cmd_buswidth_curr == 4) {
		snor->state.reg.sr_r = &sst_qpi_read_sr_acc;
		snor->state.reg.sr_w = &sst_qpi_read_sr_acc;
	}

	snor->param.max_pp_time_ms = SNOR_PP_TIMEOUT_MS;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup sst_fixups = {
	.pre_param_setup = sst_part_fixup,
	.post_param_setup = sst_post_param_setup,
};

static ufprog_status sst_read_sid(struct spi_nor *snor, uint32_t addr, uint32_t len, void *data)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_READ_SID, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_ADDR(2, addr, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_DUMMY(1, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_DATA_IN(len, data, snor->state.cmd_buswidth_curr)
	);

	if (snor->ext_param.otp->size <= UCHAR_MAX)
		op.addr.len = 1;

	if (snor->state.cmd_buswidth_curr == 4)
		op.dummy.len = 3;

	if (!ufprog_spi_mem_supports_op(snor->spi, &op))
		return UFP_UNSUPPORTED;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));

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

static ufprog_status sst_write_sid_page(struct spi_nor *snor, uint32_t addr, uint32_t len, const void *data,
					uint32_t *retlen)
{
	uint32_t proglen;

	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_PROG_SID, 1),
		SPI_MEM_OP_ADDR(2, addr, 1),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_DATA_OUT(len, data, 1)
	);

	if (snor->ext_param.otp->size <= UCHAR_MAX)
		op.addr.len = 1;

	if (!ufprog_spi_mem_supports_op(snor->spi, &op))
		return UFP_UNSUPPORTED;

	proglen = snor->param.page_size - (addr & (snor->param.page_size - 1));
	if (proglen > len)
		proglen = len;

	len = proglen;
	op.data.len = proglen;

	while (proglen) {
		STATUS_CHECK_RET(spi_nor_write_enable(snor));

		STATUS_CHECK_RET(ufprog_spi_mem_adjust_op_size(snor->spi, &op));
		STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));

		STATUS_CHECK_RET(spi_nor_wait_busy(snor, SNOR_PP_TIMEOUT_MS));

		op.data.buf.tx = (const void *)((uintptr_t)op.data.buf.tx + op.data.len);

		addr += (uint32_t)op.data.len;
		op.addr.val = addr;

		proglen -= (uint32_t)op.data.len;
		op.data.len = proglen;
	}

	if (retlen)
		*retlen = len;

	return UFP_OK;
}

static ufprog_status sst_write_sid(struct spi_nor *snor, uint32_t addr, uint32_t len, const void *data)
{
	const uint8_t *p = data;
	uint32_t retlen;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));

	while (len) {
		STATUS_CHECK_RET(sst_write_sid_page(snor, addr, len, p, &retlen));

		addr += retlen;
		p += retlen;
		len -= retlen;
	}

	return UFP_OK;
}

static ufprog_status sst_otp_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	return sst_read_sid(snor, snor->ext_param.otp->start_index + addr, len, data);
}

static ufprog_status sst_otp_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, const void *data)
{
	return sst_write_sid(snor, snor->ext_param.otp->start_index + addr, len, data);
}

static ufprog_status sst_otp_lock(struct spi_nor *snor, uint32_t index)
{
	uint8_t sr;

	STATUS_CHECK_RET(spi_nor_write_enable(snor));
	STATUS_CHECK_RET(spi_nor_issue_single_opcode(snor, SNOR_CMD_LOCK_SID));
	STATUS_CHECK_RET(spi_nor_wait_busy(snor, SNOR_PP_TIMEOUT_MS));
	STATUS_CHECK_RET(spi_nor_write_disable(snor));	/* In case some models do not use WREN */

	/* Check result now */
	STATUS_CHECK_RET(spi_nor_read_sr(snor, &sr));

	if (sr & SST_SR_SEC_LOCKED)
		return UFP_OK;

	return UFP_FAIL;
}

static ufprog_status sst_otp_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{
	uint8_t sr;

	STATUS_CHECK_RET(spi_nor_read_sr(snor, &sr));

	if (sr & SST_SR_SEC_LOCKED)
		*retlocked = true;
	else
		*retlocked = false;

	return UFP_OK;
}

const struct spi_nor_flash_part_otp_ops sst_otp_ops = {
	.read = sst_otp_read,
	.write = sst_otp_write,
	.lock = sst_otp_lock,
	.locked = sst_otp_locked,
};

static ufprog_status sst_chip_setup(struct spi_nor *snor)
{
	if (snor->param.vendor_flags & SST_F_PWRON_BLK_LOCKS) {
		uint8_t bpdata[10];

		struct ufprog_spi_mem_op op = SPI_MEM_OP(
			SPI_MEM_OP_CMD(SNOR_CMD_WRITE_BPR, 1),
			SPI_MEM_OP_NO_ADDR,
			SPI_MEM_OP_NO_DUMMY,
			SPI_MEM_OP_DATA_OUT(0, bpdata, 1)
		);

		switch (snor->param.size) {
		case SZ_2M:
			op.data.len = 6;
			break;

		case SZ_4M:
			op.data.len = 10;
			break;

		default:
			logm_err("Unsupported flash size for per-block unlocking\n");
			return UFP_UNSUPPORTED;
		}

		memset(bpdata, 0, op.data.len);

		if (!ufprog_spi_mem_supports_op(snor->spi, &op)) {
			logm_err("Controller does not support command for unlocking all blocks\n");
			return UFP_UNSUPPORTED;
		}

		STATUS_CHECK_RET(spi_nor_write_enable(snor));

		STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));
	}

	return UFP_OK;
}

static ufprog_status sst_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	if (retlen)
		*retlen = snor->ext_param.otp->start_index;

	if (!data)
		return UFP_OK;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return sst_read_sid(snor, 0, snor->ext_param.otp->start_index, data);
}

static ufprog_status sst_setup_qpi(struct spi_nor *snor, bool enabled)
{
	if (enabled) {
		snor->state.reg.sr_r = &sst_qpi_read_sr_acc;
		snor->state.reg.sr_w = &sst_qpi_read_sr_acc;
	} else {
		snor->state.reg.sr_r = &sr_acc;
		snor->state.reg.sr_w = &sr_acc;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_ops sst_ops = {
	.otp = &sst_otp_ops,

	.chip_setup = sst_chip_setup,
	.setup_qpi = sst_setup_qpi,
	.read_uid = sst_read_uid,
	.qpi_dis = spi_nor_disable_qpi_ffh,
};

const struct spi_nor_vendor vendor_sst = {
	.mfr_id = SNOR_VENDOR_SST,
	.id = "sst",
	.name = "Microchip/SST",
	.parts = sst_parts,
	.nparts = ARRAY_SIZE(sst_parts),
	.vendor_flag_names = sst_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(sst_vendor_flag_info),
	.default_part_ops = &sst_ops,
	.default_part_fixups = &sst_fixups,
};
