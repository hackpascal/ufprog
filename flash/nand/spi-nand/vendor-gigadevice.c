// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * GigaDevice SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include <ufprog/bits.h>
#include <ufprog/log.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"

/* GigaDevice feature address */
#define SPI_NAND_FEATURE_GD_STATUS2_ADDR		0xf0
#define GD_SR2_CRBSY					BIT(0)
#define GD_SR2_ECCSE_SHIFT				4
#define GD_SR2_ECCSE_MASK				BITS(5, GD_SR2_ECCSE_SHIFT)

/* GigaDevice ECC status bits */
#define GD_SR_ECC_SR_3_BITS_MASK			BITS(6, SPI_NAND_STATUS_ECC_SHIFT)

/* GigaDevice vendor flags */
#define GD_F_ECC_CAP_1_BIT				BIT(0)
#define GD_F_ECC_CAP_4_BITS				BIT(1)
#define GD_F_ECC_CAP_8_BITS_SR_2BITS			BIT(2)
#define GD_F_ECC_CAP_8_BITS_SR_3BITS			BIT(3)
#define GD_F_ECC_CAP_8_BITS_SR2_2BITS			BIT(4)
#define GD_F_PP_OTP_PAGE_4				BIT(5)
#define GD_F_UID_OTP_PAGE_6				BIT(6)

static const struct spi_nand_part_flag_enum_info gd_vendor_flag_info[] = {
	{ 0, "ecc-1-bit" },
	{ 1, "ecc-4-bits" },
	{ 2, "ecc-4-bits-sr-2-bits" },
	{ 3, "ecc-4-bits-sr-3-bits" },
	{ 4, "ecc-4-bits-sr2-2-bits" },
	{ 5, "param-page-otp-page-4" },
	{ 6, "uid-otp-page-6" },
};

static struct nand_otp_info gd_otp = {
	.start_index = 0,
	.count = 4,
};

static struct nand_otp_info gd_otp_10 = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 10,
};

static const struct nand_page_layout gd_ecc4bit_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_FREE_BYTES(3),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_PARITY_BYTES(64),
);

static const struct nand_page_layout gd_ecc8bit_sr_2bits_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_FREE_BYTES(3),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(4),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(4),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(4),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(8),
	ECC_PAGE_PARITY_BYTES(4),
);

static const struct nand_page_layout gd_ecc8bit_sr_3bits_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(63),
	ECC_PAGE_PARITY_BYTES(64),
);

static const struct nand_page_layout gd_4k_ecc8bit_sr_3bits_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(4096),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_DATA_BYTES(127),
	ECC_PAGE_PARITY_BYTES(128),
);

static const struct nand_page_layout gd_ecc8bit_sr2_2bits_no_parity_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_FREE_BYTES(3),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
);

static const struct nand_page_layout gd_ecc8bit_sr2_2bits_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_FREE_BYTES(3),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_PARITY_BYTES(64),
);

static const struct nand_page_layout gd_4k_ecc8bit_sr2_2bits_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(4096),
	ECC_PAGE_MARKER_BYTES(1),
	ECC_PAGE_OOB_FREE_BYTES(3),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_OOB_FREE_BYTES(4),
	ECC_PAGE_OOB_DATA_BYTES(12),
	ECC_PAGE_PARITY_BYTES(128),
);

static const struct spi_nand_io_opcode gd_nor_read_opcodes[__SPI_MEM_IO_MAX] = {
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_1, SNAND_CMD_FAST_READ_FROM_CACHE, 3, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_2, SNAND_CMD_READ_FROM_CACHE_DUAL_OUT, 3, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_2_2, SNAND_CMD_READ_FROM_CACHE_DUAL_IO, 2, 4),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_4, SNAND_CMD_READ_FROM_CACHE_QUAD_OUT, 3, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_4_4, SNAND_CMD_READ_FROM_CACHE_QUAD_IO, 2, 2),
};

static const struct spi_nand_io_opcode gd_rd_opcodes_a8d[__SPI_MEM_IO_MAX] = {
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_1, SNAND_CMD_FAST_READ_FROM_CACHE, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_2, SNAND_CMD_READ_FROM_CACHE_DUAL_OUT, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_2_2, SNAND_CMD_READ_FROM_CACHE_DUAL_IO, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_4, SNAND_CMD_READ_FROM_CACHE_QUAD_OUT, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_4_4, SNAND_CMD_READ_FROM_CACHE_QUAD_IO, 2, 8),
};

static const struct spi_nand_flash_part gigadevice_parts[] = {
	SNAND_PART("GD5F1GQ4UAWxx", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0x10), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 1),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_1_BIT),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&gd_otp_10),
	),

	SNAND_PART("GD5F1GQ4xAYIG", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0xf1), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_2bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F1GQ4UExxH", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0xd9), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr2_2bits_no_parity_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F1GQ4RExxH", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0xc9), &snand_memorg_1g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr2_2bits_no_parity_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F1GQ4UxxIG", SNAND_ID(SNAND_ID_DIRECT, 0xc8, 0xb1, 0x48), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR_3BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_nor_read_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F1GQ4RxxIG", SNAND_ID(SNAND_ID_DIRECT, 0xc8, 0xa1, 0x48), &snand_memorg_1g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR_3BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_nor_read_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F1GQ4UExIG", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0xd1), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr2_2bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F1GQ4RExIG", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0xc1), &snand_memorg_1g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr2_2bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F1GQ5UExxH", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0x31), &snand_memorg_1g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_4_BITS | GD_F_PP_OTP_PAGE_4 | GD_F_UID_OTP_PAGE_6),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr2_2bits_no_parity_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F1GQ5RExxH", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0x21), &snand_memorg_1g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_4_BITS | GD_F_PP_OTP_PAGE_4 | GD_F_UID_OTP_PAGE_6),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr2_2bits_no_parity_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F1GQ5UExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x51), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_4_BITS | GD_F_PP_OTP_PAGE_4 | GD_F_UID_OTP_PAGE_6),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&gd_ecc4bit_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F1GQ5RExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x41), &snand_memorg_1g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_4_BITS | GD_F_PP_OTP_PAGE_4 | GD_F_UID_OTP_PAGE_6),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&gd_ecc4bit_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F1GM7UExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x91), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp_10),
	),

	SNAND_PART("GD5F1GM7RExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x81), &snand_memorg_1g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp_10),
	),

	SNAND_PART("GD5F2GQ4xAYIG", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0xf2), &snand_memorg_2g_2k_64,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_2bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F2GQ5UExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x52), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS | GD_F_PP_OTP_PAGE_4 | GD_F_UID_OTP_PAGE_6),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_rd_opcodes_a8d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr2_2bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F2GQ5RExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x42), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS | GD_F_PP_OTP_PAGE_4 | GD_F_UID_OTP_PAGE_6),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_rd_opcodes_a8d),
		   SNAND_SPI_MAX_SPEED_MHZ(80),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr2_2bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F2GQ4UCxIG", SNAND_ID(SNAND_ID_DIRECT, 0xc8, 0xb2, 0x48), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR_3BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_nor_read_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F2GQ4RCxIG", SNAND_ID(SNAND_ID_DIRECT, 0xc8, 0xa2, 0x48), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR_3BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_nor_read_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	/* Spec identical with GD5F2GQ4UCxIG except for the ID */
	SNAND_PART("GD5F2GQ4UFxIG", SNAND_ID(SNAND_ID_DIRECT, 0xc8, 0xb2), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR_3BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_nor_read_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	/* Spec identical with GD5F2GQ4RCxIG except for the ID */
	SNAND_PART("GD5F2GQ4RFxIG", SNAND_ID(SNAND_ID_DIRECT, 0xc8, 0xa2), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR_3BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_nor_read_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F2GQ4UExIG", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0xd2), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr2_2bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F2GQ4RExIG", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0xc2), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr2_2bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F2GQ5UExxH", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0x32), &snand_memorg_2g_2k_64,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_READ_CACHE_SEQ),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS | GD_F_PP_OTP_PAGE_4 | GD_F_UID_OTP_PAGE_6),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_rd_opcodes_a8d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr2_2bits_no_parity_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F2GQ5RExxH", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0x22), &snand_memorg_2g_2k_64, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_READ_CACHE_SEQ),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS | GD_F_PP_OTP_PAGE_4 | GD_F_UID_OTP_PAGE_6),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_rd_opcodes_a8d),
		   SNAND_SPI_MAX_SPEED_MHZ(80),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr2_2bits_no_parity_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F2GQ5UExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x52), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_READ_CACHE_SEQ),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_4_BITS | GD_F_PP_OTP_PAGE_4 | GD_F_UID_OTP_PAGE_6),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_rd_opcodes_a8d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&gd_ecc4bit_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F2GQ5RExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x42), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_READ_CACHE_SEQ),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_4_BITS | GD_F_PP_OTP_PAGE_4 | GD_F_UID_OTP_PAGE_6),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_rd_opcodes_a8d),
		   SNAND_SPI_MAX_SPEED_MHZ(80),
		   SNAND_PAGE_LAYOUT(&gd_ecc4bit_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F2GM7UExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x92), &snand_memorg_2g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp_10),
	),

	SNAND_PART("GD5F2GM7RExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x82), &snand_memorg_2g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp_10),
	),

	SNAND_PART("GD5F4GQ4xAYIG", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0xf4), &snand_memorg_4g_2k_64,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_2bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F4GQ4UCxIG", SNAND_ID(SNAND_ID_DIRECT, 0xc8, 0xb4, 0x68), &snand_memorg_4g_4k_256,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR_3BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_nor_read_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_4k_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F4GQ4RCxIG", SNAND_ID(SNAND_ID_DIRECT, 0xc8, 0xa4, 0x68), &snand_memorg_4g_4k_256, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR_3BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_nor_read_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_4k_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F4GQ4UBxIG", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0xd4), &snand_memorg_4g_4k_256,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_4k_ecc8bit_sr2_2bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F4GQ4RBxIG", SNAND_ID(SNAND_ID_ADDR, 0xc8, 0xc4), &snand_memorg_4g_4k_256, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_NO_PP),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_q2d),
		   SNAND_SPI_MAX_SPEED_MHZ(120),
		   SNAND_PAGE_LAYOUT(&gd_4k_ecc8bit_sr2_2bits_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F4GQ6UExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x55), &snand_memorg_4g_2k_128,
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_READ_CACHE_SEQ),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_4_BITS | GD_F_PP_OTP_PAGE_4 | GD_F_UID_OTP_PAGE_6),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_rd_opcodes_a8d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&gd_ecc4bit_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F4GQ6RExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x45), &snand_memorg_4g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 4),
		   SNAND_FLAGS(SNAND_F_NO_PP | SNAND_F_READ_CACHE_SEQ),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_4_BITS | GD_F_PP_OTP_PAGE_4 | GD_F_UID_OTP_PAGE_6),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(gd_rd_opcodes_a8d),
		   SNAND_SPI_MAX_SPEED_MHZ(80),
		   SNAND_PAGE_LAYOUT(&gd_ecc4bit_layout),
		   NAND_OTP_INFO(&gd_otp),
	),

	SNAND_PART("GD5F4GM8UExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x95), &snand_memorg_4g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp_10),
	),

	SNAND_PART("GD5F4GM8RExxG", SNAND_ID(SNAND_ID_DUMMY, 0xc8, 0x85), &snand_memorg_4g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_VENDOR_FLAGS(GD_F_ECC_CAP_8_BITS_SR2_2BITS),
		   SNAND_QE_CR_BIT0, SNAND_ECC_CR_BIT4, SNAND_OTP_CR_BIT6,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&gd_ecc8bit_sr_3bits_layout),
		   NAND_OTP_INFO(&gd_otp_10),
	),

};

static ufprog_status spi_nand_check_ecc_gd_sr_3bits(struct spi_nand *snand)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & GD_SR_ECC_SR_3_BITS_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;

	if (!sr)
		return UFP_OK;

	if (sr == 1) {
		snand->ecc_status->step_bitflips[0] = 3;
		return UFP_ECC_CORRECTED;
	}

	if (sr < 7) {
		snand->ecc_status->step_bitflips[0] = sr + 2;
		return UFP_ECC_CORRECTED;
	}

	snand->ecc_status->step_bitflips[0] = -1;

	return UFP_ECC_UNCORRECTABLE;
}

static ufprog_status spi_nand_check_ecc_gd_sr2_2bits_common(struct spi_nand *snand, uint32_t base, bool max_bf_st_3)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & SPI_NAND_STATUS_ECC_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;

	if (!sr)
		return UFP_OK;

	if (sr == 2 || (!max_bf_st_3 && sr == 3)) {
		snand->ecc_status->step_bitflips[0] = -1;
		return UFP_ECC_UNCORRECTABLE;
	}

	if (max_bf_st_3 && sr == 3) {
		snand->ecc_status->step_bitflips[0] = snand->nand.ecc_req.strength_per_step;
		return UFP_ECC_CORRECTED;
	}

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_GD_STATUS2_ADDR, &sr));

	sr = (sr & GD_SR2_ECCSE_MASK) >> GD_SR2_ECCSE_SHIFT;

	snand->ecc_status->step_bitflips[0] = base + sr;

	return UFP_ECC_CORRECTED;
}

static ufprog_status spi_nand_check_ecc_gd_sr2_2bits_ecc_4bits(struct spi_nand *snand)
{
	return spi_nand_check_ecc_gd_sr2_2bits_common(snand, 1, false);
}

static ufprog_status spi_nand_check_ecc_gd_sr2_2bits_ecc_8bits(struct spi_nand *snand)
{
	return spi_nand_check_ecc_gd_sr2_2bits_common(snand, 4, true);
}

static ufprog_status gd_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	if (bp->p.vendor_flags & GD_F_PP_OTP_PAGE_4)
		spi_nand_probe_onfi_generic(snand, bp, 4, false);

	bp->p.flags |= SNAND_F_RND_PAGE_WRITE;

	return UFP_OK;
}

static ufprog_status gd_nor_read_enable(struct spi_nand *snand)
{
	return UFP_OK;
}

static ufprog_status gd_nor_read_enabled(struct spi_nand *snand, ufprog_bool *retenabled)
{
	*retenabled = true;

	return UFP_OK;
}

static ufprog_status gd_read_uid(struct spi_nand *snand, void *data, uint32_t *retlen)
{
	return spi_nand_read_uid_otp(snand, 6, data, retlen);
}

static ufprog_status gd_part_set_ops(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	if (bp->p.vendor_flags & GD_F_ECC_CAP_1_BIT)
		snand->ext_param.ops.check_ecc = spi_nand_check_ecc_1bit_per_step;
	else if (bp->p.vendor_flags & GD_F_ECC_CAP_4_BITS)
		snand->ext_param.ops.check_ecc = spi_nand_check_ecc_gd_sr2_2bits_ecc_4bits;
	else if (bp->p.vendor_flags & GD_F_ECC_CAP_8_BITS_SR_2BITS)
		snand->ext_param.ops.check_ecc = spi_nand_check_ecc_8bits_sr_2bits;
	else if (bp->p.vendor_flags & GD_F_ECC_CAP_8_BITS_SR_3BITS)
		snand->ext_param.ops.check_ecc = spi_nand_check_ecc_gd_sr_3bits;
	else if (bp->p.vendor_flags & GD_F_ECC_CAP_8_BITS_SR2_2BITS)
		snand->ext_param.ops.check_ecc = spi_nand_check_ecc_gd_sr2_2bits_ecc_8bits;

	if (bp->p.flags & SNAND_F_NOR_READ_CAP) {
		snand->ext_param.ops.nor_read_enable = gd_nor_read_enable;
		snand->ext_param.ops.nor_read_enabled = gd_nor_read_enabled;
	}

	if (bp->p.vendor_flags & GD_F_UID_OTP_PAGE_6)
		snand->ext_param.ops.read_uid = gd_read_uid;

	if (bp->p.flags & SNAND_F_READ_CACHE_SEQ) {
		snand->state.seq_rd_feature_addr = SPI_NAND_FEATURE_GD_STATUS2_ADDR;
		snand->state.seq_rd_crbsy_mask = GD_SR2_CRBSY;
	}

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup gd_fixups = {
	.pre_param_setup = gd_part_fixup,
	.post_param_setup = gd_part_set_ops,
};

static ufprog_status gd_pp_post_init(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bp->p.qe_type = QE_CR_BIT0;
	bp->p.ecc_type = ECC_CR_BIT4;
	bp->p.otp_en_type = OTP_CR_BIT6;

	bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4;
	bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4;

	return UFP_OK;
}

static const struct spi_nand_vendor_ops gd_ops = {
	.pp_post_init = gd_pp_post_init,
};

const struct spi_nand_vendor vendor_gigadevice = {
	.mfr_id = SNAND_VENDOR_GIGADEVICE,
	.id = "gigadevice",
	.name = "GigaDevice",
	.parts = gigadevice_parts,
	.nparts = ARRAY_SIZE(gigadevice_parts),
	.ops = &gd_ops,
	.default_part_fixups = &gd_fixups,
	.default_part_otp_ops = &spi_nand_otp_ops,
	.vendor_flag_names = gd_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(gd_vendor_flag_info),
};
