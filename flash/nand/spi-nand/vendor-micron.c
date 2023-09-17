// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Micron SPI-NAND flash parts
 */

#include <stdio.h>
#include <string.h>
#include <ufprog/sizes.h>
#include <ufprog/bits.h>
#include <ufprog/log.h>
#include "core.h"
#include "ecc.h"
#include "otp.h"
#include "vendor-micron.h"

/* Micron feature address */
#define SPI_NAND_FEATURE_MICRON_DIE_SEL_ADDR		0xc0
#define MICRON_DIE_SEL_SHIFT				6
#define MICRON_DIE_SEL_MASK				BITS(7, MICRON_DIE_SEL_SHIFT)

/* Micron ECC status bits */
#define MICRON_SR_ECC_8_BITS_MASK			BITS(6, SPI_NAND_STATUS_ECC_SHIFT)

/* Micron configuration bits */
#define SPI_NAND_CONFIG_MICRON_CONTINUOUS_READ		BIT(0)

/* SPI-NAND opcodes for Micron M70A */
#define SNAND_CMD_PROGRAM_LOAD_X2_M70A			0xa2
#define SNAND_CMD_RND_PROGRAM_LOAD_X2_M70A		0x44

/* Micron vendor flags */
#define MT_F_ECC_CAP_1_BIT				BIT(0)
#define MT_F_ECC_CAP_8_BITS				BIT(1)

static const struct spi_nand_part_flag_enum_info mt_vendor_flag_info[] = {
	{ 0, "ecc-1-bit" },
	{ 1, "ecc-8-bits" },
};

static struct nand_otp_info mt_otp = {
	.start_index = NAND_OTP_PAGE_OTP,
	.count = 10,
};

const struct nand_page_layout mt_2k_ecc_8bits_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(2048),
	ECC_PAGE_MARKER_BYTES(2),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(28),
	ECC_PAGE_OOB_DATA_BYTES(32),
	ECC_PAGE_PARITY_BYTES(64),
);

static const struct nand_page_layout mt_4k_ecc_8bits_layout = ECC_PAGE_LAYOUT(
	ECC_PAGE_DATA_BYTES(4096),
	ECC_PAGE_MARKER_BYTES(2),
	ECC_PAGE_UNUSED_BYTES(2),
	ECC_PAGE_OOB_FREE_BYTES(60),
	ECC_PAGE_OOB_DATA_BYTES(64),
	ECC_PAGE_PARITY_BYTES(128),
);

static struct spi_nand_io_opcode opcode_rfc_cfg_check = {
	.opcode = SNAND_CMD_READ_FROM_CACHE,
	.naddrs = 2,
	.ndummy = 8,
};

static const struct spi_nand_io_opcode m70a_pl_opcodes[__SPI_MEM_IO_MAX] = {
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_1, SNAND_CMD_PROGRAM_LOAD, 2, 0),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_2, SNAND_CMD_PROGRAM_LOAD_X2_M70A, 2, 0),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_4, SNAND_CMD_PROGRAM_LOAD_QUAD_IN, 2, 0),
};

const struct spi_nand_io_opcode m70a_upd_opcodes[__SPI_MEM_IO_MAX] = {
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_1, SNAND_CMD_RND_PROGRAM_LOAD, 2, 0),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_2, SNAND_CMD_RND_PROGRAM_LOAD_X2_M70A, 2, 0),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_4, SNAND_CMD_RND_PROGRAM_LOAD_QUAD_IN, 2, 0),
};

static DEFINE_SNAND_ALIAS(mt29f1g01abafd_alias, SNAND_ALIAS_VENDOR_MODEL(&vendor_esmt, "F50L1G41XA"));
static DEFINE_SNAND_ALIAS(mt29f2g01abagd_alias, SNAND_ALIAS_VENDOR_MODEL(&vendor_esmt, "F50L2G41XA"),
						SNAND_ALIAS_VENDOR_MODEL(&vendor_xtx, "XT26G02E"));
static DEFINE_SNAND_ALIAS(mt29f2g01abbgd_alias, SNAND_ALIAS_VENDOR_MODEL(&vendor_esmt, "F50D2G41XA"));
static DEFINE_SNAND_ALIAS(mt29f4g01abafd_alias, SNAND_ALIAS_VENDOR_MODEL(&vendor_esmt, "F50L4G41XB"));
static DEFINE_SNAND_ALIAS(mt29f4g01abbfd_alias, SNAND_ALIAS_VENDOR_MODEL(&vendor_esmt, "F50D4G41XB"));

static const struct spi_nand_flash_part micron_parts[] = {
	/* M68A */
	SNAND_PART("MT29F1G01AAADD", SNAND_ID(SNAND_ID_DUMMY, 0x2c, 0x12), &snand_memorg_1g_2k_64_2p,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_VENDOR_FLAGS(MT_F_ECC_CAP_1_BIT),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(50),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&mt_otp),
	),

	/* M78A */
	SNAND_PART("MT29F1G01ABAFD", SNAND_ID(SNAND_ID_DUMMY, 0x2c, 0x14), &snand_memorg_1g_2k_128,
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&mt29f1g01abafd_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(MT_F_ECC_CAP_8_BITS),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133), SNAND_DUAL_MAX_SPEED_MHZ(108), SNAND_QUAD_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&mt_otp),
	),


	SNAND_PART("MT29F1G01ABBFD", SNAND_ID(SNAND_ID_DUMMY, 0x2c, 0x15), &snand_memorg_1g_2k_128, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(MT_F_ECC_CAP_8_BITS),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(83), SNAND_DUAL_MAX_SPEED_MHZ(50), SNAND_QUAD_MAX_SPEED_MHZ(50),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&mt_otp),
	),

	/* M69A */
	SNAND_PART("MT29F2G01AAAED", SNAND_ID(SNAND_ID_DUMMY, 0x2c, 0x9f), &snand_memorg_2g_2k_64_2p,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_VENDOR_FLAGS(MT_F_ECC_CAP_1_BIT),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(50),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&mt_otp),
	),

	/* MT79A */
	SNAND_PART("MT29F2G01ABAGD", SNAND_ID(SNAND_ID_DUMMY, 0x2c, 0x24), &snand_memorg_2g_2k_128_2p,
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&mt29f2g01abagd_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(MT_F_ECC_CAP_8_BITS),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133), SNAND_DUAL_MAX_SPEED_MHZ(108), SNAND_QUAD_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&mt_otp),
	),

	SNAND_PART("MT29F2G01ABBGD", SNAND_ID(SNAND_ID_DUMMY, 0x2c, 0x25), &snand_memorg_2g_2k_128_2p, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&mt29f2g01abbgd_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(MT_F_ECC_CAP_8_BITS),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(83), SNAND_DUAL_MAX_SPEED_MHZ(50), SNAND_QUAD_MAX_SPEED_MHZ(50),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&mt_otp),
	),

	/* M60A */
	SNAND_PART("MT29F4G01AAADD", SNAND_ID(SNAND_ID_DUMMY, 0x2c, 0x32), &snand_memorg_4g_2k_64_2p,
		   NAND_ECC_REQ(512, 1),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID),
		   SNAND_VENDOR_FLAGS(MT_F_ECC_CAP_1_BIT),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(50),
		   SNAND_PAGE_LAYOUT(&ecc_2k_64_1bit_layout),
		   NAND_OTP_INFO(&mt_otp),
	),

	/* M70A */
	SNAND_PART("MT29F4G01ABAFD", SNAND_ID(SNAND_ID_DUMMY, 0x2c, 0x34), &snand_memorg_4g_4k_256,
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&mt29f4g01abafd_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_CONTINUOUS_READ),
		   SNAND_VENDOR_FLAGS(MT_F_ECC_CAP_8_BITS),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_PL_OPCODES(m70a_pl_opcodes),
		   SNAND_UPD_OPCODES(m70a_upd_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(133), SNAND_DUAL_MAX_SPEED_MHZ(100), SNAND_QUAD_MAX_SPEED_MHZ(50),
		   SNAND_PAGE_LAYOUT(&mt_4k_ecc_8bits_layout),
		   NAND_OTP_INFO(&mt_otp),
	),

	SNAND_PART("MT29F4G01ABBFD", SNAND_ID(SNAND_ID_DUMMY, 0x2c, 0x35), &snand_memorg_4g_4k_256, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_ALIAS(&mt29f4g01abbfd_alias),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_CONTINUOUS_READ),
		   SNAND_VENDOR_FLAGS(MT_F_ECC_CAP_8_BITS),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_PL_OPCODES(m70a_pl_opcodes),
		   SNAND_UPD_OPCODES(m70a_upd_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(83), SNAND_DUAL_MAX_SPEED_MHZ(74), SNAND_QUAD_MAX_SPEED_MHZ(37),
		   SNAND_PAGE_LAYOUT(&mt_4k_ecc_8bits_layout),
		   NAND_OTP_INFO(&mt_otp),
	),

	/* M79A */
	SNAND_PART("MT29F4G01ADAGD", SNAND_ID(SNAND_ID_DUMMY, 0x2c, 0x36), &snand_memorg_4g_2k_128_2p_2d,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP),
		   SNAND_VENDOR_FLAGS(MT_F_ECC_CAP_8_BITS),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_SPI_MAX_SPEED_MHZ(133), SNAND_DUAL_MAX_SPEED_MHZ(108), SNAND_QUAD_MAX_SPEED_MHZ(108),
		   SNAND_PAGE_LAYOUT(&mt_2k_ecc_8bits_layout),
		   NAND_OTP_INFO(&mt_otp),
	),

	/* M70A */
	SNAND_PART("MT29F8G01ADAFD", SNAND_ID(SNAND_ID_DUMMY, 0x2c, 0x46), &snand_memorg_8g_4k_256_2d,
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_CONTINUOUS_READ),
		   SNAND_VENDOR_FLAGS(MT_F_ECC_CAP_8_BITS),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_PL_OPCODES(m70a_pl_opcodes),
		   SNAND_UPD_OPCODES(m70a_upd_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(133), SNAND_DUAL_MAX_SPEED_MHZ(100), SNAND_QUAD_MAX_SPEED_MHZ(50),
		   SNAND_PAGE_LAYOUT(&mt_4k_ecc_8bits_layout),
		   NAND_OTP_INFO(&mt_otp),
	),

	SNAND_PART("MT29F8G01ADBFD", SNAND_ID(SNAND_ID_DUMMY, 0x2c, 0x47), &snand_memorg_8g_4k_256_2d, /* 1.8V */
		   NAND_ECC_REQ(512, 8),
		   SNAND_FLAGS(SNAND_F_GENERIC_UID | SNAND_F_READ_CACHE_RANDOM | SNAND_F_NOR_READ_CAP |
			       SNAND_F_CONTINUOUS_READ),
		   SNAND_VENDOR_FLAGS(MT_F_ECC_CAP_8_BITS),
		   SNAND_QE_DONT_CARE, SNAND_ECC_CR_BIT4,
		   SNAND_RD_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_X4),
		   SNAND_PL_IO_CAPS(BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4),
		   SNAND_RD_OPCODES(default_rd_opcodes_4d),
		   SNAND_PL_OPCODES(m70a_pl_opcodes),
		   SNAND_UPD_OPCODES(m70a_upd_opcodes),
		   SNAND_SPI_MAX_SPEED_MHZ(83), SNAND_DUAL_MAX_SPEED_MHZ(74), SNAND_QUAD_MAX_SPEED_MHZ(37),
		   SNAND_PAGE_LAYOUT(&mt_4k_ecc_8bits_layout),
		   NAND_OTP_INFO(&mt_otp),
	),
};

ufprog_status spi_nand_micron_cfg_enabled(struct spi_nand *snand, uint8_t cfg, uint32_t check_size, uint8_t *buf,
					  bool compromise, bool *retenabled)
{
	uint32_t i, n, zeros = 0, ones = 0;
	ufprog_status ret;

	STATUS_CHECK_RET(spi_nand_set_low_speed(snand));

	/* Disable ECC */
	STATUS_CHECK_RET(spi_nand_ondie_ecc_control(snand, false));

	/* Enter OTP protection mode */
	STATUS_CHECK_GOTO_RET(spi_nand_update_config(snand, SPI_NAND_MICRON_CR_CFG_MASK, cfg), ret, out);

	/* Read status to cache */
	STATUS_CHECK_GOTO_RET(spi_nand_page_op(snand, 0, SNAND_CMD_READ_TO_CACHE), ret, out);
	STATUS_CHECK_GOTO_RET(spi_nand_wait_busy(snand, SNAND_POLL_MAX_US, NULL), ret, out);

	/* Read status */
	ret = spi_nand_read_cache_custom(snand, &opcode_rfc_cfg_check,
					 ufprog_spi_mem_io_bus_width_info(SPI_MEM_IO_1_1_1), 0, check_size, buf);

out:
	/* Leave OTP protection mode */
	spi_nand_update_config(snand, SPI_NAND_MICRON_CR_CFG_MASK, 0);

	if (ret)
		return ret;

	for (i = 0; i < check_size; i++) {
		if (!buf[i]) {
			zeros += 8;
		} else if (buf[i] == 0xff) {
			ones += 8;
		} else {
			n = hweight8(buf[i]);
			ones += n;
			zeros += 8 - n;
		}
	}

	if (((zeros && ones) || zeros == ones) && !compromise) {
		logm_err("Invalid status of configuration mode %02x\n", cfg);
		return UFP_FAIL;
	}

	*retenabled = zeros > ones;

	return UFP_OK;
}

ufprog_status spi_nand_micron_enable_cfg(struct spi_nand *snand, uint8_t cfg)
{
	ufprog_status ret;
	uint8_t sr;

	/* Enter OTP protection mode */
	STATUS_CHECK_GOTO_RET(spi_nand_update_config(snand, SPI_NAND_MICRON_CR_CFG_MASK, cfg), ret, out);

	STATUS_CHECK_GOTO_RET(spi_nand_write_enable(snand), ret, out);

	STATUS_CHECK_GOTO_RET(spi_nand_page_op(snand, 0, SNAND_CMD_PROGRAM_EXECUTE), ret, out);

	ret = spi_nand_wait_busy(snand, SNAND_POLL_MAX_US, &sr);
	if (ret) {
		logm_err("Configuration enabling operation %02x timed out\n", cfg);
		goto out;
	}

	if (sr & SPI_NAND_STATUS_PROGRAM_FAIL) {
		logm_err("Configuration enabling operation %02x failed\n", cfg);
		ret = UFP_FLASH_PROGRAM_FAILED;
		goto out;
	}

	ret = UFP_OK;

out:
	/* Leave OTP protection mode */
	spi_nand_update_config(snand, SPI_NAND_MICRON_CR_CFG_MASK, 0);

	spi_nand_write_disable(snand);

	return ret;
}

ufprog_status spi_nand_otp_control_micron(struct spi_nand *snand, bool enable)
{
	ufprog_status ret;

	if (enable)
		ret = spi_nand_update_config(snand, SPI_NAND_MICRON_CR_CFG_MASK, SPI_NAND_CONFIG_OTP_EN);
	else
		ret = spi_nand_update_config(snand, SPI_NAND_MICRON_CR_CFG_MASK, 0);

	if (ret)
		logm_err("Failed to %s OTP mode\n", enable ? "enable" : "disable");

	return ret;
}

ufprog_status spi_nand_select_die_micron(struct spi_nand *snand, uint32_t dieidx)
{
	uint8_t val;

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_MICRON_DIE_SEL_ADDR, &val));

	val &= ~MICRON_DIE_SEL_MASK;
	val |= (dieidx << MICRON_DIE_SEL_SHIFT) & MICRON_DIE_SEL_MASK;

	return spi_nand_set_feature(snand, SPI_NAND_FEATURE_MICRON_DIE_SEL_ADDR, val);
}

ufprog_status spi_nand_check_ecc_micron_8bits(struct spi_nand *snand)
{
	uint8_t sr;

	spi_nand_reset_ecc_status(snand);

	STATUS_CHECK_RET(spi_nand_get_feature(snand, SPI_NAND_FEATURE_STATUS_ADDR, &sr));

	sr = (sr & MICRON_SR_ECC_8_BITS_MASK) >> SPI_NAND_STATUS_ECC_SHIFT;

	switch (sr) {
	case 0:
		return UFP_OK;

	case 1:
		snand->ecc_status->step_bitflips[0] = 3;
		return UFP_ECC_CORRECTED;

	case 3:
		snand->ecc_status->step_bitflips[0] = 6;
		return UFP_ECC_CORRECTED;

	case 5:
		snand->ecc_status->step_bitflips[0] = 8;
		return UFP_ECC_CORRECTED;

	default:
		snand->ecc_status->step_bitflips[0] = -1;
		return UFP_ECC_UNCORRECTABLE;
	}
}

ufprog_status micron_part_fixup(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bool enabled = false;
	ufprog_status ret;
	uint8_t *buf;

	spi_nand_blank_part_fill_default_opcodes(bp);

	bp->p.nops = bp->p.memorg->page_size / 512;

	if (bp->p.flags & SNAND_F_NOR_READ_CAP) {
		buf = malloc(bp->p.memorg->page_size + bp->p.memorg->oob_size);
		if (!buf) {
			logm_err("Unable to allocate temporary page buffer\n");
			return UFP_NOMEM;
		}

		ret = spi_nand_micron_cfg_enabled(snand, SPI_NAND_MICRON_CR_CFG_NOR_READ,
						  bp->p.memorg->page_size + bp->p.memorg->oob_size, buf, true,
						  &enabled);

		free(buf);

		if (ret)
			return ret;

		if (enabled) {
			if (bp->p.rd_opcodes != bp->rd_opcodes) {
				memcpy(&bp->rd_opcodes, bp->p.rd_opcodes, sizeof(bp->rd_opcodes));
				bp->p.rd_opcodes = bp->rd_opcodes;
			}

			bp->rd_opcodes[SPI_MEM_IO_1_1_1].opcode = SNAND_CMD_FAST_READ_FROM_CACHE;
			bp->rd_opcodes[SPI_MEM_IO_1_1_1].naddrs = 3;
			bp->rd_opcodes[SPI_MEM_IO_1_1_1].ndummy = 8;
		}
	}

	bp->p.flags |= SNAND_F_RND_PAGE_WRITE;

	return UFP_OK;
}

ufprog_status micron_nor_read_enable(struct spi_nand *snand)
{
	return spi_nand_micron_enable_cfg(snand, SPI_NAND_MICRON_CR_CFG_NOR_READ);
}

ufprog_status micron_nor_read_enabled(struct spi_nand *snand, ufprog_bool *retenabled)
{
	bool enabled = false;
	ufprog_status ret;

	ret = spi_nand_micron_cfg_enabled(snand, SPI_NAND_MICRON_CR_CFG_NOR_READ, snand->nand.maux.oob_page_size,
					  snand->scratch_buffer, true, &enabled);

	/* Restore bus speed */
	spi_nand_set_high_speed(snand);

	*retenabled = enabled;

	return ret;
}

static ufprog_status micron_part_set_ops(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	if (bp->p.vendor_flags & MT_F_ECC_CAP_1_BIT)
		snand->ext_param.ops.check_ecc = spi_nand_check_ecc_1bit_per_step;
	else if (bp->p.vendor_flags & MT_F_ECC_CAP_8_BITS)
		snand->ext_param.ops.check_ecc = spi_nand_check_ecc_micron_8bits;

	snand->ext_param.ops.otp_control = spi_nand_otp_control_micron;

	if (bp->p.flags & SNAND_F_NOR_READ_CAP) {
		snand->ext_param.ops.nor_read_enable = micron_nor_read_enable;
		snand->ext_param.ops.nor_read_enabled = micron_nor_read_enabled;
	}

	return UFP_OK;
}

static const struct spi_nand_flash_part_fixup micron_fixups = {
	.pre_param_setup = micron_part_fixup,
	.post_param_setup = micron_part_set_ops,
};

static ufprog_status micron_setup_chip(struct spi_nand *snand)
{
	if (snand->param.vendor_flags & SNAND_F_CONTINUOUS_READ)
		STATUS_CHECK_RET(spi_nand_update_config(snand, SPI_NAND_CONFIG_MICRON_CONTINUOUS_READ, 0));

	return UFP_OK;
}

static const struct spi_nand_flash_part_ops micron_part_ops = {
	.chip_setup = micron_setup_chip,
	.select_die = spi_nand_select_die_micron,
	.otp_control = spi_nand_otp_control_micron,
};

static ufprog_status micron_pp_post_init(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp)
{
	bp->p.qe_type = QE_CR_BIT0;
	bp->p.ecc_type = ECC_UNKNOWN;
	bp->p.otp_en_type = OTP_UNKNOWN;

	bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_1_4;
	bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1 | BIT_SPI_MEM_IO_1_1_4;

	return UFP_OK;
}

static const struct spi_nand_vendor_ops micron_ops = {
	.pp_post_init = micron_pp_post_init,
};

const struct spi_nand_vendor vendor_micron = {
	.mfr_id = SNAND_VENDOR_MICRON,
	.id = "micron",
	.name = "Micron",
	.parts = micron_parts,
	.nparts = ARRAY_SIZE(micron_parts),
	.ops = &micron_ops,
	.default_part_ops = &micron_part_ops,
	.default_part_fixups = &micron_fixups,
	.default_part_otp_ops = &spi_nand_otp_micron_ops,
	.vendor_flag_names = mt_vendor_flag_info,
	.num_vendor_flag_names = ARRAY_SIZE(mt_vendor_flag_info),
};
