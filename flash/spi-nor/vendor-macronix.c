// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Macronix SPI-NOR flash parts
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

#define MXIC_UID_LEN				16

/* CR1 bits */
#define MXIC_TB_BIT				BIT(11)
#define MXIC_DC_BIT6				BIT(14)
#define MXIC_DC_BIT7				BIT(15)
#define MXIC_DC_BIT7_6				BITS(15, 14)

/* CR2 bits */
#define MXIC_HP_MODE_BIT			BIT(17)

/* SCUR bits */
#define MXIC_SCUR_FLDO				BIT(0)
#define MXIC_SCUR_LDSO				BIT(1)

/* Octal CR2 0x300 bits */
#define MXIC_CR2_300_DC_MASK			BITS(2, 0)

/* BP Masks */
#define SR_BP3					BIT(5)
#define BP_3_0					(SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0)

 /* Macronix vendor flags */
#define MXIC_F_HP_MODE				BIT(0)
#define MXIC_F_SCUR_P_E_FAIL_IND		BIT(1)
#define MXIC_F_PP_1_4_4				BIT(2)
#define MXIC_F_OTP_64B_ESN_16B			BIT(3)
#define MXIC_F_OTP_512B_ESN_16B			BIT(4)
#define MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK	BIT(5)
#define MXIC_F_OTP_2X512B_LAST_ESN_16B		BIT(6)
#define MXIC_F_WPSEL_SCUR_BIT7			BIT(7)
#define MXIC_F_CHIP_UNPROTECT_F3		BIT(8)
#define MXIC_F_CHIP_UNPROTECT_98		BIT(9)
#define MXIC_F_WPR_4BP_TB_OTP			BIT(10)

 /* Macronix vendor runtime flags */
#define MXIC_SF_NO_QSPI				BIT(0)

static const struct spi_nor_part_flag_enum_info macronix_vendor_flag_info[] = {
	{ 0, "high-performance-mode" },
	{ 1, "program-erase-fail-indicator-in-scur" },
	{ 2, "page-program-1-4-4" },
	{ 3, "otp-64-bytes-with-esn-16-bytes" },
	{ 4, "otp-512-bytes-with-esn-16-bytes" },
	{ 5, "otp-single-with-esn-16-bytes-full-lock" },
	{ 6, "otp-2x512-bytes-with-last-esn-16-bytes" },
	{ 7, "wpsel-scur-bit7" },
	{ 8, "chip-unprotect-f3h" },
	{ 9, "chip-unprotect-98h" },
	{ 10, "wp-range-4bp-tb-in-otp" },
};

#define MXIC_REG_ACC_CR2(_addr)											\
	{ .type = SNOR_REG_NORMAL, .num = 1,									\
	  .desc[0] = { .read_opcode = SNOR_CMD_MXIC_READ_CR2, .write_opcode = SNOR_CMD_MXIC_WRITE_CR2,		\
		       .ndata = 1, .addr = (_addr), .naddr = 4, },						\
	}

static const struct spi_nor_reg_access mxic_cr2_0_acc = MXIC_REG_ACC_CR2(0);
static const struct spi_nor_reg_access mxic_cr2_200_acc = MXIC_REG_ACC_CR2(0x200);
static const struct spi_nor_reg_access mxic_cr2_300_acc = MXIC_REG_ACC_CR2(0x300);

static const struct spi_nor_reg_access mx25x_srcr_acc = SNOR_REG_ACC_SRCR(SNOR_CMD_READ_SR, SNOR_CMD_READ_SR3,
									    SNOR_CMD_WRITE_SR);

static struct spi_nor_reg_access mx25rxf_srcr_acc = {
	.type = SNOR_REG_READ_MULTI_WRITE_ONCE,
	.num = 2,
	.desc[0] = {
		.ndata = 1,
		.read_opcode = SNOR_CMD_READ_SR,
		.write_opcode = SNOR_CMD_WRITE_SR,
		.flags = SNOR_REGACC_F_SR,
	},
	.desc[1] = {
		.ndata = 2,
		.read_opcode = SNOR_CMD_READ_SR3,
	},
};

static const struct spi_nor_reg_field_item mx25rxf_scur_fields[] = {
	SNOR_REG_FIELD_YES_NO(0, 1, "FLDO", "Factory Locked-down OTP (RO)"),
	SNOR_REG_FIELD_YES_NO(1, 1, "LDSO", "Lock-down Secured OTP"),
};

static const struct spi_nor_reg_def mx25rxf_scur = SNOR_REG_DEF("SCUR", "Security Register", &scur_acc,
								mx25rxf_scur_fields);

static const struct spi_nor_reg_field_item mx25x_2bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def mx25x_2bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, mx25x_2bp_sr_fields);

static const struct snor_reg_info mx25x_2bp_regs = SNOR_REG_INFO(&mx25x_2bp_sr);

static const struct spi_nor_reg_field_item mx25x_3bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def mx25x_3bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, mx25x_3bp_sr_fields);

static const struct snor_reg_info mx25x_3bp_regs = SNOR_REG_INFO(&mx25x_3bp_sr);

static const struct spi_nor_reg_field_item mx25x_4bp_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def mx25x_4bp_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc, mx25x_4bp_sr_fields);

static const struct snor_reg_info mx25x_4bp_regs = SNOR_REG_INFO(&mx25x_4bp_sr);

static const struct spi_nor_reg_field_item mx25x_4bp_qe_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD_YES_NO(6, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def mx25x_4bp_qe_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc,
								   mx25x_4bp_qe_sr_fields);

static const struct snor_reg_info mx25x_4bp_qe_regs = SNOR_REG_INFO(&mx25x_4bp_qe_sr, &mx25rxf_scur);

static const struct spi_nor_reg_field_item mx25rxf_srcr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD_YES_NO(6, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
	SNOR_REG_FIELD(11, 1, "TB", "Top/Bottom Block Protect (OTP)"),
};

static const struct spi_nor_reg_def mx25rxf_srcr = SNOR_REG_DEF("SRCR", "Status & Configuration Registers",
								&mx25rxf_srcr_acc, mx25rxf_srcr_fields);

static const struct snor_reg_info mx25rxf_regs = SNOR_REG_INFO(&mx25rxf_srcr, &mx25rxf_scur);

static const struct spi_nor_reg_def mx25x_srcr = SNOR_REG_DEF("SRCR", "Status & Configuration Registers",
								&mx25x_srcr_acc, mx25rxf_srcr_fields);

static const struct snor_reg_info mx25x_regs = SNOR_REG_INFO(&mx25x_srcr, &mx25rxf_scur);

static const struct spi_nor_reg_field_item mx25v2039f_4bp_qe_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
	SNOR_REG_FIELD(6, 1, "BP4", "Block Protect Bit 4"),
	SNOR_REG_FIELD(7, 1, "SRWD", "Status Register Write Disable"),
};

static const struct spi_nor_reg_def mx25v2039f_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc,
								   mx25v2039f_4bp_qe_sr_fields);

static const struct snor_reg_info mx25v2039f_regs = SNOR_REG_INFO(&mx25v2039f_sr, &mx25rxf_scur);

static const struct spi_nor_reg_field_item mx25xm_octal_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "BP3", "Block Protect Bit 3"),
};

static const struct spi_nor_reg_def mx25xm_octal_sr = SNOR_REG_DEF("SR", "Status Register", &sr_acc,
								   mx25xm_octal_sr_fields);

static const struct snor_reg_info mx25xm_octal_regs = SNOR_REG_INFO(&mx25xm_octal_sr, &mx25rxf_scur);

static const struct spi_nor_otp_info mx25rxf_otp_2x512b = {
	.start_index = 0,
	.count = 2,
	.size = 0x200,
};

static const struct spi_nor_otp_info mx25x_otp_512b = {
	.start_index = 0,
	.count = 1,
	.size = 0x200,
};

static const struct spi_nor_otp_info mx25x_otp_496b = {
	.start_index = 0x10,
	.count = 1,
	.size = 0x1f0,
};

static const struct spi_nor_otp_info mx25x_otp_64b = {
	.start_index = 0,
	.count = 1,
	.size = 0x40,
};

static const struct spi_nor_otp_info mx25x_otp_48b = {
	.start_index = 0x10,
	.count = 1,
	.size = 0x30,
};

static const struct spi_nor_wp_info mx25x_wpr_type2_4bp_tb0 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_NONE(0                                         ),	/* None */
	SNOR_WP_ALL(       SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_UP(                                SR_BP0, 0),	/* Upper 64KB */
	SNOR_WP_BP_UP(                       SR_BP1         , 1),	/* Upper 128KB */
	SNOR_WP_BP_UP(                       SR_BP1 | SR_BP0, 2),	/* Upper 256KB */
	SNOR_WP_BP_UP(              SR_BP2                  , 3),	/* Upper 512KB */
	SNOR_WP_BP_UP(              SR_BP2 |          SR_BP0, 4),	/* Upper 1MB */
	SNOR_WP_BP_UP(              SR_BP2 | SR_BP1         , 5),	/* Upper 2MB */
	SNOR_WP_BP_UP(              SR_BP2 | SR_BP1 | SR_BP0, 6),	/* Upper 4MB */
	SNOR_WP_BP_CMPF_LO(SR_BP3                           , 6),	/* Lower T - 4MB */
	SNOR_WP_BP_CMPF_LO(SR_BP3 |                   SR_BP0, 5),	/* Lower T - 2MB */
	SNOR_WP_BP_CMPF_LO(SR_BP3 |          SR_BP1         , 4),	/* Lower T - 1MB */
	SNOR_WP_BP_CMPF_LO(SR_BP3 |          SR_BP1 | SR_BP0, 3),	/* Lower T - 512KB */
	SNOR_WP_BP_CMPF_LO(SR_BP3 | SR_BP2                  , 2),	/* Lower T - 256KB */
	SNOR_WP_BP_CMPF_LO(SR_BP3 | SR_BP2 |          SR_BP0, 1),	/* Lower T - 128KB */
	SNOR_WP_BP_CMPF_LO(SR_BP3 | SR_BP2 | SR_BP1         , 0),	/* Lower T - 64KB */
);

static const struct spi_nor_wp_info mx25x_wpr_type2_4bp_tb1 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_NONE(0                                         ),	/* None */
	SNOR_WP_ALL(       SR_BP3 | SR_BP2 | SR_BP1 | SR_BP0   ),	/* All */

	SNOR_WP_BP_LO(                                SR_BP0, 0),	/* Lower 64KB */
	SNOR_WP_BP_LO(                       SR_BP1         , 1),	/* Lower 128KB */
	SNOR_WP_BP_LO(                       SR_BP1 | SR_BP0, 2),	/* Lower 256KB */
	SNOR_WP_BP_LO(              SR_BP2                  , 3),	/* Lower 512KB */
	SNOR_WP_BP_LO(              SR_BP2 |          SR_BP0, 4),	/* Lower 1MB */
	SNOR_WP_BP_LO(              SR_BP2 | SR_BP1         , 5),	/* Lower 2MB */
	SNOR_WP_BP_LO(              SR_BP2 | SR_BP1 | SR_BP0, 4),	/* Lower 4MB */
	SNOR_WP_BP_CMPF_UP(SR_BP3                           , 6),	/* Upper T - 4MB */
	SNOR_WP_BP_CMPF_UP(SR_BP3 |                   SR_BP0, 5),	/* Upper T - 2MB */
	SNOR_WP_BP_CMPF_UP(SR_BP3 |          SR_BP1         , 4),	/* Upper T - 1MB */
	SNOR_WP_BP_CMPF_UP(SR_BP3 |          SR_BP1 | SR_BP0, 3),	/* Upper T - 512KB */
	SNOR_WP_BP_CMPF_UP(SR_BP3 | SR_BP2                  , 2),	/* Upper T - 256KB */
	SNOR_WP_BP_CMPF_UP(SR_BP3 | SR_BP2 |          SR_BP0, 1),	/* Upper T - 128KB */
	SNOR_WP_BP_CMPF_UP(SR_BP3 | SR_BP2 | SR_BP1         , 0),	/* Upper T - 64KB */
);

static const struct spi_nor_wp_info mx25x_wpr_type3_4bp_tb0 = SNOR_WP_BP(&sr_acc, BP_3_0,
	SNOR_WP_NONE(0                                    ),	/* None */
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

/* MX25V512F */
static const SNOR_DC_CONFIG(mx25v512f_dc_122_cfgs, SNOR_DC_IDX_VALUE(0, 4, 104), SNOR_DC_IDX_VALUE(1, 8, 104));
static const SNOR_DC_CONFIG(mx25v512f_dc_144_cfgs, SNOR_DC_IDX_VALUE(0, 6, 104), SNOR_DC_IDX_VALUE(1, 10, 104));

static const SNOR_DC_TABLE(mxic_cr1_bit6_all_104mhz_dc_table, 1,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25v512f_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25v512f_dc_144_cfgs));

/* MX25V1635F */
static const SNOR_DC_CONFIG(mx25v1635f_dc_122_cfgs, SNOR_DC_IDX_VALUE(0, 4, 80), SNOR_DC_IDX_VALUE(1, 8, 80));
static const SNOR_DC_CONFIG(mx25v1635f_dc_144_cfgs, SNOR_DC_IDX_VALUE(0, 6, 80), SNOR_DC_IDX_VALUE(1, 10, 80));

static const SNOR_DC_TABLE(mxic_cr1_bit6_all_80mhz_dc_table, 1,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25v1635f_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25v1635f_dc_144_cfgs));

/* MX25L3233F */
static const SNOR_DC_CONFIG(mx25l3233f_dc_122_cfgs, SNOR_DC_IDX_VALUE(1, 8, 133), SNOR_DC_IDX_VALUE(0, 4, 104));
static const SNOR_DC_CONFIG(mx25l3233f_dc_144_cfgs, SNOR_DC_IDX_VALUE(1, 10, 133), SNOR_DC_IDX_VALUE(0, 6, 104));

static const SNOR_DC_TABLE(mxic_cr1_bit6_104_133mhz_dc_table, 1,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25l3233f_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l3233f_dc_144_cfgs));

/* MX25L32356 */
static const SNOR_DC_CONFIG(mx25l32356_dc_122_cfgs, SNOR_DC_IDX_VALUE(1, 8, 120), SNOR_DC_IDX_VALUE(0, 4, 80));
static const SNOR_DC_CONFIG(mx25l32356_dc_144_cfgs, SNOR_DC_IDX_VALUE(1, 10, 120), SNOR_DC_IDX_VALUE(0, 6, 80));

static const SNOR_DC_TABLE(mxic_cr1_bit6_80_120mhz_dc_table, 1,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25l32356_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l32356_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25l32356_dc_144_cfgs));

/* MX25L6433F */
static const SNOR_DC_CONFIG(mx25l6433f_dc_122_cfgs, SNOR_DC_IDX_VALUE(1, 8, 133), SNOR_DC_IDX_VALUE(0, 4, 80));
static const SNOR_DC_CONFIG(mx25l6433f_dc_144_cfgs, SNOR_DC_IDX_VALUE(1, 10, 133), SNOR_DC_IDX_VALUE(0, 6, 80));

static const SNOR_DC_TABLE(mxic_cr1_bit6_80_133mhz_dc_table, 1,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25l6433f_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l6433f_dc_144_cfgs));

/* MX25L3235E */
static const SNOR_DC_CONFIG(mx25l3235e_dc_144_cfgs, SNOR_DC_IDX_VALUE(1, 8, 104), SNOR_DC_IDX_VALUE(0, 6, 86));

static const SNOR_DC_TABLE(mxic_cr1_bit7_86_104mhz_dc_table, 1,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l3235e_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25l3235e_dc_144_cfgs));

/* MX25U12835F */
static const SNOR_DC_CONFIG(mx25u12835f_dc_144_cfgs, /* SNOR_DC_IDX_VALUE(1, 8, 133), */ SNOR_DC_IDX_VALUE(0, 6, 104));

static const SNOR_DC_TABLE(mxic_cr1_bit7_104_133mhz_dc_table, 1,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25u12835f_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25u12835f_dc_144_cfgs));

/* CR1 DC7_6 Default */
static const SNOR_DC_CONFIG(mxic_cr1_dc7_6_dfl_11x_cfgs, SNOR_DC_IDX_VALUE(0, 8, 60));
static const SNOR_DC_CONFIG(mxic_cr1_dc7_6_dfl_122_cfgs, SNOR_DC_IDX_VALUE(0, 4, 60));
static const SNOR_DC_CONFIG(mxic_cr1_dc7_6_dfl_144_cfgs, SNOR_DC_IDX_VALUE(0, 6, 60));

static const SNOR_DC_TABLE(mxic_cr1_dc7_6_dfl_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_1, mxic_cr1_dc7_6_dfl_11x_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_2, mxic_cr1_dc7_6_dfl_11x_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mxic_cr1_dc7_6_dfl_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_4, mxic_cr1_dc7_6_dfl_11x_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mxic_cr1_dc7_6_dfl_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mxic_cr1_dc7_6_dfl_144_cfgs));

/* MX25U20356 */
static const SNOR_DC_CONFIG(mx25u20356_dc_122_cfgs, SNOR_DC_IDX_VALUE(1, 8, 133), SNOR_DC_IDX_VALUE(0, 4, 104));
static const SNOR_DC_CONFIG(mx25u20356_dc_144_cfgs, SNOR_DC_IDX_VALUE(1, 10, 133), SNOR_DC_IDX_VALUE(0, 6, 104));

static const SNOR_DC_TABLE(mxic_cr1_bit7_6_104_133mhz_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25u20356_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25u20356_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25u20356_dc_144_cfgs));

/* MX25U1632F */
static const SNOR_DC_CONFIG(mx25u1632f_dc_111_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 8, 104), SNOR_DC_IDX_VALUE(1, 6, 104));

static const SNOR_DC_CONFIG(mx25u1632f_dc_122_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(1, 6, 104), SNOR_DC_IDX_VALUE(0, 4, 84));

static const SNOR_DC_CONFIG(mx25u1632f_dc_114_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 8, 104), SNOR_DC_IDX_VALUE(1, 6, 84));

static const SNOR_DC_CONFIG(mx25u1632f_dc_144_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 6, 84), SNOR_DC_IDX_VALUE(1, 4, 66));

static const SNOR_DC_TABLE(mx25u1632f_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_1, mx25u1632f_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_2, mx25u1632f_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25u1632f_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_4, mx25u1632f_dc_114_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25u1632f_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25u1632f_dc_144_cfgs));

/* MX25L12833F */
static const SNOR_DC_CONFIG(mx25l12833f_dc_111_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 8, 104), SNOR_DC_IDX_VALUE(1, 6, 104));

static const SNOR_DC_CONFIG(mx25l12833f_dc_122_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(1, 6, 104), SNOR_DC_IDX_VALUE(0, 4, 84));

static const SNOR_DC_CONFIG(mx25l12833f_dc_114_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 8, 104), SNOR_DC_IDX_VALUE(1, 6, 84));

static const SNOR_DC_CONFIG(mx25l12833f_dc_144_cfgs, SNOR_DC_IDX_VALUE(3, 10, 120), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 6, 84), SNOR_DC_IDX_VALUE(1, 4, 66));

static const SNOR_DC_TABLE(mx25l12833f_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_1, mx25l12833f_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_2, mx25l12833f_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25l12833f_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_4, mx25l12833f_dc_114_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l12833f_dc_144_cfgs));

/* MX25L12835F */
static const SNOR_DC_CONFIG(mx25l12835f_dc_111_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 8, 104), SNOR_DC_IDX_VALUE(1, 6, 104));

static const SNOR_DC_CONFIG(mx25l12835f_dc_122_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(1, 6, 104), SNOR_DC_IDX_VALUE(0, 4, 84));

static const SNOR_DC_CONFIG(mx25l12835f_dc_114_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 8, 104), SNOR_DC_IDX_VALUE(1, 6, 84));

static const SNOR_DC_CONFIG(mx25l12835f_dc_144_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 6, 84), SNOR_DC_IDX_VALUE(1, 4, 70));

static const SNOR_DC_TABLE(mx25l12835f_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_1, mx25l12835f_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_2, mx25l12835f_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25l12835f_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_4, mx25l12835f_dc_114_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l12835f_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25l12835f_dc_144_cfgs));

/* MX25L12839F */
static const SNOR_DC_CONFIG(mx25l12839f_dc_111_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 8, 104), SNOR_DC_IDX_VALUE(1, 6, 104));

static const SNOR_DC_CONFIG(mx25l12839f_dc_114_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 8, 104), SNOR_DC_IDX_VALUE(1, 6, 84));

static const SNOR_DC_CONFIG(mx25l12839f_dc_144_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 6, 84), SNOR_DC_IDX_VALUE(1, 4, 70));

static const SNOR_DC_TABLE(mx25l12839f_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_1, mx25l12839f_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_4, mx25l12839f_dc_114_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l12839f_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25l12839f_dc_144_cfgs));

/* MX25L12845G */
static const SNOR_DC_CONFIG(mx25l12845g_dc_122_cfgs, SNOR_DC_IDX_VALUE(3, 8, 120), SNOR_DC_IDX_VALUE(1, 8, 120),
			    SNOR_DC_IDX_VALUE(2, 4, 80), SNOR_DC_IDX_VALUE(0, 4, 80));

static const SNOR_DC_CONFIG(mx25l12845g_dc_144_cfgs, SNOR_DC_IDX_VALUE(3, 10, 120), SNOR_DC_IDX_VALUE(2, 8, 84),
			    SNOR_DC_IDX_VALUE(0, 6, 80), SNOR_DC_IDX_VALUE(1, 4, 54));

static const SNOR_DC_TABLE(mx25l12845g_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25l12845g_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l12845g_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25l12845g_dc_144_cfgs));

/* MX25L12843G */
static const SNOR_DC_CONFIG(mx25l12843g_dc_122_cfgs, SNOR_DC_IDX_VALUE(3, 8, 133), SNOR_DC_IDX_VALUE(1, 8, 133),
			    SNOR_DC_IDX_VALUE(2, 4, 84), SNOR_DC_IDX_VALUE(0, 4, 84));

static const SNOR_DC_CONFIG(mx25l12843g_dc_144_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 6, 84), SNOR_DC_IDX_VALUE(1, 4, 66));

static const SNOR_DC_TABLE(mx25l12843g_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25l12843g_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l12843g_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25l12843g_dc_144_cfgs));

/* MX25L25633F */
static const SNOR_DC_CONFIG(mx25l25633f_dc_111_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 8, 104), SNOR_DC_IDX_VALUE(1, 6, 104));

static const SNOR_DC_CONFIG(mx25l25633f_dc_122_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(1, 6, 104), SNOR_DC_IDX_VALUE(0, 4, 84));

static const SNOR_DC_CONFIG(mx25l25633f_dc_114_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 8, 104), SNOR_DC_IDX_VALUE(1, 6, 84));

static const SNOR_DC_CONFIG(mx25l25633f_dc_144_cfgs, SNOR_DC_IDX_VALUE(3, 10, 120), SNOR_DC_IDX_VALUE(2, 8, 95),
			    SNOR_DC_IDX_VALUE(0, 6, 80), SNOR_DC_IDX_VALUE(1, 4, 60));

static const SNOR_DC_TABLE(mx25l25633f_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_1, mx25l25633f_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_2, mx25l25633f_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25l25633f_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_4, mx25l25633f_dc_114_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l25633f_dc_144_cfgs));

/* MX25U25635F */
static const SNOR_DC_CONFIG(mx25u25635f_dc_111_cfgs, /* SNOR_DC_IDX_VALUE(3, 10, 133), */ SNOR_DC_IDX_VALUE(2, 8, 108),
			    SNOR_DC_IDX_VALUE(0, 8, 108), SNOR_DC_IDX_VALUE(1, 6, 108));

static const SNOR_DC_CONFIG(mx25u25635f_dc_122_cfgs, /* SNOR_DC_IDX_VALUE(3, 10, 133), */ SNOR_DC_IDX_VALUE(2, 8, 108),
			    SNOR_DC_IDX_VALUE(1, 6, 108), SNOR_DC_IDX_VALUE(0, 4, 84));

static const SNOR_DC_CONFIG(mx25u25635f_dc_114_cfgs, /* SNOR_DC_IDX_VALUE(3, 10, 133), */ SNOR_DC_IDX_VALUE(2, 8, 108),
			    SNOR_DC_IDX_VALUE(0, 8, 108), SNOR_DC_IDX_VALUE(1, 6, 84));

static const SNOR_DC_CONFIG(mx25u25635f_dc_144_cfgs, /* SNOR_DC_IDX_VALUE(3, 10, 133), */ SNOR_DC_IDX_VALUE(2, 8, 108),
			    SNOR_DC_IDX_VALUE(0, 6, 84), SNOR_DC_IDX_VALUE(1, 4, 70));

static const SNOR_DC_TABLE(mx25u25635f_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_1, mx25u25635f_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_2, mx25u25635f_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25u25635f_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_4, mx25u25635f_dc_114_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25u25635f_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25u25635f_dc_144_cfgs));

/* MX25L25643G */
static const SNOR_DC_CONFIG(mx25l25643g_dc_122_cfgs, SNOR_DC_IDX_VALUE(3, 8, 120), SNOR_DC_IDX_VALUE(1, 8, 120),
			    SNOR_DC_IDX_VALUE(2, 4, 84), SNOR_DC_IDX_VALUE(0, 4, 84));

static const SNOR_DC_CONFIG(mx25l25643g_dc_144_cfgs, SNOR_DC_IDX_VALUE(3, 10, 120), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 6, 84), SNOR_DC_IDX_VALUE(1, 4, 66));

static const SNOR_DC_TABLE(mx25l25643g_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25l25643g_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l25643g_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25l25643g_dc_144_cfgs));

/* MX25U25645G */
static const SNOR_DC_CONFIG(mx25u25645g_dc_111_cfgs, SNOR_DC_IDX_VALUE(3, 10, 166), SNOR_DC_IDX_VALUE(2, 8, 133),
			    SNOR_DC_IDX_VALUE(0, 8, 133), SNOR_DC_IDX_VALUE(1, 6, 133));

static const SNOR_DC_CONFIG(mx25u25645g_dc_122_cfgs, SNOR_DC_IDX_VALUE(3, 10, 166), SNOR_DC_IDX_VALUE(2, 8, 133),
			    SNOR_DC_IDX_VALUE(1, 6, 104), SNOR_DC_IDX_VALUE(0, 4, 84));

static const SNOR_DC_CONFIG(mx25u25645g_dc_114_cfgs, SNOR_DC_IDX_VALUE(3, 10, 166), SNOR_DC_IDX_VALUE(2, 8, 133),
			    SNOR_DC_IDX_VALUE(0, 8, 133), SNOR_DC_IDX_VALUE(1, 6, 104));

static const SNOR_DC_CONFIG(mx25u25645g_dc_144_cfgs, SNOR_DC_IDX_VALUE(3, 10, 133), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 6, 84), SNOR_DC_IDX_VALUE(1, 4, 70));

static const SNOR_DC_TABLE(mx25u25645g_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_1, mx25u25645g_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_2, mx25u25645g_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25u25645g_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_4, mx25u25645g_dc_114_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25u25645g_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25u25645g_dc_144_cfgs));

/* MX25U25645G-54 */
static const SNOR_DC_CONFIG(mx25u25645g54_dc_111_cfgs, SNOR_DC_IDX_VALUE(0, 10, 166), SNOR_DC_IDX_VALUE(1, 8, 133),
			    SNOR_DC_IDX_VALUE(3, 8, 133), SNOR_DC_IDX_VALUE(2, 6, 133));

static const SNOR_DC_CONFIG(mx25u25645g54_dc_122_cfgs, SNOR_DC_IDX_VALUE(0, 10, 166), SNOR_DC_IDX_VALUE(1, 8, 133),
			    SNOR_DC_IDX_VALUE(2, 6, 104), SNOR_DC_IDX_VALUE(3, 4, 84));

static const SNOR_DC_CONFIG(mx25u25645g54_dc_114_cfgs, SNOR_DC_IDX_VALUE(0, 10, 166), SNOR_DC_IDX_VALUE(1, 8, 133),
			    SNOR_DC_IDX_VALUE(3, 8, 133), SNOR_DC_IDX_VALUE(2, 6, 104));

static const SNOR_DC_CONFIG(mx25u25645g54_dc_144_cfgs, SNOR_DC_IDX_VALUE(0, 10, 133), SNOR_DC_IDX_VALUE(1, 8, 104),
			    SNOR_DC_IDX_VALUE(3, 6, 84), SNOR_DC_IDX_VALUE(2, 4, 70));

static const SNOR_DC_TABLE(mx25u25645g54_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_1, mx25u25645g54_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_2, mx25u25645g54_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25u25645g54_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_4, mx25u25645g54_dc_114_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25u25645g54_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25u25645g54_dc_144_cfgs));

/* MX25L51245J */
static const SNOR_DC_CONFIG(mx25l51245j_dc_122_cfgs, SNOR_DC_IDX_VALUE(3, 8, 104), SNOR_DC_IDX_VALUE(2, 4, 80),
			    SNOR_DC_IDX_VALUE(1, 8, 104), SNOR_DC_IDX_VALUE(0, 4, 80));

static const SNOR_DC_CONFIG(mx25l51245j_dc_144_cfgs, SNOR_DC_IDX_VALUE(3, 10, 104), SNOR_DC_IDX_VALUE(2, 8, 84),
			    SNOR_DC_IDX_VALUE(0, 6, 70), SNOR_DC_IDX_VALUE(1, 4, 54));

static const SNOR_DC_TABLE(mx25l51245j_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25l51245j_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l51245j_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25l51245j_dc_144_cfgs));

/* MX25L51237G */
static const SNOR_DC_CONFIG(mx25l51237g_dc_111_cfgs, SNOR_DC_IDX_VALUE(3, 10, 104), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 8, 104), SNOR_DC_IDX_VALUE(1, 6, 84));

static const SNOR_DC_CONFIG(mx25l51237g_dc_122_cfgs, SNOR_DC_IDX_VALUE(3, 10, 104), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(1, 6, 84), SNOR_DC_IDX_VALUE(0, 4, 66));

static const SNOR_DC_CONFIG(mx25l51237g_dc_144_cfgs, SNOR_DC_IDX_VALUE(3, 10, 104), SNOR_DC_IDX_VALUE(2, 8, 104),
			    SNOR_DC_IDX_VALUE(0, 6, 84), SNOR_DC_IDX_VALUE(1, 4, 66));

static const SNOR_DC_TABLE(mx25l51237g_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_1, mx25l51237g_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_2, mx25l51237g_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_2_2, mx25l51237g_dc_122_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_4, mx25l51237g_dc_111_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_1_4_4, mx25l51237g_dc_144_cfgs),
			   SNOR_DC_TIMING(SPI_MEM_IO_4_4_4, mx25l51237g_dc_144_cfgs));

/* Octal */
static const SNOR_DC_CONFIG(mxix_octal_dc_legacy_cfgs, SNOR_DC_IDX_VALUE(0, 8, 133));

static const SNOR_DC_TABLE(mxix_octal_dc_table, 3,
			   SNOR_DC_TIMING(SPI_MEM_IO_1_1_1, mxix_octal_dc_legacy_cfgs));

static const SNOR_DC_CHIP_SETUP_ACC(mxic_dc_acc_cr1_bit6, &mx25x_srcr_acc, 1, 14);
static const SNOR_DC_CHIP_SETUP_ACC(mxic_dc_acc_cr1_bit7, &mx25x_srcr_acc, 1, 15);
static const SNOR_DC_CHIP_SETUP_ACC(mxic_dc_acc_cr1_bit7_6, &mx25x_srcr_acc, 3, 14);
static const SNOR_DC_CHIP_SETUP_ACC(mxic_dc_acc_cr2_300, &mxic_cr2_300_acc, 7, 0);


static ufprog_status mx25x_wpr_type2_4bp_tb_select(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
						   struct spi_nor_flash_part_blank *bp)
{
	uint32_t regval;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &mx25rxf_srcr_acc, &regval));

	if (regval & MXIC_TB_BIT)
		bp->p.wp_ranges = &mx25x_wpr_type2_4bp_tb1;
	else
		bp->p.wp_ranges = &mx25x_wpr_type2_4bp_tb0;

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup mx25x_wpr_type2_4bp_tb_fixups = {
	.pre_param_setup = mx25x_wpr_type2_4bp_tb_select,
};

static ufprog_status mx25x512_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					  struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt)
		return spi_nor_reprobe_part(snor, vp, bp, NULL, "MX25L512E");

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup mx25x512_fixups = {
	.pre_param_setup = mx25x512_fixup_model,
};

static ufprog_status mx25l10xx_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "MX25L1006E"));

		if (((uint8_t *)snor->sfdp.data)[0x30] == 0xfd) {
			bp->p.model = bp->model;
			snprintf(bp->model, sizeof(bp->model), "MX25L1026E");
		}
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup mx25l10xx_fixups = {
	.pre_param_setup = mx25l10xx_fixup_model,
};

static ufprog_status mx25l2026c_write_enable(struct spi_nor *snor);

static ufprog_status mx25l2026c_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				      struct spi_nor_flash_part_blank *bp)
{
	snor->ext_param.data_write_enable = mx25l2026c_write_enable;
	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup mx25l2026c_fixups = {
	.pre_param_setup = mx25l2026c_fixup,
};

static ufprog_status mx25u32xx_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == 0 && ((uint8_t *)snor->sfdp.data)[0x61] == 0x36)
			STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "MX25L3239E"));
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup mx25u32xx_fixups = {
	.pre_param_setup = mx25u32xx_fixup_model,
};

static ufprog_status mx25u64xx_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					   struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == 0 && ((uint8_t *)snor->sfdp.data)[0x61] == 0x36)
			STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "MX25L6439E"));
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup mx25u64xx_fixups = {
	.pre_param_setup = mx25u64xx_fixup_model,
};

static ufprog_status mx66l512xxx_fixup_model(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					     struct spi_nor_flash_part_blank *bp)
{
	if (snor->sfdp.bfpt) {
		if (snor->sfdp.bfpt_hdr->minor_ver == 0)
			STATUS_CHECK_RET(spi_nor_reprobe_part(snor, vp, bp, NULL, "MX66L51235F"));
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup mx66l512xxx_fixups = {
	.pre_param_setup = mx66l512xxx_fixup_model,
};

static DEFINE_SNOR_ALIAS(mx25v512_alias, SNOR_ALIAS_MODEL("MX25V512C"));
static DEFINE_SNOR_ALIAS(mx25l1005_alias, SNOR_ALIAS_MODEL("MX25L1005C"), SNOR_ALIAS_MODEL("MX25L1025C"));
static DEFINE_SNOR_ALIAS(mx25l1006e_alias, SNOR_ALIAS_MODEL("MX25L1026E"));
static DEFINE_SNOR_ALIAS(mx25l2005_alias, SNOR_ALIAS_MODEL("MX25L2005C"));
static DEFINE_SNOR_ALIAS(mx25l2006e_alias, SNOR_ALIAS_MODEL("MX25L2026E"), SNOR_ALIAS_MODEL("MX25V2006E"));
static DEFINE_SNOR_ALIAS(mx25l4005_alias, SNOR_ALIAS_MODEL("MX25L4005A"), SNOR_ALIAS_MODEL("MX25V4005C"));
static DEFINE_SNOR_ALIAS(mx25l4006e_alias, SNOR_ALIAS_MODEL("MX25L4026E"), SNOR_ALIAS_MODEL("MX25V4006E"));
static DEFINE_SNOR_ALIAS(mx25u4032e_alias, SNOR_ALIAS_MODEL("MX25U4033E"));
static DEFINE_SNOR_ALIAS(mx25l8005_alias, SNOR_ALIAS_MODEL("MX25V8005"));
static DEFINE_SNOR_ALIAS(mx25l8006e_alias, SNOR_ALIAS_MODEL("MX25L8008E"), SNOR_ALIAS_MODEL("MX25V8006E"));
static DEFINE_SNOR_ALIAS(mx25l8035e_alias, SNOR_ALIAS_MODEL("MX25L8036E"));
static DEFINE_SNOR_ALIAS(mx25v16066_alias, SNOR_ALIAS_MODEL("MX25V1606F"));
static DEFINE_SNOR_ALIAS(mx25l1633e_alias, SNOR_ALIAS_MODEL("MX25L1635D"));
static DEFINE_SNOR_ALIAS(mx25u1632f_alias, SNOR_ALIAS_MODEL("MX25U16356"));
static DEFINE_SNOR_ALIAS(mx25l3205_alias, SNOR_ALIAS_MODEL("MX25L3205A"));
static DEFINE_SNOR_ALIAS(mx25l3225d_alias, SNOR_ALIAS_MODEL("MX25L3235D"));
static DEFINE_SNOR_ALIAS(mx25l3233f_alias, SNOR_ALIAS_MODEL("MX25L3273F"));
static DEFINE_SNOR_ALIAS(mx25l3273e_alias, SNOR_ALIAS_MODEL("MX25L3275E"));
static DEFINE_SNOR_ALIAS(mx25u3232f_alias, SNOR_ALIAS_MODEL("MX25U32356"));
static DEFINE_SNOR_ALIAS(mx25l6433f_alias, SNOR_ALIAS_MODEL("MX25L6473F"));
static DEFINE_SNOR_ALIAS(mx25l64356_alias, SNOR_ALIAS_MODEL("MX25L64736"));
static DEFINE_SNOR_ALIAS(mx25l6435e_alias, SNOR_ALIAS_MODEL("MX25L6473E"), SNOR_ALIAS_MODEL("MX25L6475E"));
static DEFINE_SNOR_ALIAS(mx25l6436e_alias, SNOR_ALIAS_MODEL("MX25L6445E"), SNOR_ALIAS_MODEL("MX25L6465E"));
static DEFINE_SNOR_ALIAS(mx25u6432f_alias, SNOR_ALIAS_MODEL("MX25U64356"), SNOR_ALIAS_MODEL("MX25U6472F"),
					   SNOR_ALIAS_MODEL("MX25U64736"));
static DEFINE_SNOR_ALIAS(mx25u6435f_alias, SNOR_ALIAS_MODEL("MX25U6473F"));
static DEFINE_SNOR_ALIAS(mx25l128356_alias, SNOR_ALIAS_MODEL("MX25L128736"));
static DEFINE_SNOR_ALIAS(mx25l12845g_alias, SNOR_ALIAS_MODEL("MX25L12873G"));
static DEFINE_SNOR_ALIAS(mx25l12836e_alias, SNOR_ALIAS_MODEL("MX25L12865E"));
static DEFINE_SNOR_ALIAS(mx25l12835f_alias, SNOR_ALIAS_MODEL("MX25L12873F"), SNOR_ALIAS_MODEL("MX25L12875F"));
static DEFINE_SNOR_ALIAS(mx25u12832f_alias, SNOR_ALIAS_MODEL("MX25U12872F"));
static DEFINE_SNOR_ALIAS(mx25l25633f_alias, SNOR_ALIAS_MODEL("MX25L25672F"), SNOR_ALIAS_MODEL("MX25L25733F"));
static DEFINE_SNOR_ALIAS(mx25l25645g_alias, SNOR_ALIAS_MODEL("MX25L25673G"), SNOR_ALIAS_MODEL("MX25L25745G"),
					    SNOR_ALIAS_MODEL("MX25L25773G"));
static DEFINE_SNOR_ALIAS(mx25u25643g_alias, SNOR_ALIAS_MODEL("MX25U25672G"));
static DEFINE_SNOR_ALIAS(mx25u25645g_alias, SNOR_ALIAS_MODEL("MX25U25673G"));
static DEFINE_SNOR_ALIAS(mx25l51245g_alias, SNOR_ALIAS_MODEL("MX25L51273G"));
static DEFINE_SNOR_ALIAS(mx25u51245g_alias, SNOR_ALIAS_MODEL("MX25U51293G"));
static DEFINE_SNOR_ALIAS(mx25u1g45g_alias, SNOR_ALIAS_MODEL("MX66U1G93G"));

static const struct spi_nor_flash_part macronix_parts[] = {
	SNOR_PART("MX25*512", SNOR_ID(0xc2, 0x20, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_FIXUPS(&mx25x512_fixups),
	),

	SNOR_PART("MX25L512C", SNOR_ID(0xc2, 0x20, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("MX25L512E", SNOR_ID(0xc2, 0x20, 0x10), SZ_64K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("MX25V512", SNOR_ID(0xc2, 0x20, 0x10), SZ_64K,
		  SNOR_ALIAS(&mx25v512_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("MX25V512E", SNOR_ID(0xc2, 0x20, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75), SNOR_DUAL_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("MX25V5126F", SNOR_ID(0xc2, 0x20, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(80), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L5121E", SNOR_ID(0xc2, 0x22, 0x10), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(45),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("MX25V512F", SNOR_ID(0xc2, 0x23, 0x10), SZ_64K, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25U5121E", SNOR_ID(0xc2, 0x25, 0x30), SZ_64K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(60),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("MX25R512F", SNOR_ID(0xc2, 0x28, 0x10), SZ_64K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_HP_MODE | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25rxf_regs),
	),

	/**********************************************************************/

	SNOR_PART("MX25*10**", SNOR_ID(0xc2, 0x20, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up),
		  SNOR_FIXUPS(&mx25l10xx_fixups),
	),

	SNOR_PART("MX25L1005", SNOR_ID(0xc2, 0x20, 0x11), SZ_128K,
		  SNOR_ALIAS(&mx25l1005_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(66),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up),
	),

	SNOR_PART("MX25L1006E", SNOR_ID(0xc2, 0x20, 0x11), SZ_128K, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l1006e_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up),
	),

	SNOR_PART("MX25V1006E", SNOR_ID(0xc2, 0x20, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75), SNOR_DUAL_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up),
	),

	SNOR_PART("MX25V1006F", SNOR_ID(0xc2, 0x20, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L1021E", SNOR_ID(0xc2, 0x22, 0x11), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(45),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("MX25V1035F", SNOR_ID(0xc2, 0x23, 0x11), SZ_128K, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25U1021E", SNOR_ID(0xc2, 0x25, 0x31), SZ_128K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(60),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("MX25R1035F", SNOR_ID(0xc2, 0x28, 0x11), SZ_128K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_HP_MODE | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&mx25rxf_regs),
	),

	/**********************************************************************/
	SNOR_PART("MX25*20**", SNOR_ID(0xc2, 0x20, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("MX25L2005", SNOR_ID(0xc2, 0x20, 0x12), SZ_256K,
		  SNOR_ALIAS(&mx25l2005_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(66),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_up),
	),

	SNOR_PART("MX25L2006E", SNOR_ID(0xc2, 0x20, 0x12), SZ_256K, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l2006e_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75), SNOR_DUAL_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_2bp_regs),
		  SNOR_WP_RANGES(&wpr_2bp_all),
	),

	SNOR_PART("MX25L2026C", SNOR_ID(0xc2, 0x20, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(85),
		  SNOR_FIXUPS(&mx25l2026c_fixups),
	),

	SNOR_PART("MX25V2033F", SNOR_ID(0xc2, 0x20, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK),
		  SNOR_QE_SR1_BIT6,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(50), SNOR_DUAL_MAX_SPEED_MHZ(50), SNOR_QUAD_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_2bp_tb),
		  SNOR_OTP_INFO(&mx25x_otp_512b),
	),

	SNOR_PART("MX25V2039F", SNOR_ID(0xc2, 0x20, 0x12), SZ_256K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK),
		  SNOR_QE_DONT_CARE,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(50), SNOR_DUAL_MAX_SPEED_MHZ(50), SNOR_QUAD_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&mx25v2039f_regs),
		  SNOR_OTP_INFO(&mx25x_otp_512b),
	),

	SNOR_PART("MX25V20066", SNOR_ID(0xc2, 0x20, 0x12), SZ_256K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_4bp_up),
	),

	SNOR_PART("MX25V2035F", SNOR_ID(0xc2, 0x23, 0x12), SZ_256K, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25U20***", SNOR_ID(0xc2, 0x25, 0x32), SZ_256K,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("MX25U2033E", SNOR_ID(0xc2, 0x25, 0x32), SZ_256K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_2bp_tb),
	),

	SNOR_PART("MX25U2035F", SNOR_ID(0xc2, 0x25, 0x32), SZ_256K, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25U20356", SNOR_ID(0xc2, 0x25, 0x32), SZ_256K, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit7_6_104_133mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25R2035F", SNOR_ID(0xc2, 0x28, 0x12), SZ_256K, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_HP_MODE | MXIC_F_SCUR_P_E_FAIL_IND |
				    MXIC_F_OTP_2X512B_LAST_ESN_16B | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25rxf_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	/**********************************************************************/

	SNOR_PART("MX25*40**", SNOR_ID(0xc2, 0x20, 0x13), SZ_512K,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("MX25L4005", SNOR_ID(0xc2, 0x20, 0x13), SZ_512K,
		  SNOR_ALIAS(&mx25l4005_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("MX25L4006E", SNOR_ID(0xc2, 0x20, 0x13), SZ_512K, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l4006e_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75), SNOR_DUAL_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("MX25V40066", SNOR_ID(0xc2, 0x20, 0x13), SZ_512K, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_4bp_up),
	),

	SNOR_PART("MX25V4035F", SNOR_ID(0xc2, 0x23, 0x13), SZ_512K, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25V4035", SNOR_ID(0xc2, 0x25, 0x53), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(66), SNOR_DUAL_MAX_SPEED_MHZ(50), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25U40***", SNOR_ID(0xc2, 0x25, 0x33), SZ_512K,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_CHIP_UNPROTECT_98),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("MX25U4032E", SNOR_ID(0xc2, 0x25, 0x33), SZ_512K, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25u4032e_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25U4035", SNOR_ID(0xc2, 0x25, 0x33), SZ_512K,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(40), SNOR_DUAL_MAX_SPEED_MHZ(40), SNOR_DUAL_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25U4035F", SNOR_ID(0xc2, 0x25, 0x33), SZ_512K, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25U40356", SNOR_ID(0xc2, 0x25, 0x33), SZ_512K, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit7_6_104_133mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25R4035F", SNOR_ID(0xc2, 0x28, 0x13), SZ_512K, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_HP_MODE | MXIC_F_SCUR_P_E_FAIL_IND |
				    MXIC_F_OTP_2X512B_LAST_ESN_16B | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25rxf_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	/**********************************************************************/

	SNOR_PART("MX25*80**", SNOR_ID(0xc2, 0x20, 0x14), SZ_1M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("MX25L8005", SNOR_ID(0xc2, 0x20, 0x14), SZ_1M,
		  SNOR_ALIAS(&mx25l8005_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("MX25L8006E", SNOR_ID(0xc2, 0x20, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l8006e_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(75), SNOR_DUAL_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("MX25L8035E", SNOR_ID(0xc2, 0x20, 0x14), SZ_1M,
		  SNOR_ALIAS(&mx25l8035e_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 /* | BIT_SPI_MEM_IO_1_4_4 (4PP is too slow) */),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L8073E", SNOR_ID(0xc2, 0x20, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 /* | BIT_SPI_MEM_IO_1_4_4 (4PP is too slow) */),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25V80066", SNOR_ID(0xc2, 0x20, 0x14), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_4bp_up),
	),

	SNOR_PART("MX25V8035F", SNOR_ID(0xc2, 0x23, 0x14), SZ_1M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25U80**", SNOR_ID(0xc2, 0x25, 0x34), SZ_1M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_CHIP_UNPROTECT_98),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 ),
		  SNOR_SPI_MAX_SPEED_MHZ(33),
	),

	SNOR_PART("MX25U8035", SNOR_ID(0xc2, 0x25, 0x34), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(40), SNOR_DUAL_MAX_SPEED_MHZ(40), SNOR_DUAL_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25U8032E", SNOR_ID(0xc2, 0x25, 0x34), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25U8033E", SNOR_ID(0xc2, 0x25, 0x34), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25U8035E", SNOR_ID(0xc2, 0x25, 0x34), SZ_1M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(84), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25U8035F", SNOR_ID(0xc2, 0x25, 0x34), SZ_1M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25U80356", SNOR_ID(0xc2, 0x25, 0x34), SZ_1M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit7_6_104_133mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25V8035", SNOR_ID(0xc2, 0x25, 0x54), SZ_1M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(66), SNOR_DUAL_MAX_SPEED_MHZ(50), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25R8035F", SNOR_ID(0xc2, 0x28, 0x14), SZ_1M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_HP_MODE | MXIC_F_SCUR_P_E_FAIL_IND |
				    MXIC_F_OTP_2X512B_LAST_ESN_16B | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25rxf_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	/**********************************************************************/

	SNOR_PART("MX25*16**", SNOR_ID(0xc2, 0x20, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("MX25L1605", SNOR_ID(0xc2, 0x20, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("MX25L1605A", SNOR_ID(0xc2, 0x20, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(66),
		  SNOR_REGS(&mx25x_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("MX25L1605D", SNOR_ID(0xc2, 0x20, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(66), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L1606E", SNOR_ID(0xc2, 0x20, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(86), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L1608D", SNOR_ID(0xc2, 0x20, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(86), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&mx25x_otp_64b),
	),

	SNOR_PART("MX25L1608E", SNOR_ID(0xc2, 0x20, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(86), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&mx25x_otp_64b),
	),

	SNOR_PART("MX25V16066", SNOR_ID(0xc2, 0x20, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25v16066_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25V1635F", SNOR_ID(0xc2, 0x23, 0x15), SZ_2M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_80mhz_dc_table),
		  SNOR_FIXUPS(&mx25x_wpr_type2_4bp_tb_fixups),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25L16***", SNOR_ID(0xc2, 0x24, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L1633E", SNOR_ID(0xc2, 0x24, 0x15), SZ_2M,
		  SNOR_ALIAS(&mx25l1633e_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(86), SNOR_DUAL_MAX_SPEED_MHZ(75), SNOR_QUAD_MAX_SPEED_MHZ(75),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L1636D", SNOR_ID(0xc2, 0x24, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(66),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L1673E", SNOR_ID(0xc2, 0x24, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(85), SNOR_QUAD_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L1675E", SNOR_ID(0xc2, 0x24, 0x15), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(85), SNOR_QUAD_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L1635E", SNOR_ID(0xc2, 0x25, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(108), SNOR_DUAL_MAX_SPEED_MHZ(80), SNOR_QUAD_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L1636E", SNOR_ID(0xc2, 0x25, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(108), SNOR_QUAD_MAX_SPEED_MHZ(85),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L1655D", SNOR_ID(0xc2, 0x26, 0x15), SZ_2M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B | MXIC_F_CHIP_UNPROTECT_F3),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(66),
	),

	SNOR_PART("MX25U163*F", SNOR_ID(0xc2, 0x25, 0x35), SZ_2M, /* SFDP 1.6 */
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_OP),
	),

	SNOR_PART("MX25U1632F", SNOR_ID(0xc2, 0x25, 0x35), SZ_2M, /* SFDP 1.6 */
		  SNOR_ALIAS(&mx25u1632f_alias),
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u1632f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25U1633F", SNOR_ID(0xc2, 0x25, 0x35), SZ_2M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_80mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25U1635E", SNOR_ID(0xc2, 0x25, 0x35), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(84), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25U1635F", SNOR_ID(0xc2, 0x25, 0x35), SZ_2M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(84), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25R1635F", SNOR_ID(0xc2, 0x28, 0x15), SZ_2M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_HP_MODE | MXIC_F_SCUR_P_E_FAIL_IND |
				    MXIC_F_OTP_2X512B_LAST_ESN_16B | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25rxf_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_80mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	/**********************************************************************/

	SNOR_PART("MX25L32**", SNOR_ID(0xc2, 0x20, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_CHIP_UNPROTECT_98),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("MX25L3205", SNOR_ID(0xc2, 0x20, 0x16), SZ_4M,
		  SNOR_ALIAS(&mx25l3205_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_3bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_up),
	),

	SNOR_PART("MX25L3205D", SNOR_ID(0xc2, 0x20, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(66), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L3206E", SNOR_ID(0xc2, 0x20, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(86), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25L3208D", SNOR_ID(0xc2, 0x20, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(86), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&mx25x_otp_64b),
	),

	SNOR_PART("MX25L3208E", SNOR_ID(0xc2, 0x20, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(86), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
		  SNOR_OTP_INFO(&mx25x_otp_64b),
	),

	SNOR_PART("MX25L3233F", SNOR_ID(0xc2, 0x20, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l3233f_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_OTP_INFO(&mx25x_otp_512b),
		  SNOR_DC_INFO(&mxic_cr1_bit6_104_133mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25L32356", SNOR_ID(0xc2, 0x20, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				      MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(120), SNOR_QUAD_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_80_120mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25L32366", SNOR_ID(0xc2, 0x20, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(120), SNOR_QUAD_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_80_120mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25L3235E", SNOR_ID(0xc2, 0x20, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(86), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_OTP_INFO(&mx25x_otp_512b),
		  SNOR_DC_INFO(&mxic_cr1_bit7_86_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7),
	),

	SNOR_PART("MX25L3236F", SNOR_ID(0xc2, 0x20, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK),
		  SNOR_QE_SR1_BIT6,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_OTP_INFO(&mx25x_otp_512b),
		  SNOR_DC_INFO(&mxic_cr1_bit6_104_133mhz_dc_table),
		  SNOR_FIXUPS(&mx25x_wpr_type2_4bp_tb_fixups),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25L3273E", SNOR_ID(0xc2, 0x20, 0x16), SZ_4M, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l3273e_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7 |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(86), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit7_86_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7),
	),

	SNOR_PART("MX25U32***", SNOR_ID(0xc2, 0x25, 0x36), SZ_4M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_CHIP_UNPROTECT_98),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_FIXUPS(&mx25u32xx_fixups),
	),

	SNOR_PART("MX25L3239E", SNOR_ID(0xc2, 0x25, 0x36), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_WP_RANGES(&wpr_4bp_up),
		  SNOR_DC_INFO(&mxic_cr1_bit7_86_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7),
	),

	SNOR_PART("MX25U3232F", SNOR_ID(0xc2, 0x25, 0x36), SZ_4M, /* SFDP 1.6 */
		  SNOR_ALIAS(&mx25u3232f_alias),
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u1632f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25U3235E", SNOR_ID(0xc2, 0x25, 0x36), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(84), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25U3235F", SNOR_ID(0xc2, 0x25, 0x36), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(84), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25U3273F", SNOR_ID(0xc2, 0x25, 0x36), SZ_4M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_80mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25R3235F", SNOR_ID(0xc2, 0x28, 0x16), SZ_4M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_HP_MODE | MXIC_F_SCUR_P_E_FAIL_IND |
				    MXIC_F_OTP_2X512B_LAST_ESN_16B | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25rxf_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_80mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25L32**D", SNOR_ID(0xc2, 0x5e, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40), SNOR_DUAL_MAX_SPEED_MHZ(33), SNOR_QUAD_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25L3225D", SNOR_ID(0xc2, 0x5e, 0x16), SZ_4M,
		  SNOR_ALIAS(&mx25l3225d_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 /* | BIT_SPI_MEM_IO_1_4_4 (4PP is too slow) */),
		  SNOR_SPI_MAX_SPEED_MHZ(66),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25L3236D", SNOR_ID(0xc2, 0x5e, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(75), SNOR_QUAD_MAX_SPEED_MHZ(75),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25L3237D", SNOR_ID(0xc2, 0x5e, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 /* | BIT_SPI_MEM_IO_1_4_4 (4PP is too slow) */),
		  SNOR_SPI_MAX_SPEED_MHZ(40), SNOR_DUAL_MAX_SPEED_MHZ(33), SNOR_QUAD_MAX_SPEED_MHZ(33),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25L3255E", SNOR_ID(0xc2, 0x9e, 0x16), SZ_4M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(86), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_WP_RANGES(&wpr_4bp_up),
		  SNOR_DC_INFO(&mxic_cr1_bit7_86_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7),
	),

	/**********************************************************************/

	SNOR_PART("MX25L64**", SNOR_ID(0xc2, 0x20, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_CHIP_UNPROTECT_98),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("MX25L6405", SNOR_ID(0xc2, 0x20, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_4bp_up),
	),

	SNOR_PART("MX25L6405D", SNOR_ID(0xc2, 0x20, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(66), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
	),

	SNOR_PART("MX25L6406E", SNOR_ID(0xc2, 0x20, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(86), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25L6408D", SNOR_ID(0xc2, 0x20, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(86), SNOR_DUAL_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_3bp_tb),
		  SNOR_OTP_INFO(&mx25x_otp_64b),
	),

	SNOR_PART("MX25L6408E", SNOR_ID(0xc2, 0x20, 0x17), SZ_8M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(86), SNOR_DUAL_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
		  SNOR_OTP_INFO(&mx25x_otp_64b),
	),

	SNOR_PART("MX25L6433F", SNOR_ID(0xc2, 0x20, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l6433f_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_80_133mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25L64356", SNOR_ID(0xc2, 0x20, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l64356_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				     MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_DUAL_MAX_SPEED_MHZ(120), SNOR_QUAD_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_80_120mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX25L6435E", SNOR_ID(0xc2, 0x20, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l6435e_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7 |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(86), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit7_86_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7),
	),

	SNOR_PART("MX25L6436E", SNOR_ID(0xc2, 0x20, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l6436e_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 /* | BIT_SPI_MEM_IO_1_4_4 (4PP is too slow) */),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type3_4bp_tb0),
	),

	SNOR_PART("MX25L6436F", SNOR_ID(0xc2, 0x20, 0x17), SZ_8M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B),
		  SNOR_QE_SR1_BIT6,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_80_133mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
		  SNOR_FIXUPS(&mx25x_wpr_type2_4bp_tb_fixups),
	),

	SNOR_PART("MX25U64***", SNOR_ID(0xc2, 0x25, 0x37), SZ_8M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_CHIP_UNPROTECT_98),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
		  SNOR_FIXUPS(&mx25u64xx_fixups),
	),

	SNOR_PART("MX25L6439E", SNOR_ID(0xc2, 0x25, 0x37), SZ_8M, /* SFDP 1.0, DTR */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_WP_RANGES(&wpr_4bp_up),
		  SNOR_DC_INFO(&mxic_cr1_bit7_86_104mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7),
	),

	SNOR_PART("MX25U6432F", SNOR_ID(0xc2, 0x25, 0x37), SZ_8M, /* SFDP 1.6 */
		  SNOR_ALIAS(&mx25u6432f_alias),
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u1632f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25U6433F", SNOR_ID(0xc2, 0x25, 0x37), SZ_8M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
	),

	SNOR_PART("MX25U6435E", SNOR_ID(0xc2, 0x25, 0x37), SZ_8M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4 |
				    BIT_SPI_MEM_IO_4_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(84), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25U6435F", SNOR_ID(0xc2, 0x25, 0x37), SZ_8M, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25u6435f_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(84), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type2_4bp_tb0),
	),

	SNOR_PART("MX25L6455E", SNOR_ID(0xc2, 0x26, 0x17), SZ_8M, /* DTR */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 /* | BIT_SPI_MEM_IO_1_4_4 (4PP is too slow) */),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_4bp_up),
	),

	SNOR_PART("MX25R6435F", SNOR_ID(0xc2, 0x28, 0x17), SZ_8M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_HP_MODE | MXIC_F_SCUR_P_E_FAIL_IND |
				    MXIC_F_OTP_2X512B_LAST_ESN_16B | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(80),
		  SNOR_REGS(&mx25rxf_regs),
		  SNOR_DC_INFO(&mxic_cr1_bit6_all_80mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit6),
	),

	SNOR_PART("MX77L6450F", SNOR_ID(0xc2, 0x75, 0x17), SZ_8M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(84), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&mx25x_regs),
	),

	/**********************************************************************/

	SNOR_PART("MX25L128***", SNOR_ID(0xc2, 0x20, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_CHIP_UNPROTECT_98),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("MX25L12805D", SNOR_ID(0xc2, 0x20, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_64B_ESN_16B),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(50),
		  SNOR_REGS(&mx25x_4bp_regs),
		  SNOR_WP_RANGES(&wpr_4bp_up),
	),

	SNOR_PART("MX25L12833F", SNOR_ID(0xc2, 0x20, 0x18), SZ_16M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12833f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L128356", SNOR_ID(0xc2, 0x20, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l128356_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12833f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L12835E", SNOR_ID(0xc2, 0x20, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type3_4bp_tb0),
	),

	SNOR_PART("MX25L12835F", SNOR_ID(0xc2, 0x20, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l12835f_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7 |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12835f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L12836E", SNOR_ID(0xc2, 0x20, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_ALIAS(&mx25l12836e_alias),
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 /* | BIT_SPI_MEM_IO_1_4_4 (4PP is too slow) */),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type3_4bp_tb0),
	),

	SNOR_PART("MX25L12839F", SNOR_ID(0xc2, 0x20, 0x18), SZ_16M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12839f_dc_table),
		  SNOR_FIXUPS(&mx25x_wpr_type2_4bp_tb_fixups),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L12845E", SNOR_ID(0xc2, 0x20, 0x18), SZ_16M, /* SFDP 1.0, DTR */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 /* | BIT_SPI_MEM_IO_1_4_4 (4PP is too slow) */),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type3_4bp_tb0),
	),

	SNOR_PART("MX25L12845G", SNOR_ID(0xc2, 0x20, 0x18), SZ_16M, /* SFDP 1.6 */
		  SNOR_ALIAS(&mx25l12845g_alias), /* MX25L12873G: Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12845g_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L12850F", SNOR_ID(0xc2, 0x20, 0x18), SZ_16M, /* SFDP 1.5 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(84), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&mx25x_regs),
	),

	SNOR_PART("MX25L12872F", SNOR_ID(0xc2, 0x20, 0x18), SZ_16M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133), SNOR_QUAD_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12833f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25U128***", SNOR_ID(0xc2, 0x25, 0x38), SZ_16M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_64K | SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_CHIP_UNPROTECT_98),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(40),
	),

	SNOR_PART("MX25U12832F", SNOR_ID(0xc2, 0x25, 0x38), SZ_16M, /* SFDP 1.6 */
		  SNOR_ALIAS(&mx25u12832f_alias),
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12835f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25U12835F", SNOR_ID(0xc2, 0x25, 0x38), SZ_16M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(84), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_WP_RANGES(&wpr_4bp_up),
		  SNOR_DC_INFO(&mxic_cr1_bit7_104_133mhz_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7),
	),

	SNOR_PART("MX25U12873F", SNOR_ID(0xc2, 0x25, 0x38), SZ_16M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7 |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(84), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&mx25x_regs),
	),

	SNOR_PART("MX25U12843G", SNOR_ID(0xc2, 0x25, 0x38), SZ_16M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12843g_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L12855*", SNOR_ID(0xc2, 0x26, 0x18), SZ_16M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
	),

	SNOR_PART("MX25L12855E", SNOR_ID(0xc2, 0x26, 0x18), SZ_16M, /* DTR */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_2_2 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 /* | BIT_SPI_MEM_IO_1_4_4 (4PP is too slow) */),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&wpr_4bp_up),
	),

	SNOR_PART("MX25L12855F", SNOR_ID(0xc2, 0x26, 0x18), SZ_16M, /* DTR */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7 |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12835f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX77L12850F", SNOR_ID(0xc2, 0x75, 0x18), SZ_16M, /* SFDP 1.6 */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(84), SNOR_QUAD_MAX_SPEED_MHZ(84),
		  SNOR_REGS(&mx25x_regs),
	),

	/**********************************************************************/

	SNOR_PART("MX25L25[67]***", SNOR_ID(0xc2, 0x20, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_CHIP_UNPROTECT_98),
		  SNOR_QE_SR1_BIT6,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1),
		  SNOR_SPI_MAX_SPEED_MHZ(60),
	),

	SNOR_PART("MX25L25633F", SNOR_ID(0xc2, 0x20, 0x19), SZ_32M, /* SFDP 1.6 */
		  SNOR_ALIAS(&mx25l25633f_alias), /* MX25L25672F, MX25L25733F */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l25633f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L25635E", SNOR_ID(0xc2, 0x20, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 /* | BIT_SPI_MEM_IO_1_4_4 (4PP is too slow) */),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type3_4bp_tb0),
	),

	SNOR_PART("MX25L25635F", SNOR_ID(0xc2, 0x20, 0x19), SZ_32M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7 |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H | SNOR_4B_F_EAR | SNOR_4B_F_OPCODE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12835f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L25639F", SNOR_ID(0xc2, 0x20, 0x19), SZ_32M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7 |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H | SNOR_4B_F_EAR | SNOR_4B_F_OPCODE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12839f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L25645G", SNOR_ID(0xc2, 0x20, 0x19), SZ_32M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_ALIAS(&mx25l25645g_alias), /* MX25L25673G, MX25L25745G, MX25L25773G */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(120),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12845g_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L25735E", SNOR_ID(0xc2, 0x20, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_4B_FLAGS(SNOR_4B_F_ALWAYS),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 /* | BIT_SPI_MEM_IO_1_4_4 (4PP is too slow) */),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type3_4bp_tb0),
	),

	SNOR_PART("MX25L25735F", SNOR_ID(0xc2, 0x20, 0x19), SZ_32M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7 |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_4B_FLAGS(SNOR_4B_F_ALWAYS),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12835f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25U256***", SNOR_ID(0xc2, 0x25, 0x39), SZ_32M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_CHIP_UNPROTECT_98),
		  SNOR_QE_SR1_BIT6,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(60),
	),

	SNOR_PART("MX25U25635F", SNOR_ID(0xc2, 0x25, 0x39), SZ_32M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H | SNOR_4B_F_EAR | SNOR_4B_F_OPCODE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(108),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_WP_RANGES(&wpr_4bp_up),
		  SNOR_DC_INFO(&mx25u25635f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25U25643G", SNOR_ID(0xc2, 0x25, 0x39), SZ_32M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_ALIAS(&mx25u25643g_alias), /* MX25U25672G */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(120), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l25643g_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25U25645G", SNOR_ID(0xc2, 0x25, 0x39), SZ_32M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_ALIAS(&mx25u25645g_alias), /* MX25U25673G */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_QUAD_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u25645g_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L25655*", SNOR_ID(0xc2, 0x26, 0x19), SZ_32M,
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_NO_OP),
	),

	SNOR_PART("MX25L25655E", SNOR_ID(0xc2, 0x26, 0x19), SZ_32M, /* DTR */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7),
		  SNOR_QE_SR1_BIT6,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 /* | BIT_SPI_MEM_IO_1_4_4 (4PP is too slow) */),
		  SNOR_SPI_MAX_SPEED_MHZ(104), SNOR_DUAL_MAX_SPEED_MHZ(70), SNOR_QUAD_MAX_SPEED_MHZ(70),
		  SNOR_REGS(&mx25x_4bp_qe_regs),
		  SNOR_WP_RANGES(&mx25x_wpr_type3_4bp_tb0),
	),

	SNOR_PART("MX25L25655F", SNOR_ID(0xc2, 0x26, 0x19), SZ_32M, /* SFDP 1.0, DTR */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7 |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H | SNOR_4B_F_EAR | SNOR_4B_F_OPCODE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12835f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25UM25645G", SNOR_ID(0xc2, 0x80, 0x39), SZ_32M, /* SFDP 1.8, Preamble Bit, DTR, Octal */
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25xm_octal_regs),
		  SNOR_DC_INFO(&mxix_octal_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr2_300),
	),

	SNOR_PART("MX25UM25345G", SNOR_ID(0xc2, 0x83, 0x39), SZ_32M, /* SFDP 1.8, Preamble Bit, DTR, Octal */
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25xm_octal_regs),
		  SNOR_DC_INFO(&mxix_octal_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr2_300),
	),

	SNOR_PART("MX25LM25645G", SNOR_ID(0xc2, 0x85, 0x39), SZ_32M, /* SFDP 1.8, Preamble Bit, DTR, Octal */
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25xm_octal_regs),
		  SNOR_DC_INFO(&mxix_octal_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr2_300),
	),

	SNOR_PART("MX25U25645G-54", SNOR_ID(0xc2, 0x95, 0x39), SZ_32M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_QUAD_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u25645g54_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	/**********************************************************************/

	SNOR_PART("MX25L512**G", SNOR_ID(0xc2, 0x20, 0x1a), SZ_64M,
		  SNOR_FLAGS(SNOR_F_META | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_WPSEL_SCUR_BIT7 |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_4_4),
		  SNOR_SPI_MAX_SPEED_MHZ(60),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_dc7_6_dfl_table),
		  SNOR_FIXUPS(&mx66l512xxx_fixups),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L51237G", SNOR_ID(0xc2, 0x20, 0x1a), SZ_64M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l51237g_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L51245G", SNOR_ID(0xc2, 0x20, 0x1a), SZ_64M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_ALIAS(&mx25l51245g_alias), /* MX25L51273G */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u25645g_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25L51245J", SNOR_ID(0xc2, 0x20, 0x1a), SZ_64M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(120), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l51245j_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX66L51235F", SNOR_ID(0xc2, 0x20, 0x1a), SZ_64M, /* SFDP 1.0 */
		  SNOR_FLAGS(SNOR_F_NO_SFDP | SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K |
			     SNOR_F_SR_NON_VOLATILE),
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B | MXIC_F_WPSEL_SCUR_BIT7 |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_QE_SR1_BIT6, SNOR_QPI_35H_F5H,
		  SNOR_4B_FLAGS(SNOR_4B_F_B7H_E9H | SNOR_4B_F_EAR | SNOR_4B_F_OPCODE),
		  SNOR_SOFT_RESET_FLAGS(SNOR_SOFT_RESET_OPCODE_66H_99H),
		  SNOR_READ_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_QPI),
		  SNOR_PP_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X4),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l12835f_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25U51245G", SNOR_ID(0xc2, 0x25, 0x3a), SZ_64M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_ALIAS(&mx25u51245g_alias), /* MX25U51293G */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_QUAD_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u25645g_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX25UM51245G", SNOR_ID(0xc2, 0x80, 0x3a), SZ_64M, /* SFDP 1.8, Preamble Bit, DTR, Octal */
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25xm_octal_regs),
		  SNOR_DC_INFO(&mxix_octal_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr2_300),
	),

	SNOR_PART("MX25UM51345G", SNOR_ID(0xc2, 0x81, 0x3a), SZ_64M, /* SFDP 1.8, Preamble Bit, DTR, Octal */
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25xm_octal_regs),
		  SNOR_DC_INFO(&mxix_octal_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr2_300),
	),

	SNOR_PART("MX25LM51245G", SNOR_ID(0xc2, 0x85, 0x3a), SZ_64M, /* SFDP 1.8, Preamble Bit, DTR, Octal */
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25xm_octal_regs),
		  SNOR_DC_INFO(&mxix_octal_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr2_300),
	),

	SNOR_PART("MX25U51245G-54", SNOR_ID(0xc2, 0x95, 0x3a), SZ_64M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_QUAD_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u25645g54_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	/**********************************************************************/

	SNOR_PART("MX66L1G45*", SNOR_ID(0xc2, 0x20, 0x1b), SZ_128M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_FLAGS(SNOR_F_META),
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_WPSEL_SCUR_BIT7 |
				    MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(60),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_dc7_6_dfl_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX66L1G45G", SNOR_ID(0xc2, 0x20, 0x1b), SZ_128M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_512B_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_QUAD_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u25645g_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX66L1G45J", SNOR_ID(0xc2, 0x20, 0x1b), SZ_128M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(120), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l51245j_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX66U1G45G", SNOR_ID(0xc2, 0x25, 0x3b), SZ_128M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_ALIAS(&mx25u1g45g_alias), /* MX66U1G93G */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_QUAD_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u25645g_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX66UM1G45G", SNOR_ID(0xc2, 0x80, 0x3b), SZ_128M, /* SFDP 1.8, Preamble Bit, DTR, Octal */
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(166),
		  SNOR_REGS(&mx25xm_octal_regs),
		  SNOR_DC_INFO(&mxix_octal_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr2_300),
	),

	SNOR_PART("MX66LM1G45G", SNOR_ID(0xc2, 0x85, 0x3b), SZ_128M, /* SFDP 1.8, Preamble Bit, DTR, Octal */
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25xm_octal_regs),
		  SNOR_DC_INFO(&mxix_octal_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr2_300),
	),

	SNOR_PART("MX66U1G45G-54", SNOR_ID(0xc2, 0x95, 0x3b), SZ_128M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_QUAD_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u25645g54_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	/**********************************************************************/

	SNOR_PART("MX66L2G45*", SNOR_ID(0xc2, 0x20, 0x1c), SZ_256M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_FLAGS(SNOR_F_META),
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(60),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mxic_cr1_dc7_6_dfl_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX66L2G45G", SNOR_ID(0xc2, 0x20, 0x1c), SZ_256M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u25645g_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX66L2G45J", SNOR_ID(0xc2, 0x20, 0x1c), SZ_256M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(120), SNOR_DUAL_MAX_SPEED_MHZ(104), SNOR_QUAD_MAX_SPEED_MHZ(104),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25l51245j_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX66U2G45G", SNOR_ID(0xc2, 0x25, 0x3c), SZ_256M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_QUAD_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u25645g_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),

	SNOR_PART("MX66UM2G45G", SNOR_ID(0xc2, 0x80, 0x3c), SZ_256M, /* SFDP 1.8, Preamble Bit, DTR, Octal */
		  SNOR_VENDOR_FLAGS(MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25xm_octal_regs),
		  SNOR_DC_INFO(&mxix_octal_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr2_300),
	),

	SNOR_PART("MX66U2G45G-54", SNOR_ID(0xc2, 0x95, 0x3c), SZ_256M, /* SFDP 1.6, Preamble Bit, DTR */
		  SNOR_VENDOR_FLAGS(MXIC_F_PP_1_4_4 | MXIC_F_SCUR_P_E_FAIL_IND | MXIC_F_OTP_2X512B_LAST_ESN_16B |
				    MXIC_F_WPSEL_SCUR_BIT7 | MXIC_F_WPR_4BP_TB_OTP),
		  SNOR_SPI_MAX_SPEED_MHZ(166), SNOR_QUAD_MAX_SPEED_MHZ(133),
		  SNOR_REGS(&mx25x_regs),
		  SNOR_DC_INFO(&mx25u25645g54_dc_table),
		  SNOR_DC_CHIP_SETUP_ACC_INFO(&mxic_dc_acc_cr1_bit7_6),
	),
};

static ufprog_status mx25l2026c_write_enable(struct spi_nor *snor)
{
	/* Special Unprotection */
	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &sr_acc, 0x80, 0, false));
	STATUS_CHECK_RET(spi_nor_issue_single_opcode(snor, SNOR_CMD_MXIC_KEY1));
	STATUS_CHECK_RET(spi_nor_issue_single_opcode(snor, SNOR_CMD_MXIC_KEY2));
	STATUS_CHECK_RET(spi_nor_issue_single_opcode(snor, SNOR_CMD_MXIC_KEY1));
	STATUS_CHECK_RET(spi_nor_issue_single_opcode(snor, SNOR_CMD_MXIC_KEY2));
	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &sr_acc, 0xfc, 0, false));

	return spi_nor_write_enable(snor);
}

static ufprog_status scur_otp_single_esn_full_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{
	uint32_t reg;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &scur_acc, &reg));

	if (reg & MXIC_SCUR_FLDO) {
		*retlocked = true;
		return UFP_OK;
	}

	if (reg & MXIC_SCUR_LDSO)
		*retlocked = true;
	else
		*retlocked = false;

	return UFP_OK;
}

static const struct spi_nor_flash_part_otp_ops scur_otp_single_esn_full_lock_ops = {
	.read = scur_otp_read,
	.write = scur_otp_write,
	.lock = scur_otp_lock,
	.locked = scur_otp_single_esn_full_locked,
};

static ufprog_status scur_otp_2x512b_esn_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{
	uint32_t reg;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &scur_acc, &reg));

	if (reg & MXIC_SCUR_FLDO) {
		if (index == 1) {
			*retlocked = true;
			return UFP_OK;
		}
	}

	if (reg & MXIC_SCUR_LDSO)
		*retlocked = true;
	else
		*retlocked = false;

	return UFP_OK;
}

static const struct spi_nor_flash_part_otp_ops scur_otp_2x512b_esn_ops = {
	.read = scur_otp_read,
	.write = scur_otp_write,
	.lock = scur_otp_lock,
	.locked = scur_otp_2x512b_esn_locked,
};

static ufprog_status macronix_part_fixup(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp)
{
	uint32_t i, scur, regval;

	spi_nor_blank_part_fill_default_opcodes(bp);

	if (bp->p.vendor_flags & MXIC_F_PP_1_4_4)
		bp->p.pp_io_caps |= BIT_SPI_MEM_IO_1_4_4;

	if (bp->p.pp_io_caps & BIT_SPI_MEM_IO_1_4_4) {
		bp->pp_opcodes_3b[SPI_MEM_IO_1_4_4].opcode = SNOR_CMD_PAGE_PROG_QUAD_IO;
		bp->pp_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = 0;

		if (bp->p.size >= SZ_32M) {
			bp->pp_opcodes_4b[SPI_MEM_IO_1_4_4].opcode = SNOR_CMD_4B_PAGE_PROG_QUAD_IO;
			bp->pp_opcodes_4b[SPI_MEM_IO_1_4_4].ndummy = bp->pp_opcodes_4b[SPI_MEM_IO_1_4_4].nmode = 0;
		}
	}

	if (snor->sfdp.bfpt && snor->sfdp.bfpt_hdr->minor_ver >= SFDP_REV_MINOR_A) {
		if (bp->p.read_io_caps & BIT_SPI_MEM_IO_4_4_4) {
			bp->p.pp_io_caps |= BIT_SPI_MEM_IO_4_4_4;
			bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].opcode = SNOR_CMD_PAGE_PROG;
			bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;

			if (bp->p.size >= SZ_32M) {
				bp->pp_opcodes_4b[SPI_MEM_IO_4_4_4].opcode = SNOR_CMD_4B_PAGE_PROG;
				bp->pp_opcodes_4b[SPI_MEM_IO_4_4_4].ndummy = bp->pp_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = 0;
			}
		}
	}
	if (bp->p.size >= SZ_32M && !snor->sfdp.a4bit) {
		for (i = 0; i < ARRAY_SIZE(bp->erase_info_4b.info); i++) {
			if (!bp->erase_info_4b.info[i].opcode) {
				bp->erase_info_4b.info[i].opcode = SNOR_CMD_4B_SECTOR_ERASE_32K;
				bp->erase_info_4b.info[i].size = SZ_32K;
				break;
			}
		}
	}

	if (bp->p.vendor_flags & MXIC_F_OTP_512B_ESN_16B) {
		STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &scur_acc, &scur));

		if (scur & MXIC_SCUR_FLDO)
			bp->p.otp = &mx25x_otp_496b;
		else
			bp->p.otp = &mx25x_otp_512b;
	} else if (bp->p.vendor_flags & MXIC_F_OTP_64B_ESN_16B) {
		STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &scur_acc, &scur));

		if (scur & MXIC_SCUR_FLDO)
			bp->p.otp = &mx25x_otp_48b;
		else
			bp->p.otp = &mx25x_otp_64b;
	} else if (bp->p.vendor_flags & MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK) {
		snor->ext_param.ops.otp = &scur_otp_single_esn_full_lock_ops;
	} else if (bp->p.vendor_flags & MXIC_F_OTP_2X512B_LAST_ESN_16B) {
		snor->ext_param.ops.otp = &scur_otp_2x512b_esn_ops;
		bp->p.otp = &mx25rxf_otp_2x512b;
	}

	if (bp->p.vendor_flags & MXIC_F_WPR_4BP_TB_OTP) {
		STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &mx25rxf_srcr_acc, &regval));

		if (regval & MXIC_TB_BIT)
			bp->p.wp_ranges = &wpr_4bp_lo;
		else
			bp->p.wp_ranges = &wpr_4bp_up;
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_fixup macronix_fixups = {
	.pre_param_setup = macronix_part_fixup,
};

static ufprog_status mxic_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	uint32_t scur;

	if (snor->param.vendor_flags & (MXIC_F_OTP_64B_ESN_16B | MXIC_F_OTP_512B_ESN_16B |
					MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK | MXIC_F_OTP_2X512B_LAST_ESN_16B)) {
		STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &scur_acc, &scur));

		if (!(scur & MXIC_SCUR_FLDO))
			return UFP_UNSUPPORTED;

		if (retlen)
			*retlen = MXIC_UID_LEN;

		if (!data)
			return UFP_OK;

		if (snor->param.vendor_flags & (MXIC_F_OTP_64B_ESN_16B | MXIC_F_OTP_512B_ESN_16B |
						MXIC_F_OTP_SINGLE_ESN_16B_FULL_LOCK))
			return scur_otp_read_cust(snor, 0, MXIC_UID_LEN, data, false);

		return scur_otp_read_cust(snor, (snor->ext_param.otp->count - 1) * snor->ext_param.otp->size,
					  MXIC_UID_LEN, data, false);
	}

	return UFP_UNSUPPORTED;
}

static ufprog_status macronix_chip_setup(struct spi_nor *snor)
{
	uint32_t regval;

	if (snor->param.vendor_flags & MXIC_F_HP_MODE)
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &mx25rxf_srcr_acc, 0, MXIC_HP_MODE_BIT, false));

	if (snor->param.vendor_flags & MXIC_F_WPSEL_SCUR_BIT7) {
		/* Write-protect selection */
		STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &scur_acc, &regval));

		if (snor->param.vendor_flags & MXIC_F_WPSEL_SCUR_BIT7) {
			if (regval & BIT(7))
				snor->state.flags |= SNOR_F_GLOBAL_UNLOCK;
			else
				snor->state.flags &= ~SNOR_F_GLOBAL_UNLOCK;
		}
	}

	if (snor->param.vendor_flags & MXIC_F_CHIP_UNPROTECT_F3) {
		STATUS_CHECK_RET(spi_nor_write_enable(snor));
		STATUS_CHECK_RET(spi_nor_issue_single_opcode(snor, SNOR_CMD_MXIC_CHIP_UNPROTECT));
		STATUS_CHECK_RET(spi_nor_write_disable(snor));
	}

	if (snor->param.vendor_flags & MXIC_F_CHIP_UNPROTECT_98) {
		STATUS_CHECK_RET(spi_nor_write_enable(snor));
		STATUS_CHECK_RET(spi_nor_issue_single_opcode(snor, SNOR_CMD_GLOBAL_BLOCK_UNLOCK));
		STATUS_CHECK_RET(spi_nor_write_disable(snor));
	}

	if (snor->ext_param.dc_setup_acc == &mxic_dc_acc_cr2_300) {
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &mxic_cr2_0_acc, 0xff, 0, true));
		STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &mxic_cr2_200_acc, 0xff, 0, true));
	}

	return UFP_OK;
}

static const struct spi_nor_flash_part_ops macronix_default_part_ops = {
	.otp = &scur_otp_ops,

	.chip_setup = macronix_chip_setup,
	.read_uid = mxic_read_uid,
	.qpi_dis = spi_nor_disable_qpi_f5h,
};

const struct spi_nor_vendor vendor_macronix = {
	.mfr_id = SNOR_VENDOR_MACRONIX,
	.id = "macronix",
	.name = "Macronix",
	.parts = macronix_parts,
	.nparts = ARRAY_SIZE(macronix_parts),
	.vendor_flag_names = macronix_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(macronix_vendor_flag_info),
	.default_part_ops = &macronix_default_part_ops,
	.default_part_fixups = &macronix_fixups,
};
