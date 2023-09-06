// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Macronix SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include <ufprog/bits.h>
#include <ufprog/log.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"

/* Macronix read ECC status command */
#define SNAND_CMD_MACRONIX_READ_ECC_SR			0x7c
#define MACRONIX_ECC_SR_CURR_MASK			0x0f

/* Macronix feature address */
#define SPI_NAND_FEATURE_MACRONIX_CFG10_ADDR		0x10
#define MACRONIX_OTP_ENPGM				BIT(0)

#define SPI_NAND_FEATURE_MACRONIX_CFG60_ADDR		0x60
#define MACRONIX_SPI_NOR_EN				BIT(1)

#define SPI_NAND_FEATURE_MACRONIX_CFGE0_ADDR		0xe0
#define MACRONIX_DC_EN					BIT(2)

/* Macronix configuration bits */
#define SPI_NAND_CONFIG_MACRONIX_CONTINUOUS_READ	BIT(2)

/* Macronix status bits */
#define MXIC_SR_CRBSY					BIT(6)

/* Macronix vendor flags */
#define MXIC_F_DC_BIT					BIT(0)

static const struct spi_nand_part_flag_enum_info macronix_vendor_flag_info[] = {
	{ 0, "dc-bit" },
};

static struct nand_otp_info mxic_otp = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 30,
};

static const struct nand_page_layout mxic_ecc_2k_4bit_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
);

static const struct nand_page_layout mxic_ecc_2k_8bit_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_PARITY_BYTES(64),
);

static const struct nand_page_layout mxic_ecc_4k_8bit_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(4096),
	ECC_PAGE_MARKER_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(2),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_PARITY_BYTES(128),
);

static const struct spi_nand_flash_part maxronix_parts[] = {
	SNAND_PART("MX35LF1GE4AB", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0x12), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_SEQ | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&mxic_ecc_2k_4bit_layout),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35LF1G24AD", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0x14, 0x03), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_UNSUPPORTED, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(120), SNAND_DUAL_MAX_SPEED_MHZ(108), SNAND_QUAD_MAX_SPEED_MHZ(108),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35UF1GE4AC", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0x92, 0x01), &snand_memorg_1g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_CONTINUOUS_READ | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35UF1G14AC", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0x90), &snand_memorg_1g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_UNSUPPORTED, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35UF1GE4AD", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0x96, 0x03), &snand_memorg_1g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_CONTINUOUS_READ | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&mxic_ecc_2k_8bit_layout),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35UF1G24AD", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0x94, 0x03), &snand_memorg_1g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_BBM_2ND_PAGE),
		   SNAND_VENDOR_FLAGS(MXIC_F_DC_BIT),
		   SNAND_QE_CR_BIT0, SNAND_ECC_UNSUPPORTED, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(166),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35LF2GE4AB", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0x22), &snand_memorg_2g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&mxic_ecc_2k_4bit_layout),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35LF2G14AC", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0x20), &snand_memorg_2g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_SEQ | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_UNSUPPORTED, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35LF2G24AD", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0x24, 0x03), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_UNSUPPORTED, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(120), SNAND_DUAL_MAX_SPEED_MHZ(108), SNAND_QUAD_MAX_SPEED_MHZ(108),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35LF2GE4AD", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0x26, 0x03), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_CONTINUOUS_READ | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&mxic_ecc_2k_8bit_layout),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35UF2GE4AC", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0xa2, 0x01), &snand_memorg_2g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_CONTINUOUS_READ | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35UF2G14AC", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0xa0), &snand_memorg_2g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_UNSUPPORTED, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35UF2GE4AD", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0xa6, 0x03), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_CONTINUOUS_READ | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&mxic_ecc_2k_8bit_layout),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35UF2G24AD", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0xa4, 0x03), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_BBM_2ND_PAGE),
		   SNAND_VENDOR_FLAGS(MXIC_F_DC_BIT),
		   SNAND_QE_CR_BIT0, SNAND_ECC_UNSUPPORTED, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(166),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35LF4G24AD", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0x35, 0x03), &snand_memorg_4g_4k_256,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_UNSUPPORTED, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(120), SNAND_DUAL_MAX_SPEED_MHZ(108), SNAND_QUAD_MAX_SPEED_MHZ(108),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35LF4GE4AD", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0x37, 0x03), &snand_memorg_4g_4k_256,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_CONTINUOUS_READ | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&mxic_ecc_4k_8bit_layout),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35UF4GE4AD", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0xb7, 0x03), &snand_memorg_4g_4k_256, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_CONTINUOUS_READ | SNAND_F_BBM_2ND_PAGE),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&mxic_ecc_4k_8bit_layout),
		   NAND_OTP_INFO(&mxic_otp)
	),

	SNAND_PART("MX35UF4G24AD", SNAND_ID(SNAND_ID_DUMMY, 0xc2, 0xb5, 0x03), &snand_memorg_4g_4k_256, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_BBM_2ND_PAGE),
		   SNAND_VENDOR_FLAGS(MXIC_F_DC_BIT),
		   SNAND_QE_CR_BIT0, SNAND_ECC_UNSUPPORTED, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(166),
		   NAND_OTP_INFO(&mxic_otp)
	),
};

static ufprog_status spi_nand_check_ecc_macronix(struct spi_nand *snand)
{
	uint8_t eccst;
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNAND_CMD_MACRONIX_READ_ECC_SR, 1),
		SPI_MEM_OP_NO_ADDR,
		SPI_MEM_OP_DUMMY(1, 1),
		SPI_MEM_OP_DATA_IN(1, &eccst, 1)
	);

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snand->spi, &op));

	eccst &= MACRONIX_ECC_SR_CURR_MASK;

	if (!eccst)
		return UFP_OK;

	if (eccst <= snand->nand.ecc_req.strength_per_step) {
		snand->ecc_status->step_bitflips[0] = eccst;
		return UFP_ECC_CORRECTED;
	}

	snand->ecc_status->step_bitflips[0] = -1;
	return UFP_ECC_UNCORRECTABLE;
}

static ufprog_status macronix_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	uint8_t val;

	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	if (bp->p.flags & SNAND_F_NOR_READ_CAP) {
		STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_MACRONIX_CFG60_ADDR, &val));

		if (val & MACRONIX_SPI_NOR_EN) {
			if (bp->p.rd_opcodes != bp->rd_opcodes) {
				memcpy(&bp->rd_opcodes, bp->p.rd_opcodes, sizeof(bp->rd_opcodes));
				bp->p.rd_opcodes = bp->rd_opcodes;
			}

			bp->rd_opcodes[SPI_MEM_IO_1_1_1].opcode = SNAND_CMD_FAST_READ_FROM_CACHE;
			bp->rd_opcodes[SPI_MEM_IO_1_1_1].naddrs = 3;
			bp->rd_opcodes[SPI_MEM_IO_1_1_1].ndummy = 8;
		}
	}

	if (bp->p.vendor_flags & MXIC_F_DC_BIT) {
		STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_MACRONIX_CFGE0_ADDR, &val));
		val |= MACRONIX_DC_EN;
		STATUS_CHECK_RET(spi_nand_set_feature(snand, SPI_NAND_FEATURE_MACRONIX_CFGE0_ADDR, val));

		STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_MACRONIX_CFGE0_ADDR, &val));

		if (val & MACRONIX_DC_EN) {
			if (bp->p.rd_opcodes != bp->rd_opcodes) {
				memcpy(&bp->rd_opcodes, bp->p.rd_opcodes, sizeof(bp->rd_opcodes));
				bp->p.rd_opcodes = bp->rd_opcodes;
			}

			if (bp->rd_opcodes[SPI_MEM_IO_1_2_2].ndummy)
				bp->rd_opcodes[SPI_MEM_IO_1_2_2].ndummy = 8;

			if (bp->rd_opcodes[SPI_MEM_IO_1_4_4].ndummy)
				bp->rd_opcodes[SPI_MEM_IO_1_4_4].ndummy = 8;
		}
	}

	return UFP_OK;
}

static ufprog_status macronix_nor_read_enable(struct spi_nand *snand)
{
	ufprog_status ret;
	uint8_t val;

	STATUS_CHECK_RET(spi_nand_set_feature(snand, SPI_NAND_FEATURE_MACRONIX_CFG10_ADDR, MACRONIX_OTP_ENPGM));

	ret = spi_nand_set_feature(snand, SPI_NAND_FEATURE_MACRONIX_CFG60_ADDR, MACRONIX_SPI_NOR_EN);
	if (ret) {
		logm_err("Failed to set SPI_NOR_EN bit\n");
		goto cleanup;
	}

	ret = spi_nand_get_feature(snand, SPI_NAND_FEATURE_MACRONIX_CFG60_ADDR, &val);
	if (ret) {
		logm_err("Failed to read SPI_NOR_EN bit\n");
		goto cleanup;
	}

	if (!(val & MACRONIX_SPI_NOR_EN)) {
		logm_err("Unabled to set SPI_NOR_EN bit. Maybe SPI_NO_EN is not supported?\n");
		goto cleanup;
	}

	STATUS_CHECK_GOTO_RET(spi_nand_write_enable(snand), ret, cleanup);

	STATUS_CHECK_GOTO_RET(spi_nand_page_op(snand, 0, SNAND_CMD_PROGRAM_EXECUTE), ret, cleanup);

	ret = spi_nand_wait_busy(snand, SNAND_POLL_MAX_US, NULL);
	if (ret) {
		logm_err("Program SPI_NOR_EN bit timed out\n");
		goto cleanup;
	}

	ret = spi_nand_get_feature(snand, SPI_NAND_FEATURE_MACRONIX_CFG60_ADDR, &val);
	if (ret) {
		logm_err("Failed to read SPI_NOR_EN bit\n");
		goto cleanup;
	}

	if (!(val & MACRONIX_SPI_NOR_EN)) {
		logm_err("Failed to program SPI_NOR_EN bit\n");
		ret = UFP_FAIL;
		goto cleanup;
	}

	ret = UFP_OK;

cleanup:
	spi_nand_write_disable(snand);
	spi_nand_set_feature(snand, SPI_NAND_FEATURE_MACRONIX_CFG60_ADDR, 0);
	spi_nand_set_feature(snand, SPI_NAND_FEATURE_MACRONIX_CFG10_ADDR, 0);

	return ret;
}

static ufprog_status macronix_nor_read_enabled(struct spi_nand *snand, ufprog_bool *retenabled)
{
	uint8_t val;

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_MACRONIX_CFG60_ADDR, &val));

	*retenabled = !!(val & MACRONIX_SPI_NOR_EN);

	return UFP_OK;
}

static ufprog_status macronix_part_set_ops(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	if (bp->p.flags & SNAND_F_NOR_READ_CAP) {
		snand->ext_param.ops.nor_read_enable = macronix_nor_read_enable;
		snand->ext_param.ops.nor_read_enabled = macronix_nor_read_enabled;
	}

	if (bp->p.flags & SNAND_F_READ_CACHE_SEQ) {
		snand->state.seq_rd_feature_addr = SPI_NAND_FEATURE_STATUS_ADDR;
		snand->state.seq_rd_crbsy_mask = MXIC_SR_CRBSY;
	}

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup macronix_fixups = {
	.pre_param_setup = macronix_part_fixup,
	.post_param_setup = macronix_part_set_ops,
};

static ufprog_status macronix_setup_chip(struct spi_nand *snand)
{
	if (snand->param.vendor_flags & SNAND_F_CONTINUOUS_READ)
		STATUS_CHECK_RET(spi_nand_update_config(snand, SPI_NAND_CONFIG_MACRONIX_CONTINUOUS_READ, 0));

	return UFP_OK;
}

static const struct spi_nand_flash_part_ops macronix_part_ops = {
	.chip_setup = macronix_setup_chip,
	.check_ecc = spi_nand_check_ecc_macronix,
};

static ufprog_status macronix_pp_post_init(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bp->p.qe_type = QE_CR_BIT0;
	bp->p.ecc_type = ECC_UNKNOWN;
	bp->p.otp_en_type = OTP_CR_BIT6;

	bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4;
	bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4;

	return UFP_OK;
}

static const struct spi_nand_vendor_ops macronix_ops = {
	.pp_post_init = macronix_pp_post_init,
};

const struct spi_nand_vendor vendor_macronix = {
	.mfr_id = SNAND_VENDOR_MACRONIX,
	.id = "macronix",
	.name = "Macronix",
	.parts = maxronix_parts,
	.nparts = ARRAY_SIZE(maxronix_parts),
	.ops = &macronix_ops,
	.default_part_ops = &macronix_part_ops,
	.default_part_fixups = &macronix_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
	.vendor_flag_names = macronix_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(macronix_vendor_flag_info),
};
