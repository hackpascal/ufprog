// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * XMC SPI-NOR flash parts
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

#define XMC_UID_LEN				8

 /* XMC vendor flags */
#define XMC_F_HFM				BIT(0)
#define XMC_F_LC_SR3_BIT3_0_RST_0		BIT(1)
#define XMC_F_DC_SR3_BIT0			BIT(2)
#define XMC_F_DC_SR3_BIT1_0			BIT(3) /* 0x3 -> D8/Q10/133MHz */
#define XMC_F_DC_SR3_BIT1_0_RST_0		BIT(4) /* 0x0 -> D4/Q6/104MHz */
#define XMC_F_DC_SR3_BIT4_3			BIT(5) /* 0x3 -> D8/Q10/133MHz */

static const struct spi_nor_part_flag_enum_info xmc_vendor_flag_info[] = {
	{ 0, "high-freq-mode" },
	{ 1, "lc-sr3-bit0-3-reset-to-0" },
	{ 2, "dc-sr3-bit0" },
	{ 3, "dc-sr3-bit0-1" },
	{ 4, "dc-sr3-bit0-1-reset-to-0" },
	{ 3, "dc-sr3-bit3-4" },
};

static const struct spi_nor_otp_info xmc_otp_3 = {
	.start_index = 1,
	.count = 3,
	.size = 0x100,
};

static const struct spi_nor_reg_field_item xmc_no_srp1_sr2_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(1, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(3, 1, "LB1", "Security Register Lock Bit 1"),
	SNOR_REG_FIELD(4, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(5, 1, "LB3", "Security Register Lock Bit 3"),
	SNOR_REG_FIELD(6, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def xmc_no_srp1_sr2 = SNOR_REG_DEF("SR2", "Status Register 2", &cr_acc,
								   xmc_no_srp1_sr2_fields);

static const struct spi_nor_reg_field_item xmc_hfm_drv56_hrst_sr3_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(4, 1, "HFM", "High Frequency Mode Enable"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def xmc_hfm_drv56_hrst_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
									  xmc_hfm_drv56_hrst_sr3_fields);

static const struct snor_reg_info xmc_sr_cr_nosrp1_sr3_hfm_regs = SNOR_REG_INFO(&w25q_sr1, &xmc_no_srp1_sr2,
										&xmc_hfm_drv56_hrst_sr3);

static const struct spi_nor_reg_field_item xmc_4lc_hfm_drv56_hrst_sr3_fields[] = {
	SNOR_REG_FIELD(0, 0xf, "LC", "Latency Control"),
	SNOR_REG_FIELD_ENABLED_DISABLED(4, 1, "HFM", "High Frequency Mode Enable"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def xmc_4lc_hfm_drv56_hrst_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
									      xmc_4lc_hfm_drv56_hrst_sr3_fields);

static const struct snor_reg_info xmc_sr_cr_sr3_lc_hfm_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2,
									    &xmc_4lc_hfm_drv56_hrst_sr3);

static const struct spi_nor_reg_field_item xmc_drv56_hrst_sr3_fields[] = {
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def xmc_drv56_hrst_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
								      xmc_drv56_hrst_sr3_fields);

static const struct snor_reg_info xmc_sr_cr_sr3_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &xmc_drv56_hrst_sr3);

static const struct spi_nor_reg_field_item xmc_dc_hfm_drv56_hrst_sr3_fields[] = {
	SNOR_REG_FIELD(0, 1, "DC", "Dummy Configuration"),
	SNOR_REG_FIELD_ENABLED_DISABLED(4, 1, "HFM", "High Frequency Mode Enable"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def xmc_dc_hfm_drv56_hrst_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
									     xmc_dc_hfm_drv56_hrst_sr3_fields);

static const struct snor_reg_info xmc_sr_cr_sr3_dc_hfm_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2,
									    &xmc_dc_hfm_drv56_hrst_sr3);

static const struct spi_nor_reg_field_item xmc_2dc_drv56_hrst_sr3_fields[] = {
	SNOR_REG_FIELD(0, 3, "DC", "Dummy Configuration"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def xmc_2dc_drv56_hrst_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
									  xmc_2dc_drv56_hrst_sr3_fields);

static const struct snor_reg_info xmc_sr_cr_sr3_2dc_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &xmc_2dc_drv56_hrst_sr3);

static const struct spi_nor_reg_field_item xmc_adp_2dc_drv56_hrst_sr3_fields[] = {
	SNOR_REG_FIELD_FULL(1, 1, "ADP", "Power-up Address Mode", &w25q_sr3_adp_values),
	SNOR_REG_FIELD(3, 3, "DC", "Dummy Configuration"),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def xmc_adp_2dc_drv56_hrst_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc,
									      xmc_adp_2dc_drv56_hrst_sr3_fields);

static const struct snor_reg_info xmc_adp_sr_cr_sr3_2dc_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2,
									     &xmc_adp_2dc_drv56_hrst_sr3);

static ufprog_status xm25qh16_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == 6)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25QH16C");

		return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25QH16B");
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup xm25qh16_fixups = {
	.pre_param_setup = xm25qh16_fixup_model,
};

static ufprog_status xm25qu16_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == 6)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25QU16C");

		return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25QU16B");
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup xm25qu16_fixups = {
	.pre_param_setup = xm25qu16_fixup_model,
};

static ufprog_status xm25qx32_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	/* STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1)); */

	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == 6)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25QH32C");

		if (((uint8_t *)snor->sfdp.vendor)[3] == 0x27)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25QH32B");
		else if (((uint8_t *)snor->sfdp.vendor)[3] == 0x23)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25QE32C");
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup xm25qx32_fixups = {
	.pre_param_setup = xm25qx32_fixup_model,
};

static ufprog_status xm25xu32c_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (((uint8_t *)snor->sfdp.vendor)[1] == 0x19)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25QU32C");
		else if (((uint8_t *)snor->sfdp.data)[1] == 0x20)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25LU32C");
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup xm25xu32c_fixups = {
	.pre_param_setup = xm25xu32c_fixup_model,
};

static ufprog_status xm25qh64a_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QH64A"));

	vp->vendor_init = vp->vendor;
	vp->vendor = &vendor_xmc;

	bp->p.model = bp->model;
	snprintf(bp->model, sizeof(bp->model), "XM25QH64A");

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup xm25qh64a_fixups = {
	.pre_param_setup = xm25qh64a_fixup_model,
};

static ufprog_status xm25xu64c_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (((uint8_t *)snor->sfdp.vendor)[1] == 0x19)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25QU64C");
		else if (((uint8_t *)snor->sfdp.data)[1] == 0x20)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25LU64C");
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup xm25xu64c_fixups = {
	.pre_param_setup = xm25xu64c_fixup_model,
};

static ufprog_status xm25qh128a_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "EN25QH128A"));

	vp->vendor_init = vp->vendor;
	vp->vendor = &vendor_xmc;

	bp->p.model = bp->model;
	snprintf(bp->model, sizeof(bp->model), "XM25QH128A");

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup xm25qh128a_fixups = {
	.pre_param_setup = xm25qh128a_fixup_model,
};

static ufprog_status xm25xu128c_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (((uint8_t *)snor->sfdp.vendor)[1] == 0x19)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25QU128C");
		else if (((uint8_t *)snor->sfdp.data)[1] == 0x20)
			return spi_nor_reprobe_part(snor, vp, bp, NULL, "XM25LU128C");
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup xm25xu128c_fixups = {
	.pre_param_setup = xm25xu128c_fixup_model,
};

static ufprog_status xm25qh256b_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "IS25LP256D"));

	vp->vendor_init = vp->vendor;
	vp->vendor = &vendor_xmc;

	bp->p.model = bp->model;
	snprintf(bp->model, sizeof(bp->model), "XM25QH256B");

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup xm25qh256b_fixups = {
	.pre_param_setup = xm25qh256b_fixup_model,
};

static ufprog_status xm25qu256b_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					    struct spi_nor_flash_part_blank *bp)
{
	STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "IS25WP256D"));

	vp->vendor_init = vp->vendor;
	vp->vendor = &vendor_xmc;

	bp->p.model = bp->model;
	snprintf(bp->model, sizeof(bp->model), "XM25QU256B");

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup xm25qu256b_fixups = {
	.pre_param_setup = xm25qu256b_fixup_model,
};

static const struct spi_nor_flash_part xmc_parts[] = {
	SNOR_PART("XM25QH10B", SNOR_ID(0x20, 0x40, 0x11), SZ_128K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XMC_F_HFM),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xmc_sr_cr_nosrp1_sr3_hfm_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QH20B", SNOR_ID(0x20, 0x40, 0x12), SZ_256K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XMC_F_HFM),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xmc_sr_cr_nosrp1_sr3_hfm_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QH40B", SNOR_ID(0x20, 0x40, 0x13), SZ_512K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XMC_F_HFM),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xmc_sr_cr_nosrp1_sr3_hfm_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QU41B", SNOR_ID(0x20, 0x50, 0x13), SZ_512K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XMC_F_HFM),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xmc_sr_cr_nosrp1_sr3_hfm_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QH80B", SNOR_ID(0x20, 0x40, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XMC_F_HFM),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xmc_sr_cr_nosrp1_sr3_hfm_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QU80B", SNOR_ID(0x20, 0x50, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XMC_F_HFM),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xmc_sr_cr_nosrp1_sr3_hfm_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QH16*", SNOR_ID(0x20, 0x40, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XMC_F_LC_SR3_BIT3_0_RST_0),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(60),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
		  SNOR_FIXUPS(&xm25qh16_fixups),
	),

	SNOR_PART("XM25QH16B", SNOR_ID(0x20, 0x40, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XMC_F_HFM | XMC_F_LC_SR3_BIT3_0_RST_0),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xmc_sr_cr_sr3_lc_hfm_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QH16C", SNOR_ID(0x20, 0x40, 0x15), SZ_2M, /* SFDP 1.6 */
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&xmc_sr_cr_sr3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QU16*", SNOR_ID(0x20, 0x50, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(60),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
		  SNOR_FIXUPS(&xm25qu16_fixups),
	),

	SNOR_PART("XM25QU16B", SNOR_ID(0x20, 0x50, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XMC_F_HFM),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xmc_sr_cr_nosrp1_sr3_hfm_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QU16C", SNOR_ID(0x20, 0x50, 0x15), SZ_2M, /* SFDP 1.6 */
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&xmc_sr_cr_sr3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QW16C", SNOR_ID(0x20, 0x42, 0x15), SZ_2M, /* SFDP 1.6 */
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&xmc_sr_cr_sr3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25Q*32*", SNOR_ID(0x20, 0x40, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XMC_F_LC_SR3_BIT3_0_RST_0),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(60),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
		  SNOR_FIXUPS(&xm25qx32_fixups),
	),

	SNOR_PART("XM25QE32C", SNOR_ID(0x20, 0x40, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XMC_F_HFM | XMC_F_DC_SR3_BIT0),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xmc_sr_cr_sr3_dc_hfm_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QH32B", SNOR_ID(0x20, 0x40, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(XMC_F_HFM | XMC_F_LC_SR3_BIT3_0_RST_0),
		  SNOR_QE_SR2_BIT1, SNOR_QPI_QER_38H_FFH,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_4_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&xmc_sr_cr_sr3_lc_hfm_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QH32C", SNOR_ID(0x20, 0x40, 0x16), SZ_4M, /* SFDP 1.6 */
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&xmc_sr_cr_sr3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25*U32C", SNOR_ID(0x20, 0x50, 0x16), SZ_4M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_META),
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT1_0_RST_0),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&xmc_sr_cr_sr3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
		  SNOR_FIXUPS(&xm25xu32c_fixups),
	),

	SNOR_PART("XM25QU32C", SNOR_ID(0x20, 0x50, 0x16), SZ_4M, /* SFDP 1.6 */
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&xmc_sr_cr_sr3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25LU32C", SNOR_ID(0x20, 0x50, 0x16), SZ_4M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT1_0),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QW32C", SNOR_ID(0x20, 0x42, 0x16), SZ_4M, /* SFDP 1.6 */
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&xmc_sr_cr_sr3_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25Q*32*-QPI", SNOR_ID(0x20, 0x60, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_META),
		  SNOR_FIXUPS(&xm25qx32_fixups),
	),

	SNOR_PART("XM25QH64A", SNOR_ID(0x20, 0x70, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_META),
		  SNOR_FIXUPS(&xm25qh64a_fixups),
	),

	SNOR_PART("XM25QH64C", SNOR_ID(0x20, 0x40, 0x17), SZ_8M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT1_0),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25*U64C", SNOR_ID(0x20, 0x41, 0x17), SZ_8M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_META),
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT1_0),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&xmc_otp_3),
		  SNOR_FIXUPS(&xm25xu64c_fixups),
	),

	SNOR_PART("XM25QU64C", SNOR_ID(0x20, 0x41, 0x17), SZ_8M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT1_0),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25LU64C", SNOR_ID(0x20, 0x41, 0x17), SZ_8M, /* SFDP 1.6, DTR */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT1_0),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QW64C", SNOR_ID(0x20, 0x42, 0x17), SZ_8M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT1_0),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QH128A", SNOR_ID(0x20, 0x70, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_META),
		  SNOR_FIXUPS(&xm25qh128a_fixups),
	),

	SNOR_PART("XM25QH128C", SNOR_ID(0x20, 0x40, 0x18), SZ_16M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT1_0),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25*U128C", SNOR_ID(0x20, 0x41, 0x18), SZ_16M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_META),
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT1_0),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&xmc_otp_3),
		  SNOR_FIXUPS(&xm25xu128c_fixups),
	),

	SNOR_PART("XM25QU128C", SNOR_ID(0x20, 0x41, 0x18), SZ_16M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT1_0),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25LU128C", SNOR_ID(0x20, 0x41, 0x18), SZ_16M, /* SFDP 1.6, DTR */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT1_0),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QW128C", SNOR_ID(0x20, 0x42, 0x18), SZ_16M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT1_0),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QH256B", SNOR_ID(0x20, 0x60, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_META),
		  SNOR_FIXUPS(&xm25qh256b_fixups),
	),

	SNOR_PART("XM25QU256B", SNOR_ID(0x20, 0x70, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_META),
		  SNOR_FIXUPS(&xm25qu256b_fixups),
	),

	SNOR_PART("XM25QH256C", SNOR_ID(0x20, 0x40, 0x19), SZ_32M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT4_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_adp_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QU256C", SNOR_ID(0x20, 0x41, 0x19), SZ_32M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT4_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_adp_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QW256C", SNOR_ID(0x20, 0x42, 0x19), SZ_32M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT4_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_adp_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QH512C", SNOR_ID(0x20, 0x40, 0x20), SZ_64M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT4_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_adp_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),

	SNOR_PART("XM25QU512C", SNOR_ID(0x20, 0x41, 0x20), SZ_64M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(XMC_F_DC_SR3_BIT4_3),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&xmc_adp_sr_cr_sr3_2dc_regs),
		  SNOR_WP_RANGES(&wpr_4bp_tb_cmp),
		  SNOR_OTP_INFO(&xmc_otp_3),
	),
};


static ufprog_status xmc_part_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				    struct spi_nor_flash_part_blank *bp)
{
	spi_nor_blank_part_fill_default_opcodes(bp);

	/* 8 dummy cycles will be used for QPI read */
	if (bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].opcode) {
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = 8;
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;

		if (bp->p.size >= SZ_32M) {
			bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].ndummy = 8;
			bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].nmode = 0;
		}
	}

	if (bp->p.vendor_flags & (XMC_F_DC_SR3_BIT0 | XMC_F_DC_SR3_BIT1_0 | XMC_F_DC_SR3_BIT4_3)) {
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

	return UFP_OK;
};

static const struct spi_nor_flash_part_fixup xmc_fixups = {
	.pre_param_setup = xmc_part_fixup,
};

static ufprog_status xmc_chip_setup(struct spi_nor *snor)
{
	if (snor->param.vendor_flags & XMC_F_HFM)
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &sr3_acc, 0, BIT(4), false));

	if (snor->param.vendor_flags & XMC_F_LC_SR3_BIT3_0_RST_0)
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &sr3_acc, 0xf, 0, false));
	else if (snor->param.vendor_flags & XMC_F_DC_SR3_BIT0)
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &sr3_acc, 0, 1, false));
	else if (snor->param.vendor_flags & XMC_F_DC_SR3_BIT1_0)
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &sr3_acc, 0, 3, false));
	else if (snor->param.vendor_flags & XMC_F_DC_SR3_BIT1_0_RST_0)
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &sr3_acc, 0x3, 0, false));
	else if (snor->param.vendor_flags & XMC_F_DC_SR3_BIT4_3)
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &sr3_acc, BITS(4, 3), 3 << 3, false));

	return UFP_OK;
}

static ufprog_status xmc_setup_qpi(struct spi_nor *snor, bool enabled)
{
	if (enabled) {
		/* Set QPI read dummy cycles to 8 for maximum speed */
		return spi_nor_write_reg(snor, SNOR_CMD_SET_READ_PARAMETERS, QPI_READ_DUMMY_CLOCKS_8);
	}

	return UFP_OK;
}

static ufprog_status xmc_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_READ_UNIQUE_ID, 1),
		SPI_MEM_OP_NO_ADDR,
		SPI_MEM_OP_DUMMY(snor->state.a4b_mode ? 5 : 4, 1),
		SPI_MEM_OP_DATA_IN(XMC_UID_LEN, data, 1)
	);

	if (retlen)
		*retlen = XMC_UID_LEN;

	if (!data)
		return UFP_OK;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static const struct spi_nor_flash_part_ops xmc_default_part_ops = {
	.otp = &secr_otp_ops,

	.chip_setup = xmc_chip_setup,
	.setup_qpi = xmc_setup_qpi,
	.qpi_dis = spi_nor_disable_qpi_ffh,
	.read_uid = xmc_read_uid,
};

const struct spi_nor_vendor vendor_xmc = {
	.mfr_id = SNOR_VENDOR_XMC,
	.id = "xmc",
	.name = "XMC",
	.parts = xmc_parts,
	.nparts = ARRAY_SIZE(xmc_parts),
	.default_part_fixups = &xmc_fixups,
	.default_part_ops = &xmc_default_part_ops,
	.vendor_flag_names = xmc_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(xmc_vendor_flag_info),
};
