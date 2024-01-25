// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Atmel/Adesto/Dialog/Renesas SPI-NOR flash parts
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

#define ATMEL_ESN_UID_64B_LEN			64
#define ATMEL_FF_UID_LEN			128

/* Block-protection bits */
#define SR_BP3					BIT(5)
#define SR_BP4					BIT(6)

#define BP_1_0					(SR_BP1 | SR_BP0)
#define BP_43_10				(SR_BP4 | SR_BP3 | SR_BP1 | SR_BP0)
#define BP_4_0					(SR_BP4 | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0)

/* Atmel vendor flags */
#define ATMEL_F_SR_BIT5_EPE			BIT(0)
#define ATMEL_F_SR4_BIT4_EE_BIT5_PE		BIT(1)
#define ATMEL_F_OTP_ESN_128B			BIT(2)
#define ATMEL_F_OTP_SECR			BIT(3)
#define ATMEL_F_OTP_SECR_IDX_BIT8		BIT(4)
#define ATMEL_F_OTP_ESN_SCUR			BIT(5)
#define ATMEL_F_OTP_UID_FF			BIT(6)
#define ATMEL_F_UID_WINBOND_8B			BIT(7)
#define ATMEL_F_UID_WINBOND_16B			BIT(8)

static const struct spi_nor_part_flag_enum_info atmel_vendor_flag_info[] = {
	{ 0, "program-erase-fail-indicator-in-sr-bit5" },
	{ 1, "program-erase-fail-indicator-in-sr4-bit4-5" },
	{ 2, "128-bytes-for-otp-and-esn" },
	{ 3, "secr-style-otp" },
	{ 4, "secr-style-otp-index-bit-8" },
	{ 5, "scur-style-otp-and-esn" },
	{ 6, "at25ff-style-otp-and-otp" },
	{ 7, "winbond-style-uid-8-bytes" },
	{ 8, "winbond-style-uid-16-bytess" },
};

#define AT25FF_ANY_REG(_addr)											\
	{ .type = SNOR_REG_NORMAL, .num = 1,									\
	  .desc[0] = { .flags = SNOR_REGACC_F_SR, .addr = (_addr), .naddr = 1, .ndummy_read = 1, .ndata = 1,	\
		       .read_opcode = SNOR_CMD_READ_AR, .write_opcode = SNOR_CMD_WRITE_AR }			\
	}

static const struct spi_nor_reg_access at25ff_sr4_acc = AT25FF_ANY_REG(4);
static const struct spi_nor_reg_access at25ff_sr5_acc = AT25FF_ANY_REG(5);
static const struct spi_nor_reg_access at25xe_sr6_acc = AT25FF_ANY_REG(6);

static const struct spi_nor_reg_field_item at25f_1bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD_YES_NO(7, 1, "WPEN", "Write Protection Enabled"),
};

static const struct spi_nor_reg_def at25f_1bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, at25f_1bp_sr_fields);

static const struct snor_reg_info at25f_1bp_regs = SNOR_REG_INFO(&at25f_1bp_sr);

static const struct spi_nor_reg_field_item at25f_2bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD_YES_NO(7, 1, "WPEN", "Write Protection Enabled"),
};

static const struct spi_nor_reg_def at25f_2bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, at25f_2bp_sr_fields);

static const struct snor_reg_info at25f_2bp_regs = SNOR_REG_INFO(&at25f_2bp_sr);

static const struct spi_nor_reg_field_item at25f_3bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD_YES_NO(7, 1, "WPEN", "Write Protection Enabled"),
};

static const struct spi_nor_reg_def at25f_3bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, at25f_3bp_sr_fields);

static const struct snor_reg_info at25f_3bp_regs = SNOR_REG_INFO(&at25f_3bp_sr);

static const struct spi_nor_reg_field_item at25fs010_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(6, 1, "BP4", "Block Protect Bit 4"),
	SNOR_REG_FIELD_YES_NO(7, 1, "WPEN", "Write Protection Enabled"),
};

static const struct spi_nor_reg_def at25fs010_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, at25fs010_sr_fields);

static const struct snor_reg_info at25fs010_regs = SNOR_REG_INFO(&at25fs010_sr);

static const struct spi_nor_reg_field_item at25fs040_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(6, 1, "BP4", "Block Protect Bit 4"),
	SNOR_REG_FIELD_YES_NO(7, 1, "WPEN", "Write Protection Enabled"),
};

static const struct spi_nor_reg_def at25fs040_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, at25fs040_sr_fields);

static const struct snor_reg_info at25fs040_regs = SNOR_REG_INFO(&at25fs040_sr);

static const struct spi_nor_reg_field_item at_1bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD_YES_NO(7, 1, "BPL", "Block Protection Locked"),
};

static const struct spi_nor_reg_def at_1bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, at_1bp_sr_fields);

static const struct snor_reg_info at_1bp_regs = SNOR_REG_INFO(&at_1bp_sr);

static const struct spi_nor_reg_field_item at_qe_cr_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(7, 1, "QE", "Quad Enable"),
};

static const struct spi_nor_reg_def at_qe_cr = SNOR_REG_DEF("CR", "Configuration Register", &cr_3e3f_acc,
							    at_qe_cr_fields);

static const struct snor_reg_info at_qe_only_regs = SNOR_REG_INFO(&at_qe_cr);

static const struct spi_nor_reg_field_item at25eu_sr3_fields[] = {
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

static const struct spi_nor_reg_def at25eu_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc, at25eu_sr3_fields);

static const struct snor_reg_info at25eu_3_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &at25eu_sr3);

static const struct spi_nor_reg_field_item at25ff_sr4_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(3, 1, "XIP", "XiP Mode Select"),
	SNOR_REG_FIELD(7, 1, "PDM", "Power-Down Mode"),
};

static const struct spi_nor_reg_def at25ff_sr4 = SNOR_REG_DEF("SR4", "Status Register 4", &at25ff_sr4_acc,
							      at25ff_sr4_fields);

static const struct spi_nor_reg_field_values st25ff_sr5_dc_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "2 clocks"),
	VALUE_ITEM(1, "4 clocks"),
	VALUE_ITEM(2, "6 clocks"),
	VALUE_ITEM(3, "8 clocks"),
	VALUE_ITEM(4, "10 clocks"),
);

static const struct spi_nor_reg_field_item at25ff_sr5_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(0, 1, "DWA", "Dowble-word Aligned"),
	SNOR_REG_FIELD_FULL(4, 7, "DC", "Dummy Cycles", &st25ff_sr5_dc_values),
	SNOR_REG_FIELD(7, 1, "PDM", "Power-Down Mode"),
};

static const struct spi_nor_reg_def at25ff_sr5 = SNOR_REG_DEF("SR5", "Status Register 5", &at25ff_sr5_acc,
							      at25ff_sr5_fields);

static const struct snor_reg_info at25ff_5_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &w25q_sr3, &at25ff_sr4,
								&at25ff_sr5);

static const struct spi_nor_reg_field_item at25qf_sr3_fields[] = {
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
};

static const struct spi_nor_reg_def at25qf_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc, at25qf_sr3_fields);

static const struct snor_reg_info at25qf_3_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &at25qf_sr3);

static const struct spi_nor_reg_field_item at25ql321_sr1_fields[] = {
	SNOR_REG_FIELD_YES_NO(7, 1, "SRP0", "Status Register Protect 0"),
};

static const struct spi_nor_reg_def at25ql321_sr1 = SNOR_REG_DEF("SR1", "Status Register 1", &sr_acc,
								 at25ql321_sr1_fields);

static const struct spi_nor_reg_field_item at25ql321_sr2_fields[] = {
	SNOR_REG_FIELD_YES_NO(0, 1, "SRP1", "Status Register Protect 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(1, 1, "QE", "Quad Enable"),
};

static const struct spi_nor_reg_def at25ql321_sr2 = SNOR_REG_DEF("SR2", "Status Register 2", &cr_acc,
								 at25ql321_sr2_fields);

static const struct snor_reg_info at25ql321_regs = SNOR_REG_INFO(&at25ql321_sr1, &at25ql321_sr2);

static const struct spi_nor_reg_field_item at25ql_sr2_fields[] = {
	SNOR_REG_FIELD_YES_NO(0, 1, "SRP1", "Status Register Protect 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(1, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(6, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def at25ql_sr2 = SNOR_REG_DEF("SR2", "Status Register 2", &cr_acc, at25ql_sr2_fields);

static const struct snor_reg_info at25ql_regs = SNOR_REG_INFO(&w25q_sr1, &at25ql_sr2);

static const struct snor_reg_info at25sf_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2);

static const struct spi_nor_reg_field_item at25xe_sr5_fields[] = {
	SNOR_REG_FIELD_ENABLED_DISABLED(0, 1, "DWA", "Dowble-word Aligned"),
	SNOR_REG_FIELD_FULL(4, 7, "DC", "Dummy Cycles", &st25ff_sr5_dc_values),
};

static const struct spi_nor_reg_def at25xe_sr5 = SNOR_REG_DEF("SR5", "Status Register 5", &at25ff_sr5_acc,
							      at25xe_sr5_fields);

static const struct spi_nor_reg_field_values at25xe_sr6_lbd_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "100us"),
	VALUE_ITEM(1, "1ms"),
);

static const struct spi_nor_reg_field_values at25xe_sr6_lbld_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "10uA"),
	VALUE_ITEM(1, "100uA"),
	VALUE_ITEM(2, "1mA"),
	VALUE_ITEM(3, "10mA"),
);

static const struct spi_nor_reg_field_values at25xe_sr6_lbvl_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "1.8V"),
	VALUE_ITEM(1, "2.0V"),
	VALUE_ITEM(2, "2.2V"),
	VALUE_ITEM(3, "2.4V"),
	VALUE_ITEM(3, "2.6V"),
	VALUE_ITEM(3, "2.8V"),
	VALUE_ITEM(3, "3.0V"),
	VALUE_ITEM(3, "3.2V"),
);

static const struct spi_nor_reg_field_item at25xe_sr6_fields[] = {
	SNOR_REG_FIELD_FULL(0, 1, "LBD", "Load Battery Delay", &at25xe_sr6_lbd_values),
	SNOR_REG_FIELD_FULL(1, 3, "LBLD", "Low Battery Load", &at25xe_sr6_lbld_values),
	SNOR_REG_FIELD_FULL(3, 7, "LBVL", "Low Battery Volatage Level", &at25xe_sr6_lbvl_values),
};

static const struct spi_nor_reg_def at25xe_sr6 = SNOR_REG_DEF("SR6", "Status Register 6", &at25xe_sr6_acc,
							      at25xe_sr6_fields);

static const struct snor_reg_info at25xe_regs = SNOR_REG_INFO(&w25q_sr1, &w25q_sr2, &w25q_sr3, &at25ff_sr4,
							      &at25xe_sr5, &at25xe_sr6);

static const struct spi_nor_reg_field_item at26f_sr_fields[] = {
	SNOR_REG_FIELD_YES_NO(7, 1, "SPRL", "Sector Protection Registers Lockes"),
};

static const struct spi_nor_reg_def at26f_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, at26f_sr_fields);

static const struct snor_reg_info at26f_regs = SNOR_REG_INFO(&at26f_sr);

static const struct spi_nor_otp_info at_otp_esn_64b = {
	.start_index = 0,
	.count = 1,
	.size = 64,
};

static const struct spi_nor_otp_info at_otp_3x256b = {
	.start_index = 1,
	.count = 3,
	.size = 0x100,
};

static const struct spi_nor_otp_info at_otp_3x512b = {
	.start_index = 1,
	.count = 3,
	.size = 0x200,
};

static const struct spi_nor_otp_info at_otp_4x128b = {
	.start_index = 0,
	.count = 4,
	.size = 0x80,
};

static const struct spi_nor_otp_info at_otp_512b = {
	.start_index = 0x10,
	.count = 1,
	.size = 0x1f0,
};

static const struct spi_nor_wp_info at25f512_wpr = SNOR_WP_BP(&sr_acc, BP_1_0,
	SNOR_WP_NONE( 0              ),	/* None */
	SNOR_WP_NONE(          SR_BP0),	/* None */
	SNOR_WP_NONE( SR_BP1         ),	/* None */
	SNOR_WP_ALL(  SR_BP1 | SR_BP0),	/* All */
);

static const struct spi_nor_wp_info at25fs010_wpr = SNOR_WP_BP(&sr_acc, BP_43_10,
	SNOR_WP_NONE( 0                                   ),	/* None */
	SNOR_WP_ALL(                    SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(           SR_BP3 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_BP4 |          SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_BP4 | SR_BP3 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_RP_UP(         SR_BP3                  , 5),	/* Upper 1/32 */
	SNOR_WP_RP_UP(SR_BP4                           , 4),	/* Upper 1/16 */
	SNOR_WP_RP_UP(SR_BP4 | SR_BP3                  , 3),	/* Upper 1/8 */

	SNOR_WP_RP_UP(                           SR_BP0, 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(         SR_BP3 |          SR_BP0, 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(SR_BP4 |                   SR_BP0, 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(SR_BP4 | SR_BP3 |          SR_BP0, 2),	/* Upper 1/4 */

	SNOR_WP_RP_UP(                  SR_BP1         , 1),	/* Upper 1/2 */
	SNOR_WP_RP_UP(         SR_BP3 | SR_BP1         , 1),	/* Upper 1/2 */
	SNOR_WP_RP_UP(SR_BP4 |          SR_BP1         , 1),	/* Upper 1/2 */
	SNOR_WP_RP_UP(SR_BP4 | SR_BP3 | SR_BP1         , 1),	/* Upper 1/2 */
);

static const struct spi_nor_wp_info at25fs040_wpr = SNOR_WP_BP(&sr_acc, BP_4_0,
	SNOR_WP_NONE( 0                                            ),	/* None */
	SNOR_WP_ALL(  SR_BP4 | SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_ALL(                    SR_BP2                     ),	/* All */
	SNOR_WP_ALL(           SR_BP3 | SR_BP2                     ),	/* All */
	SNOR_WP_ALL(  SR_BP4 |          SR_BP2                     ),	/* All */
	SNOR_WP_ALL(  SR_BP4 | SR_BP3 | SR_BP2                     ),	/* All */

	SNOR_WP_ALL(                    SR_BP2 |          SR_BP0   ),	/* All */
	SNOR_WP_ALL(           SR_BP3 | SR_BP2 |          SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_BP4 |          SR_BP2 |          SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_BP4 | SR_BP3 | SR_BP2 |          SR_BP0   ),	/* All */

	SNOR_WP_ALL(                    SR_BP2 | SR_BP1            ),	/* All */
	SNOR_WP_ALL(           SR_BP3 | SR_BP2 | SR_BP1            ),	/* All */
	SNOR_WP_ALL(  SR_BP4 |          SR_BP2 | SR_BP1            ),	/* All */
	SNOR_WP_ALL(  SR_BP4 | SR_BP3 | SR_BP2 | SR_BP1            ),	/* All */

	SNOR_WP_ALL(                    SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(           SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */
	SNOR_WP_ALL(  SR_BP4 |          SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_RP_UP(         SR_BP3                           , 6),	/* Upper 1/64 */
	SNOR_WP_RP_UP(SR_BP4                                    , 5),	/* Upper 1/32 */
	SNOR_WP_RP_UP(SR_BP4 | SR_BP3                           , 4),	/* Upper 1/16 */

	SNOR_WP_RP_UP(                                    SR_BP0, 3),	/* Upper 1/8 */
	SNOR_WP_RP_UP(         SR_BP3 |                   SR_BP0, 3),	/* Upper 1/8 */
	SNOR_WP_RP_UP(SR_BP4 |                            SR_BP0, 3),	/* Upper 1/8 */
	SNOR_WP_RP_UP(SR_BP4 | SR_BP3 |                   SR_BP0, 3),	/* Upper 1/8 */

	SNOR_WP_RP_UP(                           SR_BP1         , 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(         SR_BP3 |          SR_BP1         , 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(SR_BP4 |                   SR_BP1         , 2),	/* Upper 1/4 */
	SNOR_WP_RP_UP(SR_BP4 | SR_BP3 |          SR_BP1         , 2),	/* Upper 1/4 */

	SNOR_WP_RP_UP(                           SR_BP1 | SR_BP0, 1),	/* Upper 1/2 */
	SNOR_WP_RP_UP(         SR_BP3 |          SR_BP1 | SR_BP0, 1),	/* Upper 1/2 */
	SNOR_WP_RP_UP(SR_BP4 |                   SR_BP1 | SR_BP0, 1),	/* Upper 1/2 */
	SNOR_WP_RP_UP(SR_BP4 | SR_BP3 |          SR_BP1 | SR_BP0, 1),	/* Upper 1/2 */
);

/* AT25QL321 */
static const SNOR_DC_CONFIG(at25ql321_dc_qpi_cfgs, SNOR_DC_IDX_VALUE(2, 6, 104), SNOR_DC_TUPLE(0, 1, 4, 0, 80));

static const SNOR_DC_TABLE(at25ql321_dc_table, 3, SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, at25ql321_dc_qpi_cfgs));

/* AT25QL641 */
static const SNOR_DC_CONFIG(at25ql641_dc_qpi_cfgs, SNOR_DC_IDX_VALUE(3, 8, 133), SNOR_DC_IDX_VALUE(2, 6, 104),
			    SNOR_DC_TUPLE(0, 1, 4, 0, 80));

static const SNOR_DC_TABLE(at25ql641_dc_table, 3, SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, at25ql641_dc_qpi_cfgs));

/* AT25FF041A */
static const SNOR_DC_CONFIG(at25ff041a_dc_144_cfgs, SNOR_DC_IDX_VALUE(4, 10, 108), SNOR_DC_IDX_VALUE(3, 8, 85),
			    SNOR_DC_IDX_VALUE(2, 6, 60), SNOR_DC_IDX_VALUE(1, 4, 45), SNOR_DC_IDX_VALUE(0, 2, 25));

static const SNOR_DC_TABLE(at25ff041a_dc_table, 7, SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, at25ff041a_dc_144_cfgs));

/* AT25FF161A */
static const SNOR_DC_CONFIG(at25ff161a_dc_144_cfgs, SNOR_DC_IDX_VALUE(4, 10, 90), SNOR_DC_IDX_VALUE(3, 8, 70),
			    SNOR_DC_IDX_VALUE(2, 6, 50), SNOR_DC_IDX_VALUE(1, 4, 40), SNOR_DC_IDX_VALUE(0, 2, 20));

static const SNOR_DC_TABLE(at25ff161a_dc_table, 7, SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, at25ff161a_dc_144_cfgs));

/* AT25FF321A */
static const SNOR_DC_CONFIG(at25ff321a_dc_144_cfgs, SNOR_DC_IDX_VALUE(4, 10, 100), SNOR_DC_IDX_VALUE(3, 8, 90),
			    SNOR_DC_IDX_VALUE(2, 6, 75), SNOR_DC_IDX_VALUE(1, 4, 50), SNOR_DC_IDX_VALUE(0, 2, 30));

static const SNOR_DC_TABLE(at25ff321a_dc_table, 7, SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, at25ff321a_dc_144_cfgs));

static const SNOR_DC_CHIP_SETUP_ACC(atmel_dc_acc_sr5_dc6_4, &at25ff_sr5_acc, 7, 4);

static ufprog_status at25sf041_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "AT25SF041");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "AT25SF041B");
}

static const struct spi_nor_flash_part_fixup at25sf041_fixups = {
	.pre_param_setup = at25sf041_fixup_model,
};

static ufprog_status at25sf081_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "AT25SF081");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "AT25SF081B");
}

static const struct spi_nor_flash_part_fixup at25sf081_fixups = {
	.pre_param_setup = at25sf081_fixup_model,
};

static ufprog_status at25sf161_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "AT25SF161");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "AT25SF161B");
}

static const struct spi_nor_flash_part_fixup at25sf161_fixups = {
	.pre_param_setup = at25sf161_fixup_model,
};

static ufprog_status at25sf321_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (!snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "AT25SF321");

	return spi_nor_reprobe_part(snor, vp, bp, NULL, "AT25SF321B");
}

static const struct spi_nor_flash_part_fixup at25sf321_fixups = {
	.pre_param_setup = at25sf321_fixup_model,
};

static ufprog_status at25eu_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	/* 1-1-4/1-4-4 read test failed using FT4222H with very low speed. One clock seems missing. */
	bp->p.read_io_caps &= ~BIT_SPI_MEM_IO_X4;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup at25eu_fixups = {
	.pre_param_setup = at25eu_fixup_model,
};

static const struct spi_nor_io_opcode at25f_read_opcodes[__SPI_MEM_IO_MAX] = {
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_1, SNOR_CMD_READ, 0, 0),
};

static const struct spi_nor_erase_info at25f_erase_opcodes_64k = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_64K, SNOR_CMD_SECTOR_ERASE_32K)
);

static const struct spi_nor_io_opcode at26df041_pp_opcodes[__SPI_MEM_IO_MAX] = {
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_1, SNOR_CMD_AT26DF_PAGE_PROG, 0, 0),
};

static DEFINE_SNOR_ALIAS(at25df256_alias, SNOR_ALIAS_MODEL("AT25DN256"));
static DEFINE_SNOR_ALIAS(at25df512c_alias, SNOR_ALIAS_MODEL("AT25DN512C"), SNOR_ALIAS_MODEL("AT25XE512C"));
static DEFINE_SNOR_ALIAS(at25df011_alias, SNOR_ALIAS_MODEL("AT25DN011"), SNOR_ALIAS_MODEL("AT25XE011"));
static DEFINE_SNOR_ALIAS(at25df021a_alias, SNOR_ALIAS_MODEL("AT25XE021A"), SNOR_ALIAS_MODEL("AT25XV021A"));
static DEFINE_SNOR_ALIAS(at25df041b_alias, SNOR_ALIAS_MODEL("AT25XE041B"), SNOR_ALIAS_MODEL("AT25XV041B"));
static DEFINE_SNOR_ALIAS(at25qf128a_alias, SNOR_ALIAS_MODEL("AT25SF128A"));
static DEFINE_SNOR_ALIAS(at25df321_alias, SNOR_ALIAS_MODEL("AT26DF321"));
static DEFINE_SNOR_ALIAS(at25ql321_alias, SNOR_ALIAS_MODEL("AT25SL321"));
static DEFINE_SNOR_ALIAS(at25ql641_alias, SNOR_ALIAS_MODEL("AT25SL641"));
static DEFINE_SNOR_ALIAS(at25ql128a_alias, SNOR_ALIAS_MODEL("AT25SL128A"));

static const struct spi_nor_flash_part atmel_parts[] = {
	SNOR_PART("AT25DF256", SNOR_ID(0x1f, 0x40, 0x00, 0x00), SZ_32K,
		  SNOR_ALIAS(&at25df256_alias), /* AT25DN256 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&at_1bp_regs),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
		  SNOR_WP_RANGES(&wpr_1bp),
	),

	SNOR_PART("AT25F512", SNOR_ID_NONE, SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_READ_OPCODES(at25f_read_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(20),
		  SNOR_REGS(&at25f_2bp_regs),
		  SNOR_WP_RANGES(&at25f512_wpr),
	),

	SNOR_PART("AT25F512A", SNOR_ID_NONE, SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_READ_OPCODES(at25f_read_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&at25f_1bp_regs),
		  SNOR_WP_RANGES(&wpr_1bp),
	),

	SNOR_PART("AT25F512B", SNOR_ID(0x1f, 0x65, 0x00, 0x00), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&at_1bp_regs),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
		  SNOR_WP_RANGES(&wpr_1bp),
	),

	SNOR_PART("AT25DF512C", SNOR_ID(0x1f, 0x65, 0x01, 0x00), SZ_64K,
		  SNOR_ALIAS(&at25df512c_alias), /* AT25DN512C, AT25XE512C */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&at_1bp_regs),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
		  SNOR_WP_RANGES(&wpr_1bp),
	),

	SNOR_PART("AT25F1024", SNOR_ID_NONE, SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_READ_OPCODES(at25f_read_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(20),
		  SNOR_REGS(&at25f_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up_ratio),
	),

	SNOR_PART("AT25FS010", SNOR_ID(0x1f, 0x66, 0x01), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&at25fs010_regs),
		  SNOR_WP_RANGES(&at25fs010_wpr),
	),

	SNOR_PART("AT25DF011", SNOR_ID(0x1f, 0x42, 0x00, 0x00), SZ_128K,
		  SNOR_ALIAS(&at25df011_alias), /* AT25DN011, AT25XE011 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&at_1bp_regs),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
		  SNOR_WP_RANGES(&wpr_1bp),
	),

	SNOR_PART("AT25EU0011A", SNOR_ID(0x1f, 0x10, 0x01), SZ_128K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_PP_DUAL_INPUT),
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR | ATMEL_F_UID_WINBOND_16B),
		  SNOR_SPI_MAX_SPEED_MHZ(85), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&at25eu_3_regs),
		  SNOR_OTP_INFO(&at_otp_3x512b),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&at25eu_fixups),
	),

	SNOR_PART("AT25F2048", SNOR_ID_NONE, SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_READ_OPCODES(at25f_read_opcodes),
		  SNOR_ERASE_INFO(&at25f_erase_opcodes_64k),
		  SNOR_SPI_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&at25f_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up_ratio),
	),

	SNOR_PART("AT25DF021", SNOR_ID(0x1f, 0x43, 0x00, 0x00), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
	),

	SNOR_PART("AT25DF021A", SNOR_ID(0x1f, 0x43, 0x01, 0x00), SZ_256K,
		  SNOR_ALIAS(&at25df021a_alias), /* AT25XE021A, AT25XV021A */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(70), SNOR_DUAL_MAX_SPEED_MHZ(40),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
	),

	SNOR_PART("AT25EU0021A", SNOR_ID(0x1f, 0x11, 0x01), SZ_256K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_PP_DUAL_INPUT),
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR | ATMEL_F_UID_WINBOND_16B),
		  SNOR_SPI_MAX_SPEED_MHZ(85), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&at25eu_3_regs),
		  SNOR_OTP_INFO(&at_otp_3x512b),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&at25eu_fixups),
	),

	SNOR_PART("AT25F4096", SNOR_ID_NONE, SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_READ_OPCODES(at25f_read_opcodes),
		  SNOR_ERASE_INFO(&at25f_erase_opcodes_64k),
		  SNOR_SPI_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&at25f_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up_ratio),
	),

	SNOR_PART("AT25FS040", SNOR_ID(0x1f, 0x66, 0x04), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&at25fs040_regs),
		  SNOR_WP_RANGES(&at25fs040_wpr),
	),

	SNOR_PART("AT26DF041", SNOR_ID(0x1f, 0x44, 0x00, 0x00), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_NO_WREN),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_OPCODES(at26df041_pp_opcodes),
		  SNOR_SPI_MAX_SPEED_MHZ(25),
	),

	SNOR_PART("AT25DF041A", SNOR_ID(0x1f, 0x44, 0x01, 0x00), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
	),

	SNOR_PART("AT25DF041B", SNOR_ID(0x1f, 0x44, 0x02, 0x00), SZ_512K,
		  SNOR_ALIAS(&at25df041b_alias), /* AT25XE041B, AT25XV041B */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(85), SNOR_DUAL_MAX_SPEED_MHZ(40),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
	),

	SNOR_PART("AT25FF041A", SNOR_ID(0x1f, 0x44, 0x08, 0x01), SZ_512K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK | SNOR_F_PP_DUAL_INPUT),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR4_BIT4_EE_BIT5_PE | ATMEL_F_OTP_UID_FF),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&at25ff_5_regs),
		  SNOR_OTP_INFO(&at_otp_4x128b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp, &srcr_comb_acc),
		  SNOR_DC_INFO(&at25ff041a_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&atmel_dc_acc_sr5_dc6_4),
	),

	SNOR_PART("AT25XE041D", SNOR_ID(0x1f, 0x44, 0x0c, 0x01), SZ_512K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK | SNOR_F_PP_DUAL_INPUT),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR4_BIT4_EE_BIT5_PE | ATMEL_F_OTP_UID_FF),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&at25xe_regs),
		  SNOR_OTP_INFO(&at_otp_4x128b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp, &srcr_comb_acc),
		  SNOR_DC_INFO(&at25ff041a_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&atmel_dc_acc_sr5_dc6_4),
	),

	SNOR_PART("AT25EU0041A", SNOR_ID(0x1f, 0x14, 0x01), SZ_512K, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_PP_DUAL_INPUT),
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR | ATMEL_F_UID_WINBOND_16B),
		  SNOR_SPI_MAX_SPEED_MHZ(85), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&at25eu_3_regs),
		  SNOR_OTP_INFO(&at_otp_3x512b),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&at25eu_fixups),
	),

	SNOR_PART("AT25SF041 (Meta)", SNOR_ID(0x1f, 0x84, 0x01), SZ_512K,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&at25sf_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&at25sf041_fixups),
	),

	SNOR_PART("AT25SF041", SNOR_ID(0x1f, 0x84, 0x01), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR_IDX_BIT8),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&at25sf_regs),
		  SNOR_OTP_INFO(&at_otp_3x256b),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("AT25SF041B", SNOR_ID(0x1f, 0x84, 0x01), SZ_512K, /* SFDP 1.8 (BFPT 1.7) */
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR | ATMEL_F_UID_WINBOND_8B),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&at25sf_regs),
		  SNOR_OTP_INFO(&at_otp_3x256b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp, &srcr_comb_acc),
	),

	SNOR_PART("AT26F004", SNOR_ID(0x1f, 0x04, 0x00, 0x00), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&at26f_regs),
	),

	SNOR_PART("AT26DF081A", SNOR_ID(0x1f, 0x45, 0x01, 0x00), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(70),
	),

	SNOR_PART("AT25DF081A", SNOR_ID(0x1f, 0x45, 0x01, 0x01, 0x00), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
	),

	SNOR_PART("AT25DL081", SNOR_ID(0x1f, 0x45, 0x02, 0x01, 0x00), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
	),

	SNOR_PART("AT25FF081A", SNOR_ID(0x1f, 0x45, 0x08, 0x01), SZ_1M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK | SNOR_F_PP_DUAL_INPUT),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR4_BIT4_EE_BIT5_PE | ATMEL_F_OTP_UID_FF),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&at25ff_5_regs),
		  SNOR_OTP_INFO(&at_otp_4x128b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp, &srcr_comb_acc),
		  SNOR_DC_INFO(&at25ff041a_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&atmel_dc_acc_sr5_dc6_4),
	),

	SNOR_PART("AT25XE081D", SNOR_ID(0x1f, 0x45, 0x0c, 0x01), SZ_1M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK | SNOR_F_PP_DUAL_INPUT),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR4_BIT4_EE_BIT5_PE | ATMEL_F_OTP_UID_FF),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&at25xe_regs),
		  SNOR_OTP_INFO(&at_otp_4x128b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp, &srcr_comb_acc),
		  SNOR_DC_INFO(&at25ff041a_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&atmel_dc_acc_sr5_dc6_4),
	),

	SNOR_PART("AT25SF081 (Meta)", SNOR_ID(0x1f, 0x85, 0x01), SZ_1M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&at25sf_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&at25sf081_fixups),
	),

	SNOR_PART("AT25SF081", SNOR_ID(0x1f, 0x85, 0x01), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR_IDX_BIT8),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&at25sf_regs),
		  SNOR_OTP_INFO(&at_otp_3x256b),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("AT25SF081B", SNOR_ID(0x1f, 0x85, 0x01), SZ_1M, /* SFDP 1.8 (BFPT 1.7) */
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR | ATMEL_F_UID_WINBOND_8B),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&at25sf_regs),
		  SNOR_OTP_INFO(&at_otp_3x256b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp, &srcr_comb_acc),
	),

	SNOR_PART("AT26DF161A", SNOR_ID(0x1f, 0x46, 0x01, 0x00), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(70),
	),

	SNOR_PART("AT25DF161", SNOR_ID(0x1f, 0x46, 0x02, 0x00), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
	),

	SNOR_PART("AT25DL161", SNOR_ID(0x1f, 0x46, 0x03, 0x01, 0x00), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(85), SNOR_DUAL_MAX_SPEED_MHZ(66),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
	),

	SNOR_PART("AT25FF161A", SNOR_ID(0x1f, 0x46, 0x08, 0x01), SZ_2M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK | SNOR_F_PP_DUAL_INPUT),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR4_BIT4_EE_BIT5_PE | ATMEL_F_OTP_UID_FF),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&at25ff_5_regs),
		  SNOR_OTP_INFO(&at_otp_4x128b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp, &srcr_comb_acc),
		  SNOR_DC_INFO(&at25ff161a_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&atmel_dc_acc_sr5_dc6_4),
	),

	SNOR_PART("AT25XE161D", SNOR_ID(0x1f, 0x46, 0x0c, 0x01), SZ_2M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK | SNOR_F_PP_DUAL_INPUT),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR4_BIT4_EE_BIT5_PE | ATMEL_F_OTP_UID_FF),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&at25xe_regs),
		  SNOR_OTP_INFO(&at_otp_4x128b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp, &srcr_comb_acc),
		  SNOR_DC_INFO(&at25ff161a_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&atmel_dc_acc_sr5_dc6_4),
	),

	SNOR_PART("AT25DQ161", SNOR_ID(0x1f, 0x86, 0x00, 0x01, 0x00), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_QE_SR2_BIT7,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&at_qe_only_regs),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
	),

	SNOR_PART("AT25SF161 (Meta)", SNOR_ID(0x1f, 0x86, 0x01), SZ_2M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&at25sf_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&at25sf161_fixups),
	),

	SNOR_PART("AT25SF161", SNOR_ID(0x1f, 0x86, 0x01), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR_IDX_BIT8),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&at25sf_regs),
		  SNOR_OTP_INFO(&at_otp_3x256b),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("AT25SF161B", SNOR_ID(0x1f, 0x86, 0x01), SZ_2M, /* SFDP 1.8 (BFPT 1.7) */
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR | ATMEL_F_UID_WINBOND_8B),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&at25qf_3_regs),
		  SNOR_OTP_INFO(&at_otp_3x256b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp, &srcr_comb_acc),
	),

	SNOR_PART("AT25DF321", SNOR_ID(0x1f, 0x47, 0x00, 0x00), SZ_4M,
		  SNOR_ALIAS(&at25df321_alias), /* AT26DF321 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(66),
	),

	SNOR_PART("AT25DF321A", SNOR_ID(0x1f, 0x47, 0x01, 0x00), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
	),

	SNOR_PART("AT25FF321A", SNOR_ID(0x1f, 0x47, 0x08, 0x01), SZ_4M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK | SNOR_F_PP_DUAL_INPUT),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR4_BIT4_EE_BIT5_PE | ATMEL_F_OTP_UID_FF),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&at25ff_5_regs),
		  SNOR_OTP_INFO(&at_otp_4x128b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp, &srcr_comb_acc),
		  SNOR_DC_INFO(&at25ff321a_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&atmel_dc_acc_sr5_dc6_4),
	),

	SNOR_PART("AT25XE321D", SNOR_ID(0x1f, 0x47, 0x0c, 0x01), SZ_4M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_GLOBAL_UNLOCK | SNOR_F_PP_DUAL_INPUT),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR4_BIT4_EE_BIT5_PE | ATMEL_F_OTP_UID_FF),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&at25xe_regs),
		  SNOR_OTP_INFO(&at_otp_4x128b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp, &srcr_comb_acc),
		  SNOR_DC_INFO(&at25ff321a_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&atmel_dc_acc_sr5_dc6_4),
	),

	SNOR_PART("AT25QL321", SNOR_ID(0x1f, 0x42, 0x16), SZ_4M, /* SFDP 1.6 */
		  SNOR_ALIAS(&at25ql321_alias), /* AT25SL321 */
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_ESN_SCUR),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&at25ql321_regs),
		  SNOR_OTP_INFO(&at_otp_512b),
		  SNOR_DC_INFO(&at25ql321_dc_table),
		  SNOR_DC_QPI_SET_READING_PARAM_DFL(),
	),

	SNOR_PART("AT25DQ321", SNOR_ID(0x1f, 0x87, 0x00, 0x01, 0x00), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_QE_SR2_BIT7,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(85), SNOR_QUAD_MAX_SPEED_MHZ(66),
		  SNOR_REGS(&at_qe_only_regs),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
	),

	SNOR_PART("AT25SF321 (Meta)", SNOR_ID(0x1f, 0x87, 0x01), SZ_4M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&at25sf_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
		  SNOR_FIXUPS(&at25sf321_fixups),
	),

	SNOR_PART("AT25SF321", SNOR_ID(0x1f, 0x87, 0x01), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR_IDX_BIT8),
		  SNOR_QE_SR2_BIT1_WR_SR1,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&at25sf_regs),
		  SNOR_OTP_INFO(&at_otp_3x256b),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp),
	),

	SNOR_PART("AT25SF321B", SNOR_ID(0x1f, 0x87, 0x01), SZ_4M, /* SFDP 1.8 (BFPT 1.7) */
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR | ATMEL_F_UID_WINBOND_8B),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&at25qf_3_regs),
		  SNOR_OTP_INFO(&at_otp_3x256b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp, &srcr_comb_acc),
	),

	SNOR_PART("AT25DF641", SNOR_ID(0x1f, 0x48, 0x00, 0x00), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(75), SNOR_DUAL_MAX_SPEED_MHZ(55),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
	),

	SNOR_PART("AT25DF641A", SNOR_ID(0x1f, 0x48, 0x00, 0x01, 0x00), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(ATMEL_F_SR_BIT5_EPE | ATMEL_F_OTP_ESN_128B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_SPI_MAX_SPEED_MHZ(85), SNOR_DUAL_MAX_SPEED_MHZ(65),
		  SNOR_OTP_INFO(&at_otp_esn_64b),
	),

	SNOR_PART("AT25QF641B", SNOR_ID(0x1f, 0x88, 0x01), SZ_8M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR | ATMEL_F_UID_WINBOND_8B),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&at25qf_3_regs),
		  SNOR_OTP_INFO(&at_otp_3x256b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp_ratio, &srcr_comb_acc),
	),

	SNOR_PART("AT25QL641", SNOR_ID(0x1f, 0x43, 0x17), SZ_8M, /* SFDP 1.6 */
		  SNOR_ALIAS(&at25ql641_alias), /* AT25SL641 */
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_ESN_SCUR),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&at25ql_regs),
		  SNOR_OTP_INFO(&at_otp_512b),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_DC_INFO(&at25ql641_dc_table),
		  SNOR_DC_QPI_SET_READING_PARAM_DFL(),
	),

	SNOR_PART("AT25SF641", SNOR_ID(0x1f, 0x32, 0x17), SZ_8M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_ESN_SCUR),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&at25ql_regs),
		  SNOR_OTP_INFO(&at_otp_512b),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_DC_INFO(&at25ql321_dc_table),
		  SNOR_DC_QPI_SET_READING_PARAM_DFL(),
	),

	SNOR_PART("AT25SF641B", SNOR_ID(0x1f, 0x88, 0x01), SZ_8M, /* SFDP 1.8 (BFPT 1.7) */
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR | ATMEL_F_UID_WINBOND_8B),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&at25qf_3_regs),
		  SNOR_OTP_INFO(&at_otp_3x256b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp_ratio, &srcr_comb_acc),
	),

	SNOR_PART("AT25QF128A", SNOR_ID(0x1f, 0x89, 0x01), SZ_16M, /* SFDP 1.? */
		  SNOR_ALIAS(&at25qf128a_alias), /* AT25SF128A */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H),
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_SECR | ATMEL_F_UID_WINBOND_8B),
		  SNOR_QE_SR2_BIT1,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&at25qf_3_regs),
		  SNOR_OTP_INFO(&at_otp_3x256b),
		  SNOR_WP_RANGES_ACC(&wpr_3bp_tb_sec_cmp_ratio, &srcr_comb_acc),
	),

	SNOR_PART("AT25QL128A", SNOR_ID(0x1f, 0x42, 0x18), SZ_16M, /* SFDP 1.6 */
		  SNOR_ALIAS(&at25ql128a_alias), /* AT25SL128A */
		  SNOR_VENDOR_FLAGS(ATMEL_F_OTP_ESN_SCUR),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&at25ql_regs),
		  SNOR_OTP_INFO(&at_otp_512b),
		  SNOR_WP_RANGES(&wpr_3bp_tb_sec_cmp_ratio),
		  SNOR_DC_INFO(&at25ql641_dc_table),
		  SNOR_DC_QPI_SET_READING_PARAM_DFL(),
	),
};

static ufprog_status atmel_read_otp_raw(struct spi_nor *snor, uint32_t addr, uint32_t len, void *data)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_ATMEL_READ_OTP, 1),
		SPI_MEM_OP_ADDR(3, addr, 1),
		SPI_MEM_OP_DUMMY(2, 1),
		SPI_MEM_OP_DATA_IN(len, data, 1)
	);

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_status atmel_write_otp_raw(struct spi_nor *snor, uint32_t addr, uint32_t len, const void *data)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_ATMEL_PROG_OTP, 1),
		SPI_MEM_OP_ADDR(3, addr, 1),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_DATA_OUT(len, data, 1)
	);

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	STATUS_CHECK_RET(spi_nor_write_enable(snor));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_status atmel_otp_read_64b(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	return atmel_read_otp_raw(snor, addr, len, data);
}

static ufprog_status atmel_otp_write_64b(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, const void *data)
{
	return atmel_write_otp_raw(snor, addr, len, data);
}

static const struct spi_nor_flash_part_otp_ops atmel_otp_64b_ops = {
	.read = atmel_otp_read_64b,
	.write = atmel_otp_write_64b,
};

static ufprog_status atmel_read_uid_esn_64b(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	if (retlen)
		*retlen = ATMEL_ESN_UID_64B_LEN;

	if (!data)
		return UFP_OK;

	return atmel_read_otp_raw(snor, snor->ext_param.otp->size, ATMEL_ESN_UID_64B_LEN, data);
}

static ufprog_status at25ff_read_otp_raw(struct spi_nor *snor, uint32_t addr, uint32_t len, void *data)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_MICRON_READ_OTP, 1),
		SPI_MEM_OP_ADDR(3, addr, 1),
		SPI_MEM_OP_DUMMY(1, 1),
		SPI_MEM_OP_DATA_IN(len, data, 1)
	);

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_status at25ff_otp_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	return at25ff_read_otp_raw(snor, index * snor->ext_param.otp->size + addr, len, data);
}

static ufprog_status at25ff_otp_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, const void *data)
{
	return atmel_write_otp_raw(snor, index * snor->ext_param.otp->size + addr, len, data);
}

static ufprog_status at25ff_secr_otp_lock_bit(struct spi_nor *snor, uint32_t index, uint32_t *retbit,
					      const struct spi_nor_reg_access **retacc)
{
	*retbit = index + 2;
	*(retacc) = &cr_acc;

	return UFP_OK;
}

static const struct spi_nor_flash_secr_otp_ops at25ff_otp_secr_ops = {
	.otp_lock_bit = at25ff_secr_otp_lock_bit,
};

static const struct spi_nor_flash_part_otp_ops at25ff_otp_ops = {
	.secr = &at25ff_otp_secr_ops,

	.read = at25ff_otp_read,
	.write = at25ff_otp_write,
	.lock = secr_otp_lock,
	.locked = secr_otp_locked,
};

static uint32_t at_secr_otp_idx_bit8_addr(struct spi_nor *snor, uint32_t index, uint32_t addr)
{
	return (index << 8) | addr;
}

static const struct spi_nor_flash_secr_otp_ops at_otp_secr_idx_bit8_ops = {
	.otp_addr = at_secr_otp_idx_bit8_addr,
};

static const struct spi_nor_flash_part_otp_ops at_secr_otp_idx_bit8_ops = {
	.secr = &at_otp_secr_idx_bit8_ops,

	.read = secr_otp_read,
	.write = secr_otp_write,
	.erase = secr_otp_erase,
	.lock = secr_otp_lock,
	.locked = secr_otp_locked,
};

static ufprog_status at25ff_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	if (retlen)
		*retlen = ATMEL_FF_UID_LEN;

	if (!data)
		return UFP_OK;

	return at25ff_read_otp_raw(snor, 0, ATMEL_FF_UID_LEN, data);
}

static ufprog_status atmel_read_uid_winbond(struct spi_nor *snor, uint32_t uid_len, void *data, uint32_t *retlen)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_READ_UNIQUE_ID, 1),
		SPI_MEM_OP_NO_ADDR,
		SPI_MEM_OP_DUMMY(snor->state.a4b_mode ? 5 : 4, 1),
		SPI_MEM_OP_DATA_IN(uid_len, data, 1)
	);

	if (retlen)
		*retlen = uid_len;

	if (!data)
		return UFP_OK;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_status atmel_read_uid_winbond_8b(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	return atmel_read_uid_winbond(snor, 8, data, retlen);
}

static ufprog_status atmel_read_uid_winbond_16b(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	return atmel_read_uid_winbond(snor, 16, data, retlen);
}

static ufprog_status atmel_read_uid_scur_16b(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	if (retlen)
		*retlen = snor->ext_param.otp->start_index;

	if (!data)
		return UFP_OK;

	return scur_otp_read_cust(snor, 0, snor->ext_param.otp->start_index, data, false);
}

static ufprog_status atmel_part_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					struct spi_nor_flash_part_blank *bp)
{
	spi_nor_blank_part_fill_default_opcodes(bp);

	if (snor->sfdp.bfpt && snor->sfdp.bfpt_hdr->minor_ver >= SFDP_REV_MINOR_A) {
		bp->p.pp_io_caps |= BIT_SPI_MEM_IO_1_1_4;

		bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].opcode = SNOR_CMD_PAGE_PROG_QUAD_IN;
		bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_1_1_4].nmode = 0;

		if (bp->p.size > SZ_16M && (bp->p.a4b_flags & SNOR_4B_F_OPCODE)) {
			bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].opcode = SNOR_CMD_4B_PAGE_PROG_QUAD_IN;
			bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].ndummy = bp->pp_opcodes_4b[SPI_MEM_IO_1_1_4].nmode = 0;
		}
	}

	if (bp->p.vendor_flags & ATMEL_F_OTP_ESN_128B) {
		snor->ext_param.ops.read_uid = atmel_read_uid_esn_64b;
		snor->ext_param.ops.otp = &atmel_otp_64b_ops;
	} else if (bp->p.vendor_flags & ATMEL_F_OTP_UID_FF) {
		snor->ext_param.ops.read_uid = at25ff_read_uid;
		snor->ext_param.ops.otp = &at25ff_otp_ops;
	} else if (bp->p.vendor_flags & ATMEL_F_OTP_SECR) {
		snor->ext_param.ops.otp = &secr_otp_ops;
	} else if (bp->p.vendor_flags & ATMEL_F_OTP_SECR_IDX_BIT8) {
		snor->ext_param.ops.otp = &at_secr_otp_idx_bit8_ops;
	} else if (bp->p.vendor_flags & ATMEL_F_OTP_ESN_SCUR) {
		snor->ext_param.ops.read_uid = atmel_read_uid_scur_16b;
		snor->ext_param.ops.otp = &scur_otp_ops;
	}

	if (bp->p.vendor_flags & ATMEL_F_UID_WINBOND_8B)
		snor->ext_param.ops.read_uid = atmel_read_uid_winbond_8b;
	else if (bp->p.vendor_flags & ATMEL_F_UID_WINBOND_16B)
		snor->ext_param.ops.read_uid = atmel_read_uid_winbond_16b;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup atmel_fixups = {
	.pre_param_setup = atmel_part_fixup,
};

const struct spi_nor_vendor vendor_atmel = {
	.mfr_id = SNOR_VENDOR_ATMEL,
	.id = "atmel",
	.name = "Atmel/Adesto/Renesas",
	.parts = atmel_parts,
	.nparts = ARRAY_SIZE(atmel_parts),
	.default_part_fixups = &atmel_fixups,
	.vendor_flag_names = atmel_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(atmel_vendor_flag_info),
};
