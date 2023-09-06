// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ISSI SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include "core.h"
#include "otp.h"
#include "vendor-micron.h"

static struct nand_otp_info issi_otp = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 30,
};

static DEFINE_SNAND_ALIAS(is37sml01g8a_alias, SNAND_ALIAS_MODEL("IS38SML01G8A"));
static DEFINE_SNAND_ALIAS(is37smw01g8a_alias, SNAND_ALIAS_MODEL("IS38SMW01G8A"));
static DEFINE_SNAND_ALIAS(is37sml02g8a_alias, SNAND_ALIAS_MODEL("IS38SML02G8A"));
static DEFINE_SNAND_ALIAS(is37smw02g8a_alias, SNAND_ALIAS_MODEL("IS38SMW02G8A"));
static DEFINE_SNAND_ALIAS(is37sml04g8a_alias, SNAND_ALIAS_MODEL("IS38SML04G8A"));
static DEFINE_SNAND_ALIAS(is37smw04g8a_alias, SNAND_ALIAS_MODEL("IS38SMW04G8A"));
static DEFINE_SNAND_ALIAS(is37sml08g8a_alias, SNAND_ALIAS_MODEL("IS38SML08G8A"));
static DEFINE_SNAND_ALIAS(is37smw08g8a_alias, SNAND_ALIAS_MODEL("IS38SMW08G8A"));

static const struct spi_nand_flash_part issi_parts[] = {
	SNAND_PART("IS37SML01G8A", SNAND_ID(SNAND_ID_DUMMY, 0x9d, 0x16), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&is37sml01g8a_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&issi_otp),
	),

	SNAND_PART("IS37SMW01G8A", SNAND_ID(SNAND_ID_DUMMY, 0x9d, 0x17), &snand_memorg_1g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&is37smw01g8a_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&issi_otp),
	),

	SNAND_PART("IS37SML02G8A", SNAND_ID(SNAND_ID_DUMMY, 0x9d, 0x26), &snand_memorg_2g_2k_128_2p,
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&is37sml02g8a_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&issi_otp),
	),

	SNAND_PART("IS37SMW02G8A", SNAND_ID(SNAND_ID_DUMMY, 0x9d, 0x27), &snand_memorg_2g_2k_128_2p, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&is37smw02g8a_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&issi_otp),
	),

	SNAND_PART("IS37SML04G8A", SNAND_ID(SNAND_ID_DUMMY, 0x9d, 0x36), &snand_memorg_4g_2k_128_2p_2d,
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&is37sml04g8a_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&issi_otp),
	),

	SNAND_PART("IS37SMW04G8A", SNAND_ID(SNAND_ID_DUMMY, 0x9d, 0x37), &snand_memorg_4g_2k_128_2p_2d, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&is37smw04g8a_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&issi_otp),
	),

	SNAND_PART("IS37SML08G8A", SNAND_ID(SNAND_ID_DUMMY, 0x9d, 0x46), &snand_memorg_8g_2k_128_2p_4d,
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&is37sml08g8a_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&issi_otp),
	),

	SNAND_PART("IS37SMW08G8A", SNAND_ID(SNAND_ID_DUMMY, 0x9d, 0x47), &snand_memorg_8g_2k_128_2p_4d, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&is37smw08g8a_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(104),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&issi_otp),
	),
};

static const struct spi_nand_flash_part_fixup issi_fixups = {
	.pre_param_setup = micron_part_fixup,
};

static const struct spi_nand_flash_part_ops issi_part_ops = {
	.select_die = spi_nand_select_die_micron,
	.check_ecc = spi_nand_check_ecc_micron_8bits,
	.otp_control = spi_nand_otp_control_micron,
	.nor_read_enable = micron_nor_read_enable,
	.nor_read_enabled = micron_nor_read_enabled,
};

static ufprog_status issi_pp_post_init(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bp->p.qe_type = QE_CR_BIT0;
	bp->p.ecc_type = ECC_UNKNOWN;
	bp->p.otp_en_type = OTP_UNKNOWN;

	bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4;
	bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4;

	return UFP_OK;
}

static const struct spi_nand_vendor_ops issi_ops = {
	.pp_post_init = issi_pp_post_init,
};

const struct spi_nand_vendor vendor_issi = {
	.mfr_id = SNAND_VENDOR_ISSI,
	.id = "issi",
	.name = "ISSI",
	.parts = issi_parts,
	.nparts = ARRAY_SIZE(issi_parts),
	.ops = &issi_ops,
	.default_part_ops = &issi_part_ops,
	.default_part_fixups = &issi_fixups,
	.default_part_otp_ops = &spi_nand_otp_micron_ops,
};
