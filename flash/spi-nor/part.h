/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash part definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NOR_PART_H_
#define _UFPROG_SPI_NOR_PART_H_

#include <stdint.h>
#include <ufprog/spi-nor.h>
#include "regs.h"
#include "wp.h"

struct spi_nor_vendor_part;
struct spi_nor_flash_part_blank;

#define SNOR_VENDOR_MODEL_LEN			128

#define SNOR_F_META				BIT(0)
#define SNOR_F_NO_SFDP				BIT(1)
#define SNOR_F_SECT_4K				BIT(2)
#define SNOR_F_SECT_32K				BIT(3)
#define SNOR_F_SECT_64K				BIT(4)
#define SNOR_F_SECT_256K			BIT(5)
#define SNOR_F_SR_NON_VOLATILE			BIT(6)
#define SNOR_F_SR_VOLATILE			BIT(7)
#define SNOR_F_SR_VOLATILE_WREN_50H		BIT(8)
#define SNOR_F_UNIQUE_ID			BIT(9)
#define SNOR_F_FULL_DPI_OPCODES			BIT(10)
#define SNOR_F_FULL_QPI_OPCODES			BIT(11)
#define SNOR_F_SFDP_4B_MODE			BIT(12)
#define SNOR_F_GLOBAL_UNLOCK			BIT(13)
#define SNOR_F_AAI_WRITE			BIT(14)
#define SNOR_F_NO_OP				BIT(15)
#define SNOR_F_BYPASS_VENDOR_FIXUPS		BIT(16)

#define SNOR_FLAGS(_f)				.flags = (_f)
#define SNOR_VENDOR_FLAGS(_f)			.vendor_flags = (_f)

#define SNOR_NDIES(_num)			.ndies = (_num)

#define SNOR_ID(...)	{ .id = { __VA_ARGS__ }, .len = sizeof((uint8_t[]){ __VA_ARGS__ }) }
#define SNOR_ID_NONE	{ .id = { 0 }, .len = 0 }

#define SNOR_ID_MASK(_mask)			.id_mask = _mask

struct spi_nor_erase_sector_info {
	uint8_t opcode;
	uint32_t size;
	uint32_t max_erase_time_ms;
};

#define SNOR_ERASE_MAX_TIME_MS(_ms)		.max_erase_time_ms = (_ms)
#define SNOR_ERASE_SECTOR(_size, _opcode, ...)	{ .size = (_size), .opcode = (_opcode), __VA_ARGS__ }

struct spi_nor_erase_info {
	struct spi_nor_erase_sector_info info[SPI_NOR_MAX_ERASE_INFO];
};

#define SNOR_ERASE_SECTORS(...)			{ .info = { __VA_ARGS__ } }

#define SNOR_ERASE_INFO(_info)			.erase_info_3b = (_info)
#define SNOR_ERASE_INFO_4B(_info)		.erase_info_4b = (_info)

#define SNOR_SPI_MAX_SPEED_MHZ(_speed)		.max_speed_spi_mhz = (_speed)
#define SNOR_DUAL_MAX_SPEED_MHZ(_speed)		.max_speed_dual_mhz = (_speed)
#define SNOR_QUAD_MAX_SPEED_MHZ(_speed)		.max_speed_quad_mhz = (_speed)

#define SNOR_READ_IO_CAPS(_caps)		.read_io_caps = (_caps)
#define SNOR_PP_IO_CAPS(_caps)			.pp_io_caps = (_caps)

struct spi_nor_io_opcode {
	uint8_t opcode;
	uint8_t ndummy;
	uint8_t nmode;
};
#define SNOR_IO_OPCODE(_iotype, _opcode, _ndummy, _nmode)	\
	[_iotype] = { .opcode = (_opcode), .ndummy = (_ndummy), .nmode = (_nmode) }

#define SNOR_READ_OPCODES(_opcodes)		.read_opcodes_3b = (_opcodes)
#define SNOR_READ_OPCODES_4B(_opcodes)		.read_opcodes_4b = (_opcodes)
#define SNOR_PP_OPCODES(_opcodes)		.pp_opcodes_3b = (_opcodes)
#define SNOR_PP_OPCODES_4B(_opcodes)		.pp_opcodes_4b = (_opcodes)

enum snor_quad_en_type {
	QE_UNKNOWN,
	QE_DONT_CARE,
	QE_SR1_BIT6,
	QE_SR2_BIT1,
	QE_SR2_BIT1_WR_SR1,
	QE_SR2_BIT7,
	QE_NVCR_BIT4,
};

#define SNOR_QUAD_EN(_type)			.qe_type = (QE_##_type)
#define SNOR_QE_DONT_CARE			SNOR_QUAD_EN(DONT_CARE)
#define SNOR_QE_SR1_BIT6			SNOR_QUAD_EN(SR1_BIT6)
#define SNOR_QE_SR2_BIT1			SNOR_QUAD_EN(SR2_BIT1)
#define SNOR_QE_SR2_BIT1_WR_SR1			SNOR_QUAD_EN(SR2_BIT1_WR_SR1)
#define SNOR_QE_SR2_BIT7			SNOR_QUAD_EN(SR2_BIT7)
#define SNOR_QE_NVCR_BIT4			SNOR_QUAD_EN(NVCR_BIT4)

enum snor_qpi_en_type {
	QPI_EN_NONE,
	QPI_EN_VENDOR,
	QPI_EN_QER_38H,
	QPI_EN_38H,
	QPI_EN_35H,
	QPI_EN_VECR_BIT7_CLR,
};

enum snor_qpi_dis_type {
	QPI_DIS_NONE,
	QPI_DIS_VENDOR,
	QPI_DIS_FFH,
	QPI_DIS_F5H,
	QPI_DIS_66H_99H,
};

#define SNOR_QPI(_en_type, _dis_type)			\
	.qpi_en_type = (QPI_EN_##_en_type),		\
	.qpi_dis_type = (QPI_DIS_##_dis_type)

#define SNOR_QPI_VENDOR				SNOR_QPI(VENDOR, VENDOR)
#define SNOR_QPI_QER_38H_FFH			SNOR_QPI(QER_38H, FFH)
#define SNOR_QPI_38H_FFH			SNOR_QPI(38H, FFH)
#define SNOR_QPI_35H_F5H			SNOR_QPI(35H, F5H)

enum snor_4b_en_type {
	A4B_EN_NONE,
	A4B_EN_B7H,
	A4B_EN_WREN_B7H,
	A4B_EN_EAR,
	A4B_EN_BANK,
	A4B_EN_NVCR,
	A4B_EN_4B_OPCODE,
	A4B_EN_ALWAYS,
};

enum snor_4b_dis_type {
	A4B_DIS_NONE,
	A4B_DIS_E9H,
	A4B_DIS_WREN_E9H,
	A4B_DIS_EAR,
	A4B_DIS_BANK,
	A4B_DIS_NVCR,
	A4B_DIS_66H_99H,
};

#define SNOR_4B(_en_type, _dis_type)			\
	.a4b_en_type = (A4B_EN_##_en_type),		\
	.a4b_dis_type = (A4B_DIS_##_dis_type)

#define SNOR_4B_F_B7H_E9H			BIT(0)
#define SNOR_4B_F_WREN_B7H_E9H			BIT(1)
#define SNOR_4B_F_EAR				BIT(2)
#define SNOR_4B_F_BANK				BIT(3)
#define SNOR_4B_F_NVCR				BIT(4)
#define SNOR_4B_F_OPCODE			BIT(5)
#define SNOR_4B_F_ALWAYS			BIT(6)

#define SNOR_4B_FLAGS(_flags)			.a4b_flags = (_flags)

#define SNOR_SOFT_RESET_DRV_FH_4IO_8CLKS	BIT(0)
#define SNOR_SOFT_RESET_DRV_FH_4IO_10CLKS_4B	BIT(1)
#define SNOR_SOFT_RESET_DRV_FH_4IO_16CLKS	BIT(2)
#define SNOR_SOFT_RESET_OPCODE_F0H		BIT(3)
#define SNOR_SOFT_RESET_OPCODE_66H_99H		BIT(4)

#define SNOR_SOFT_RESET_FLAGS(_flags)		.soft_reset_flags = (_flags)


#define SNOR_REGS(_regs)			.regs = (_regs)
#define SNOR_OTP_INFO(_info)			.otp = (_info)
#define SNOR_WP_RANGES(_wp_ranges)		.wp_ranges = (_wp_ranges)

#define SNOR_PAGE_SIZE(_pgsz)			.page_size = (_pgsz)
#define SNOR_PP_MAX_TIME_US(_us)		.max_pp_time_us = (_us)

struct spi_nor_flash_part_fixup {
	ufprog_status (*pre_param_setup)(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
					 struct spi_nor_flash_part_blank *bp);
	ufprog_status (*post_param_setup)(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp);
	ufprog_status (*pre_chip_setup)(struct spi_nor *snor);
};

#define SNOR_FIXUPS(_fixups)			.fixups = (_fixups)

struct spi_nor_flash_secr_otp_ops {
	uint32_t (*otp_addr)(struct spi_nor *snor, uint32_t index, uint32_t addr);
	ufprog_status (*otp_lock_bit)(struct spi_nor *snor, uint32_t index, uint32_t *retbit,
				      const struct spi_nor_reg_access **retacc);
};

struct spi_nor_flash_part_otp_ops {
	ufprog_status (*read)(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data);
	ufprog_status (*write)(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, const void *data);
	ufprog_status (*erase)(struct spi_nor *snor, uint32_t index);
	ufprog_status (*lock)(struct spi_nor *snor, uint32_t index);
	ufprog_status (*locked)(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked);

	const struct spi_nor_flash_secr_otp_ops *secr;
};

struct spi_nor_flash_part_ops {
	const struct spi_nor_flash_part_otp_ops *otp;

	ufprog_status (*chip_setup)(struct spi_nor *snor);
	ufprog_status (*select_die)(struct spi_nor *snor, uint8_t id);
	ufprog_status (*write_addr_high_byte)(struct spi_nor *snor, uint8_t addr_byte);
	ufprog_status (*setup_dpi)(struct spi_nor *snor, bool enabled);
	ufprog_status (*setup_qpi)(struct spi_nor *snor, bool enabled);
	ufprog_status (*read_uid)(struct spi_nor *snor, void *data, uint32_t *retlen);

	ufprog_status (*quad_enable)(struct spi_nor *snor);
	ufprog_status (*a4b_en)(struct spi_nor *snor);
	ufprog_status (*a4b_dis)(struct spi_nor *snor);
	ufprog_status (*dpi_en)(struct spi_nor *snor);
	ufprog_status (*dpi_dis)(struct spi_nor *snor);
	ufprog_status (*qpi_en)(struct spi_nor *snor);
	ufprog_status (*qpi_dis)(struct spi_nor *snor);
	ufprog_status (*soft_reset)(struct spi_nor *snor);
};

#define SNOR_OPS(_ops)				.ops = (_ops)

struct spi_nor_flash_part_alias_item {
	const struct spi_nor_vendor *vendor;
	const char *model;
};

#define SNOR_ALIAS_VENDOR_MODEL(_vendor, _model)	{ .vendor = (_vendor), .model = (_model) }
#define SNOR_ALIAS_MODEL(_model)			{ .model = (_model) }

struct spi_nor_flash_part_alias {
	uint32_t num;
	struct spi_nor_flash_part_alias_item items[];
};

#define DEFINE_SNOR_ALIAS(_name, ...)								\
	const struct spi_nor_flash_part_alias _name = {						\
		.items = { __VA_ARGS__ },							\
		.num = sizeof((const struct spi_nor_flash_part_alias_item[]){ __VA_ARGS__ }) /	\
		       sizeof(struct spi_nor_flash_part_alias_item) }

#define SNOR_ALIAS(_alias)			.alias = (_alias)

struct spi_nor_flash_part {
	const char *model;
	const struct spi_nor_flash_part_alias *alias;
	struct spi_nor_id id;
	const uint8_t *id_mask;
	uint32_t flags;
	uint32_t vendor_flags;

	enum snor_quad_en_type qe_type;
	enum snor_qpi_en_type qpi_en_type;
	enum snor_qpi_dis_type qpi_dis_type;

	uint32_t a4b_flags;
	enum snor_4b_en_type a4b_en_type;
	enum snor_4b_dis_type a4b_dis_type;

	uint32_t soft_reset_flags;

	uint32_t max_speed_spi_mhz;
	uint32_t max_speed_dual_mhz;
	uint32_t max_speed_quad_mhz;

	uint64_t size;
	uint32_t ndies;
	uint32_t page_size;
	uint32_t max_pp_time_us;

	const struct spi_nor_erase_info *erase_info_3b;
	const struct spi_nor_erase_info *erase_info_4b;

	uint32_t read_io_caps;
	const struct spi_nor_io_opcode *read_opcodes_3b;
	const struct spi_nor_io_opcode *read_opcodes_4b;

	uint32_t pp_io_caps;
	const struct spi_nor_io_opcode *pp_opcodes_3b;
	const struct spi_nor_io_opcode *pp_opcodes_4b;

	const struct snor_reg_info *regs;
	const struct spi_nor_otp_info *otp;
	const struct spi_nor_wp_info *wp_ranges;

	const struct spi_nor_flash_part_ops *ops;
	const struct spi_nor_flash_part_fixup *fixups;

	uint32_t ext_id_flags;
};

struct spi_nor_flash_part_blank {
	struct spi_nor_flash_part p;

	char model[SNOR_VENDOR_MODEL_LEN];

	struct spi_nor_erase_info erase_info_3b;
	struct spi_nor_erase_info erase_info_4b;

	struct spi_nor_io_opcode read_opcodes_3b[__SPI_MEM_IO_MAX];
	struct spi_nor_io_opcode read_opcodes_4b[__SPI_MEM_IO_MAX];

	struct spi_nor_io_opcode pp_opcodes_3b[__SPI_MEM_IO_MAX];
	struct spi_nor_io_opcode pp_opcodes_4b[__SPI_MEM_IO_MAX];
};

#define SNOR_PART(_model, _id, _size, ...)	{ .model = (_model), .id = _id, .size = _size, __VA_ARGS__ }

/* Predefined opcode tables */
extern const struct spi_nor_io_opcode default_read_opcodes_3b[];
extern const struct spi_nor_io_opcode default_read_opcodes_4b[];
extern const struct spi_nor_io_opcode default_pp_opcodes_3b[];
extern const struct spi_nor_io_opcode default_pp_opcodes_4b[];

extern const struct spi_nor_erase_info default_erase_opcodes_3b;
extern const struct spi_nor_erase_info default_erase_opcodes_4b;

/* Predefined register field values */
extern const struct spi_nor_reg_field_values reg_field_values_yes_no;
extern const struct spi_nor_reg_field_values reg_field_values_yes_no_rev;
extern const struct spi_nor_reg_field_values reg_field_values_true_false;
extern const struct spi_nor_reg_field_values reg_field_values_true_false_rev;
extern const struct spi_nor_reg_field_values reg_field_values_on_off;
extern const struct spi_nor_reg_field_values reg_field_values_on_off_rev;
extern const struct spi_nor_reg_field_values reg_field_values_enabled_disabled;
extern const struct spi_nor_reg_field_values reg_field_values_enabled_disabled_rev;

void spi_nor_prepare_blank_part(struct spi_nor_flash_part_blank *bp, const struct spi_nor_flash_part *refpart);
void spi_nor_blank_part_fill_default_opcodes(struct spi_nor_flash_part_blank *bp);

bool spi_nor_id_match(const uint8_t *id1, const uint8_t *id2, const uint8_t *mask, uint32_t len);

const struct spi_nor_flash_part *spi_nor_find_part(const struct spi_nor_flash_part *parts, size_t count,
						   const uint8_t *id);
const struct spi_nor_flash_part *spi_nor_find_part_by_name(const struct spi_nor_flash_part *parts, size_t count,
							   const char *model,
							   const struct spi_nor_vendor **ret_alias_vendor);

#endif /* _UFPROG_SPI_NOR_PART_H_ */
