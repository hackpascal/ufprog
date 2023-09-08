/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash register access definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NOR_REGS_H_
#define _UFPROG_SPI_NOR_REGS_H_

#include <ufprog/spi-nor.h>

#define SNOR_MAX_REG_DESC				4

#define SNOR_REGACC_F_BIG_ENDIAN			BIT(0)
#define SNOR_REGACC_F_NO_WREN				BIT(1)
#define SNOR_REGACC_F_VOLATILE_WREN_50H			BIT(2)
#define SNOR_REGACC_F_HAS_VOLATILE_WR_OPCODE		BIT(3)
#define SNOR_REGACC_F_ADDR_4B_MODE			BIT(4)

enum snor_reg_access_type {
	SNOR_REG_NORMAL,
	SNOR_REG_READ_MULTI_WRITE_ONCE,

	__MAX_SNOR_REG_ACCESS_TYPE
};

struct spi_nor_reg_desc {
	uint32_t flags;
	uint8_t read_opcode;
	uint8_t write_opcode;
	uint8_t write_opcode_volatile;
	uint8_t naddr;
	uint8_t ndummy_read;
	uint8_t ndummy_write;
	uint8_t ndata;
	uint32_t addr;
};

struct spi_nor_reg_access {
	enum snor_reg_access_type type;
	bool read_big_endian;
	bool write_big_endian;
	uint32_t num;
	struct spi_nor_reg_desc desc[SNOR_MAX_REG_DESC];

	ufprog_status (*pre_acc)(struct spi_nor *snor, const struct spi_nor_reg_access *access);
	ufprog_status (*post_acc)(struct spi_nor *snor, const struct spi_nor_reg_access *access);
};

#define SNOR_REG_ACC_NORMAL(_read_opcode, _write_opcode)						\
	{ .type = SNOR_REG_NORMAL, .num = 1,								\
	  .desc[0] = { .ndata = 1, .read_opcode = (_read_opcode), .write_opcode = (_write_opcode), },	\
	}

#define SNOR_REG_ACC_SRCR(_read_opcode, _read_opcode2, _write_opcode)					\
	{ .type = SNOR_REG_READ_MULTI_WRITE_ONCE, .num = 2,						\
	  .desc[0] = { .ndata = 1, .read_opcode = (_read_opcode), .write_opcode = (_write_opcode), },	\
	  .desc[1] = { .ndata = 1, .read_opcode = (_read_opcode2), },					\
	}

#define VALUE_ITEM(_val, _name)										\
	{ .value = (_val), .name = (_name) }

#define SNOR_REG_FIELD_VALUES(...)									\
	{ .items = { __VA_ARGS__ },									\
	  .num = sizeof((struct spi_nor_reg_field_value_item[]){ __VA_ARGS__ }) /			\
		 sizeof(struct spi_nor_reg_field_value_item) }

#define SNOR_REG_FIELD_FULL(_shift, _mask, _name, _desc, _values)					\
	{ .name = (_name), .desc = (_desc), .shift = (_shift), .mask = (_mask), .values = (_values) }

#define SNOR_REG_FIELD(_shift, _mask, _name, _desc)							\
	SNOR_REG_FIELD_FULL(_shift, _mask, _name, _desc, NULL)

#define SNOR_REG_FIELD_YES_NO(_shift, _mask, _name, _desc)						\
	SNOR_REG_FIELD_FULL(_shift, _mask, _name, _desc, &reg_field_values_yes_no)

#define SNOR_REG_FIELD_YES_NO_REV(_shift, _mask, _name, _desc)						\
	SNOR_REG_FIELD_FULL(_shift, _mask, _name, _desc, &reg_field_values_yes_no_rev)

#define SNOR_REG_FIELD_TRUE_FALSE(_shift, _mask, _name, _desc)						\
	SNOR_REG_FIELD_FULL(_shift, _mask, _name, _desc, &reg_field_values_true_false)

#define SNOR_REG_FIELD_TRUE_FALSE_REV(_shift, _mask, _name, _desc)					\
	SNOR_REG_FIELD_FULL(_shift, _mask, _name, _desc, &reg_field_values_true_false_rev)

#define SNOR_REG_FIELD_ON_OFF(_shift, _mask, _name, _desc)						\
	SNOR_REG_FIELD_FULL(_shift, _mask, _name, _desc, &reg_field_values_on_off)

#define SNOR_REG_FIELD_ON_OFF_REV(_shift, _mask, _name, _desc)						\
	SNOR_REG_FIELD_FULL(_shift, _mask, _name, _desc, &reg_field_values_on_off_rev)

#define SNOR_REG_FIELD_ENABLED_DISABLED(_shift, _mask, _name, _desc)					\
	SNOR_REG_FIELD_FULL(_shift, _mask, _name, _desc, &reg_field_values_enabled_disabled)

#define SNOR_REG_FIELD_ENABLED_DISABLED_REV(_shift, _mask, _name, _desc)				\
	SNOR_REG_FIELD_FULL(_shift, _mask, _name, _desc, &reg_field_values_enabled_disabled_rev)

#define SNOR_REG_DEF(_name, _desc, _acc, _fields)							\
	{ .name = (_name), .desc = (_desc), .access = (_acc), .nfields = ARRAY_SIZE(_fields), .fields = (_fields) }

#define SNOR_REG_INFO(...)										\
	{ .regs = { __VA_ARGS__ },									\
	  .num = sizeof((const struct spi_nor_reg_def*[]){ __VA_ARGS__ }) / sizeof(struct spi_nor_reg_def*) }

extern const struct spi_nor_reg_access sr_acc;
extern const struct spi_nor_reg_access cr_acc;
extern const struct spi_nor_reg_access sr3_acc;
extern const struct spi_nor_reg_access srcr_acc;
extern const struct spi_nor_reg_access ear_acc;
extern const struct spi_nor_reg_access br_acc;
extern const struct spi_nor_reg_access scur_acc;

extern const struct snor_reg_info w25q_no_lb_regs;
extern const struct snor_reg_info w25q_regs;

extern const struct spi_nor_reg_def w25q_sr1;
extern const struct spi_nor_reg_def w25q_sr2;
extern const struct spi_nor_reg_def w25q_sr3;

extern const struct spi_nor_reg_field_values w25q_sr3_drv_values;
extern const struct spi_nor_reg_field_values w25q_sr3_hold_rst_values;
extern const struct spi_nor_reg_field_values w25q_sr3_adp_values;
extern const struct spi_nor_reg_field_values w25q_sr3_wps_values;

ufprog_status spi_nor_read_reg_acc(struct spi_nor *snor, const struct spi_nor_reg_access *access, uint32_t *retval);
ufprog_status spi_nor_write_reg_acc(struct spi_nor *snor, const struct spi_nor_reg_access *access, uint32_t val,
				    bool volatile_write);
ufprog_status spi_nor_update_reg_acc(struct spi_nor *snor, const struct spi_nor_reg_access *access, uint32_t clr,
				     uint32_t set, bool volatile_write);

#endif /* _UFPROG_SPI_NOR_REGS_H_ */
