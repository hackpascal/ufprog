/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash part definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NAND_PART_H_
#define _UFPROG_SPI_NAND_PART_H_

#include <stdint.h>
#include <ufprog/spi-nand.h>
#include <ecc-internal.h>
#include <nand-internal.h>

struct spi_nand_vendor;
struct spi_nand_flash_part_blank;

#define SNAND_F_META				BIT(0)
#define SNAND_F_NO_PP				BIT(1)
#define SNAND_F_GENERIC_UID			BIT(2)
#define SNAND_F_EXTENDED_ECC_BFR_8B		BIT(3)
#define SNAND_F_READ_CACHE_RANDOM		BIT(4)
#define SNAND_F_READ_CACHE_SEQ			BIT(5)
#define SNAND_F_NOR_READ_CAP			BIT(6)
#define SNAND_F_CONTINUOUS_READ			BIT(7)
#define SNAND_F_BBM_2ND_PAGE			BIT(8)
#define SNAND_F_NO_OP				BIT(9)
#define SNAND_F_RND_PAGE_WRITE			BIT(10)

#define SNAND_FLAGS(_f)				.flags = (_f)
#define SNAND_VENDOR_FLAGS(_f)			.vendor_flags = (_f)

#define SNAND_SPI_MAX_SPEED_MHZ(_speed)		.max_speed_spi_mhz = (_speed)
#define SNAND_DUAL_MAX_SPEED_MHZ(_speed)	.max_speed_dual_mhz = (_speed)
#define SNAND_QUAD_MAX_SPEED_MHZ(_speed)	.max_speed_quad_mhz = (_speed)

#define SNAND_RD_IO_CAPS(_caps)			.rd_io_caps = (_caps)
#define SNAND_PL_IO_CAPS(_caps)			.pl_io_caps = (_caps)

#define SNAND_MEMORG(_ps, _ss, _ppb, _bpl, _lpc, _ppl) \
	NAND_MEMORG((_ps), (_ss), (_ppb), (_bpl), (_lpc), 1, (_ppl))

enum spi_nand_id_type {
	SNAND_ID_DUMMY,
	SNAND_ID_ADDR,
	SNAND_ID_DIRECT,

	__SNAND_ID_TYPE_MAX
};

struct spi_nand_id {
	enum spi_nand_id_type type;
	struct nand_id val;
};

#define SNAND_ID(_type, ...)		{ .type = (_type), .val = NAND_ID(__VA_ARGS__) }

enum snand_quad_en_type {
	QE_UNKNOWN,
	QE_DONT_CARE,
	QE_CR_BIT0,
};

#define SNAND_QUAD_EN(_type)			.qe_type = (QE_##_type)
#define SNAND_QE_DONT_CARE			SNAND_QUAD_EN(DONT_CARE)
#define SNAND_QE_CR_BIT0			SNAND_QUAD_EN(CR_BIT0)

enum snand_ecc_en_type {
	ECC_UNKNOWN,
	ECC_UNSUPPORTED,
	ECC_ALWAYS_ON,
	ECC_CR_BIT4,
};

#define SNAND_ECC_EN(_type)			.ecc_type = (ECC_##_type)
#define SNAND_ECC_UNSUPPORTED			SNAND_ECC_EN(UNSUPPORTED)
#define SNAND_ECC_ALWAYS_ON			SNAND_ECC_EN(ALWAYS_ON)
#define SNAND_ECC_CR_BIT4			SNAND_ECC_EN(CR_BIT4)

enum snand_otp_en_type {
	OTP_UNKNOWN,
	OTP_UNSUPPORTED,
	OTP_CR_BIT6,
};

#define SNAND_OTP_EN(_type)			.otp_en_type = (OTP_##_type)
#define SNAND_OTP_UNSUPPORTED			SNAND_OTP_EN(UNSUPPORTED)
#define SNAND_OTP_CR_BIT6			SNAND_OTP_EN(CR_BIT6)

struct spi_nand_io_opcode {
	uint8_t opcode;
	uint8_t naddrs;
	uint8_t ndummy;
};

#define SNAND_IO_OPCODE(_io, _opcode, _naddrs, _ndummy)		\
	[_io] = { .opcode = (_opcode), .naddrs = (_naddrs), .ndummy = (_ndummy) }

#define SNAND_RD_OPCODES(_opcodes)		.rd_opcodes = (_opcodes)
#define SNAND_PL_OPCODES(_opcodes)		.pl_opcodes = (_opcodes)

#define SNAND_PAGE_LAYOUT(_layout)		.page_layout = (_layout)

struct spi_nand_flash_part_fixup {
	ufprog_status (*pre_param_setup)(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp);
	ufprog_status (*post_param_setup)(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp);
	ufprog_status (*pre_chip_setup)(struct spi_nand *snand);
};

#define SNAND_FIXUPS(_fixups)			.fixups = (_fixups)

struct spi_nand_flash_part_ops {
	ufprog_status (*chip_setup)(struct spi_nand *snand);
	ufprog_status (*select_die)(struct spi_nand *snand, uint32_t dieidx);
	ufprog_status (*quad_enable)(struct spi_nand *snand);
	ufprog_status (*ecc_control)(struct spi_nand *snand, bool enable);
	ufprog_status (*otp_control)(struct spi_nand *snand, bool enable);
	ufprog_status (*check_ecc)(struct spi_nand *snand);
	ufprog_status (*read_uid)(struct spi_nand *snand, void *data, uint32_t *retlen);
	ufprog_status (*nor_read_enable)(struct spi_nand *snand);
	ufprog_status (*nor_read_enabled)(struct spi_nand *snand, ufprog_bool *retenabled);
};

#define SNAND_OPS(_ops)				.ops = (_ops)

struct spi_nand_flash_part_alias_item {
	const struct spi_nand_vendor *vendor;
	const char *model;
};

#define SNAND_ALIAS_VENDOR_MODEL(_vendor, _model)	{ .vendor = (_vendor), .model = (_model) }
#define SNAND_ALIAS_MODEL(_model)			{ .model = (_model) }

struct spi_nand_flash_part_alias {
	uint32_t num;
	struct spi_nand_flash_part_alias_item items[];
};

#define DEFINE_SNAND_ALIAS(_name, ...)								\
	const struct spi_nand_flash_part_alias _name = {					\
		.items = { __VA_ARGS__ },							\
		.num = sizeof((const struct spi_nand_flash_part_alias_item[]){ __VA_ARGS__ }) /	\
		       sizeof(struct spi_nand_flash_part_alias_item) }

#define SNAND_ALIAS(_alias)			.alias = (_alias)

struct spi_nand_flash_part {
	const char *model;
	const struct spi_nand_flash_part_alias *alias;
	struct spi_nand_id id;
	uint32_t flags;
	uint32_t vendor_flags;
	uint32_t nops;

	enum snand_quad_en_type qe_type;
	enum snand_ecc_en_type ecc_type;
	enum snand_otp_en_type otp_en_type;

	uint32_t max_speed_spi_mhz;
	uint32_t max_speed_dual_mhz;
	uint32_t max_speed_quad_mhz;

	const struct nand_memorg *memorg;
	struct nand_ecc_config ecc_req;

	uint32_t rd_io_caps;
	const struct spi_nand_io_opcode *rd_opcodes;

	uint32_t pl_io_caps;
	const struct spi_nand_io_opcode *pl_opcodes;

	const struct nand_page_layout *page_layout;

	const struct nand_otp_info *otp;

	const struct spi_nand_flash_part_ops *ops;
	const struct spi_nand_flash_part_fixup *fixups;

	const struct nand_flash_otp_ops *otp_ops;

	uint32_t ext_id_flags;
};

struct spi_nand_flash_part_blank {
	struct spi_nand_flash_part p;

	char vendor[NAND_VENDOR_MODEL_LEN];
	char model[NAND_VENDOR_MODEL_LEN];

	struct nand_memorg memorg;

	struct spi_nand_io_opcode rd_opcodes[__SPI_MEM_IO_MAX];
	struct spi_nand_io_opcode pl_opcodes[__SPI_MEM_IO_MAX];
};

#define SNAND_PART(_model, _id, _memorg, _eccrq, ...) \
	{ .model = (_model), .id = _id, .memorg = _memorg, .ecc_req = _eccrq, __VA_ARGS__ }

/* Predefined opcode tables */
extern const struct spi_nand_io_opcode default_rd_opcodes_4d[];
extern const struct spi_nand_io_opcode default_rd_opcodes_q2d[];
extern const struct spi_nand_io_opcode default_pl_opcodes[];

/* Predefined register field values */
extern const struct nand_memorg snand_memorg_512m_2k_64;
extern const struct nand_memorg snand_memorg_512m_2k_128;
extern const struct nand_memorg snand_memorg_1g_2k_64;
extern const struct nand_memorg snand_memorg_2g_2k_64;
extern const struct nand_memorg snand_memorg_2g_2k_120;
extern const struct nand_memorg snand_memorg_4g_2k_64;
extern const struct nand_memorg snand_memorg_1g_2k_120;
extern const struct nand_memorg snand_memorg_1g_2k_128;
extern const struct nand_memorg snand_memorg_2g_2k_128;
extern const struct nand_memorg snand_memorg_4g_2k_128;
extern const struct nand_memorg snand_memorg_4g_4k_240;
extern const struct nand_memorg snand_memorg_4g_4k_256;
extern const struct nand_memorg snand_memorg_8g_2k_128;
extern const struct nand_memorg snand_memorg_8g_4k_256;
extern const struct nand_memorg snand_memorg_1g_2k_64_2p;
extern const struct nand_memorg snand_memorg_2g_2k_64_2p;
extern const struct nand_memorg snand_memorg_2g_2k_64_2d;
extern const struct nand_memorg snand_memorg_2g_2k_128_2p;
extern const struct nand_memorg snand_memorg_4g_2k_64_2p;
extern const struct nand_memorg snand_memorg_4g_2k_128_2p_2d;
extern const struct nand_memorg snand_memorg_8g_4k_256_2d;
extern const struct nand_memorg snand_memorg_8g_2k_128_2p_4d;

void spi_nand_prepare_blank_part(struct spi_nand_flash_part_blank *bp, const struct spi_nand_flash_part *refpart);
void spi_nand_blank_part_fill_default_opcodes(struct spi_nand_flash_part_blank *bp);

const struct spi_nand_flash_part *spi_nand_find_part(const struct spi_nand_flash_part *parts, size_t count,
						     enum spi_nand_id_type type, const uint8_t *id);
const struct spi_nand_flash_part *spi_nand_find_part_by_name(const struct spi_nand_flash_part *parts, size_t count,
							     const char *model);

#endif /* _UFPROG_SPI_NAND_PART_H_ */
