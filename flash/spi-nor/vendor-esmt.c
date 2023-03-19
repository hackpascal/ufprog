// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ESMT SPI-NOR flash parts
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

/* Security Register bits */
#define SCUR_WPSEL					BIT(7)

 /* ESMT vendor flags */
#define ESMT_F_OTP_NO_EXSO				BIT(0)

static struct spi_nor_part_flag_enum_info esmt_vendor_flag_info[] = {
	{ 0, "otp-no-exso" },
};

static const struct spi_nor_reg_field_item f25lxpa_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(7, 1, "BPL", "Block Protection Lock-Down"),
};

static const struct spi_nor_reg_def f25lxpa_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, f25lxpa_sr_fields);

static const struct snor_reg_info f25lxpa_regs = SNOR_REG_INFO(&f25lxpa_sr);

static const struct spi_nor_reg_field_item f25lxqa_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD_YES_NO(6, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(7, 1, "BPL", "Block Protection Lock-Down"),
};

static const struct spi_nor_reg_def f25lxqa_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, f25lxqa_sr_fields);

static const struct snor_reg_info f25lxqa_regs = SNOR_REG_INFO(&f25lxqa_sr);

static const struct spi_nor_reg_field_item f25dxqa_scur_fields[] = {
	SNOR_REG_FIELD_YES_NO(1, 1, "LDSO", "Lock-down Secured OTP"),
	SNOR_REG_FIELD(7, 1, "WPSEL", "Write Protection Selection"),
};

static const struct spi_nor_reg_def f25dxqa_scur = SNOR_REG_DEF("SCUR", "Security Register", &scur_acc,
								f25dxqa_scur_fields);

static const struct snor_reg_info f25dxqa_regs = SNOR_REG_INFO(&f25lxqa_sr, &f25dxqa_scur);

static const struct spi_nor_otp_info esmt_otp_512b = {
	.start_index = 0,
	.count = 1,
	.size = 0x200,
};

static const struct spi_nor_otp_info esmt_otp_512b_16b = {
	.start_index = 0x10,
	.count = 1,
	.size = 0x1f0,
};

static const struct spi_nor_otp_info esmt_otp_4k = {
	.start_index = 0,
	.count = 1,
	.size = 0x1000,
};

static DEFINE_SNOR_ALIAS(f25l004a_alias, "F25S004A");
static DEFINE_SNOR_ALIAS(f25l04pa_alias, "F25S04PA");

static const struct spi_nor_flash_part esmt_parts[] = {
	SNOR_PART("F25L05PA", SNOR_ID(0x8c, 0x30, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&f25lxpa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("F25L01PA", SNOR_ID(0x8c, 0x30, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&f25lxpa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("F25L02PA", SNOR_ID(0x8c, 0x30, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&f25lxpa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("F25L04UA", SNOR_ID(0x8c, 0x8c, 0x8c), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SR_VOLATILE | SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("F25L004A", SNOR_ID(0x8c, 0x20, 0x13), SZ_512K,
		  SNOR_ALIAS(&f25l004a_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("F25L04PA", SNOR_ID(0x8c, 0x30, 0x13), SZ_512K,
		  SNOR_ALIAS(&f25l04pa_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&f25lxpa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("F25L008A", SNOR_ID(0x8c, 0x20, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("F25L008A(Bottom)", SNOR_ID(0x8c, 0x21, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("F25L08PA", SNOR_ID(0x8c, 0x20, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE),
		  SNOR_VENDOR_FLAGS(ESMT_F_OTP_NO_EXSO),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_OTP_INFO(&esmt_otp_4k),
	),

	SNOR_PART("F25L08QA", SNOR_ID(0x8c, 0x40, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ESMT_F_OTP_NO_EXSO),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&f25lxqa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&esmt_otp_512b),
	),

	SNOR_PART("F25D08QA", SNOR_ID(0x8c, 0x25, 0x34), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&f25dxqa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&esmt_otp_512b_16b),
	),

	SNOR_PART("F25L016A", SNOR_ID(0x8c, 0x20, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("F25L016A/F25L16PA", SNOR_ID(0x8c, 0x21, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_META | SNOR_F_NO_OP),
	),

	SNOR_PART("F25L016A(Bottom)", SNOR_ID(0x8c, 0x21, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE |
			     SNOR_F_AAI_WRITE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("F25L16PA", SNOR_ID(0x8c, 0x21, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ESMT_F_OTP_NO_EXSO),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&f25lxpa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&esmt_otp_512b),
	),

	SNOR_PART("F25L16QA", SNOR_ID(0x8c, 0x40, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ESMT_F_OTP_NO_EXSO),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&f25lxqa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&esmt_otp_512b),
	),

	SNOR_PART("F25L32PA", SNOR_ID(0x8c, 0x20, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_VOLATILE),
		  SNOR_VENDOR_FLAGS(ESMT_F_OTP_NO_EXSO),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_OTP_INFO(&esmt_otp_512b),
	),

	SNOR_PART("F25L32QA", SNOR_ID(0x8c, 0x41, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ESMT_F_OTP_NO_EXSO),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&f25lxqa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&esmt_otp_512b),
	),

	SNOR_PART("F25L64QA", SNOR_ID(0x8c, 0x41, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ESMT_F_OTP_NO_EXSO),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&f25lxqa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&esmt_otp_512b),
	),

	SNOR_PART("F25D64QA", SNOR_ID(0x8c, 0x25, 0x37), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_UNIQUE_ID | SNOR_F_GLOBAL_UNLOCK),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&f25dxqa_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&esmt_otp_512b_16b),
	),
};

static ufprog_status esmt_otp_read_no_exso(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len,
					   void *data)
{
	return scur_otp_read_cust(snor, snor->ext_param.otp->start_index + addr, len, data, true);
}

static ufprog_status esmt_otp_write_no_exso(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len,
					    const void *data)
{
	return scur_otp_write_cust(snor, snor->ext_param.otp->start_index + addr, len, data, true);
}

static ufprog_status esmt_otp_lock_no_exso(struct spi_nor *snor, uint32_t index)
{
	return scur_otp_lock_cust(snor, true);
}

static ufprog_status esmt_otp_locked_no_exso(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{
	ufprog_status ret;
	uint8_t es, id;

	STATUS_CHECK_RET(spi_nor_issue_single_opcode(snor, SNOR_CMD_ENSO));
	ret = spi_nor_read_reg(snor, SNOR_CMD_RES, &es);
	STATUS_CHECK_RET(spi_nor_write_disable(snor));

	if (ret)
		return ret;

	id = (snor->param.id.id[2] & 0xf) - 1;

	if (id == (0x30 | id))
		*retlocked = false;
	else if (id == (0x70 | id))
		*retlocked = true;
	else
		return UFP_FAIL;

	return UFP_OK;
}

static const struct spi_nor_flash_part_otp_ops esmt_otp_no_exso_ops = {
	.read = esmt_otp_read_no_exso,
	.write = esmt_otp_write_no_exso,
	.lock = esmt_otp_lock_no_exso,
	.locked = esmt_otp_locked_no_exso,
};

static ufprog_status esmt_part_fixup(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	spi_nor_blank_part_fill_default_opcodes(bp);
	uint32_t scur;

	if (bp->p.pp_io_caps & BIT_SPI_MEM_IO_1_4_4) {
		bp->pp_opcodes_3b[SPI_MEM_IO_1_4_4].opcode = SNOR_CMD_PAGE_PROG_QUAD_IO;
		bp->pp_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;
	}

	/* 6 dummy cycles will be used for QPI read */
	if (bp->p.read_io_caps & BIT_SPI_MEM_IO_4_4_4) {
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 6;
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;
	}

	if (bp->p.flags & SNOR_F_GLOBAL_UNLOCK) {
		STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &scur_acc, &scur));

		if (!(scur & SCUR_WPSEL))
			bp->p.flags &= ~SNOR_F_GLOBAL_UNLOCK;
	}

	return UFP_OK;
}

static ufprog_status esmt_otp_fixup(struct spi_nor *snor)
{
	if (snor->param.vendor_flags & ESMT_F_OTP_NO_EXSO)
		snor->ext_param.ops.otp = &esmt_otp_no_exso_ops;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup esmt_fixups = {
	.pre_param_setup = esmt_part_fixup,
	.pre_chip_setup = esmt_otp_fixup,
};

static ufprog_status esmt_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	if (retlen)
		*retlen = snor->ext_param.otp->start_index;

	if (!data)
		return UFP_OK;

	return scur_otp_read_cust(snor, 0, snor->ext_param.otp->start_index, data,
				  snor->param.vendor_flags & ESMT_F_OTP_NO_EXSO);
}

static const struct spi_nor_flash_part_ops esmt_ops = {
	.otp = &scur_otp_ops,

	.read_uid = esmt_read_uid,
};

const struct spi_nor_vendor vendor_esmt = {
	.mfr_id = SNOR_VENDOR_ESMT,
	.id = "esmt",
	.name = "ESMT",
	.parts = esmt_parts,
	.nparts = ARRAY_SIZE(esmt_parts),
	.vendor_flag_names = esmt_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(esmt_vendor_flag_info),
	.default_part_ops = &esmt_ops,
	.default_part_fixups = &esmt_fixups,
};
