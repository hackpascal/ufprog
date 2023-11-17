/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash core definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NOR_CORE_H_
#define _UFPROG_SPI_NOR_CORE_H_

#include <stdint.h>
#include <stdbool.h>
#include <ufprog/spi.h>
#include <ufprog/spi-nor.h>
#include "vendor.h"
#include "sfdp.h"
#include "regs.h"

#define SNOR_ID_READ_RETRIES			3

#define SNOR_SPEED_LOW				10000000
#define SNOR_SPEED_HIGH				60000000

#define SNOR_PP_TIMEOUT_MS			1000
#define SNOR_ERASE_TIMEOUT_MS			2500
#define SNOR_RESET_WAIT_MS			25
#define SNOR_WRITE_NV_REG_TIMEOUT_MS		100

#define SNOR_DFL_PAGE_SIZE			256

#define SNOR_READ_NO_ADDR_DUMMY_OP(_opcode, _bw, _len, _data)	\
	SPI_MEM_OP(						\
		SPI_MEM_OP_CMD(_opcode, _bw),			\
		SPI_MEM_OP_NO_ADDR,				\
		SPI_MEM_OP_NO_DUMMY,				\
		SPI_MEM_OP_DATA_IN(_len, _data, _bw)		\
	);

#define SNOR_READ_ID_OP(_opcode, _bw, _len, _ndummy, _id)	\
	SPI_MEM_OP(						\
		SPI_MEM_OP_CMD(_opcode, _bw),			\
		SPI_MEM_OP_NO_ADDR,				\
		SPI_MEM_OP_DUMMY(_ndummy, _bw),			\
		SPI_MEM_OP_DATA_IN(_len, _id, _bw)		\
	);

#define SNOR_READ_SFDP_OP(_bw, _addr, _naddr, _len, _data)	\
	SPI_MEM_OP(						\
		SPI_MEM_OP_CMD(SNOR_CMD_READ_SFDP, _bw),	\
		SPI_MEM_OP_ADDR(_naddr, _addr, _bw),		\
		SPI_MEM_OP_DUMMY(_bw, _bw),			\
		SPI_MEM_OP_DATA_IN(_len, _data, _bw)		\
	);

#define SNOR_WRITE_NO_ADDR_DUMMY_OP(_opcode, _bw, _len, _data)	\
	SPI_MEM_OP(						\
		SPI_MEM_OP_CMD(_opcode, _bw),			\
		SPI_MEM_OP_NO_ADDR,				\
		SPI_MEM_OP_NO_DUMMY,				\
		SPI_MEM_OP_DATA_OUT(_len, _data, _bw)		\
	);

#define SNOR_ERASE_REGION(_region_size, _min_erasesize, _max_erasesize, _eraseop_mask)	\
	{ .size = (_region_size), .min_erasesize = (_min_erasesize), .max_erasesize = (_max_erasesize), \
	  .erasesizes_mask = (_eraseop_mask) }

struct spi_nor_param {
	struct spi_nor_id id;

	char vendor[SNOR_VENDOR_MODEL_LEN];
	char model[SNOR_VENDOR_MODEL_LEN];

	uint32_t flags;
	uint32_t vendor_flags;
	uint32_t max_speed;

	uint64_t size;
	uint32_t ndies;
	uint32_t page_size;
	uint32_t max_pp_time_ms;
	uint8_t naddr;

	struct spi_nor_erase_info erase_info;
};

struct spi_nor_ext_param {
	struct spi_nor_flash_part_ops ops;

	const struct spi_nor_otp_info *otp;
	const struct snor_reg_info *regs;
	const struct spi_nor_wp_info *wp_ranges;
	const struct spi_nor_reg_access *wp_regacc;

	struct spi_nor_erase_region *erase_regions;
	uint32_t num_erase_regions;

	ufprog_status (*data_write_enable)(struct spi_nor *snor);
	ufprog_status (*pre_read_hook)(struct spi_nor *snor, uint64_t addr, size_t len, void *data);
	ufprog_status (*write_page)(struct spi_nor *snor, uint64_t addr, size_t len, const void *data, size_t *retlen);
};

struct spi_nor_reg_param {
	const struct spi_nor_reg_access *sr_r;
	const struct spi_nor_reg_access *sr_w;
	const struct spi_nor_reg_access *cr;
	uint32_t cr_shift;
};

struct spi_nor_state {
	uint32_t flags;
	uint32_t vendor_flags;

	uint32_t curr_die;
	uint32_t curr_high_addr;
	uint32_t die_read_granularity;

	uint32_t speed_low;
	uint32_t speed_high;

	uint8_t cmd_buswidth;
	uint8_t cmd_buswidth_curr;

	uint8_t naddr;

	uint8_t read_opcode;
	uint8_t read_ndummy;
	uint32_t read_io_info;

	uint8_t pp_opcode;
	uint32_t pp_io_info;

	uint32_t max_nvcr_pp_time_ms;

	bool qe_set;
	bool a4b_mode;

	struct spi_nor_reg_param reg;
};

struct spi_nor {
	struct ufprog_spi *spi;
	uint32_t max_speed;
	uint32_t allowed_io_caps;

	struct spi_nor_sfdp sfdp;
	struct spi_nor_param param;
	struct spi_nor_ext_param ext_param;

	struct spi_nor_state state;

	struct spi_nor_wp_regions *wp_regions;
	struct spi_nor_erase_region uniform_erase_region;
};

ufprog_status spi_nor_set_low_speed(struct spi_nor *snor);
ufprog_status spi_nor_set_high_speed(struct spi_nor *snor);

ufprog_status spi_nor_read_reg(struct spi_nor *snor, uint8_t regopcode, uint8_t *retval);
ufprog_status spi_nor_write_reg(struct spi_nor *snor, uint8_t regopcode, uint8_t val);
ufprog_status spi_nor_issue_single_opcode(struct spi_nor *snor, uint8_t opcode);

ufprog_status spi_nor_sr_write_enable(struct spi_nor *snor, bool volatile_write, bool *retpoll);
ufprog_status spi_nor_read_sr(struct spi_nor *snor, uint8_t *retval);
ufprog_status spi_nor_write_sr(struct spi_nor *snor, uint8_t val, bool volatile_write);

ufprog_status spi_nor_write_enable(struct spi_nor *snor);
ufprog_status spi_nor_data_write_enable(struct spi_nor *snor);
ufprog_status spi_nor_volatile_write_enable(struct spi_nor *snor);
ufprog_status spi_nor_write_disable(struct spi_nor *snor);

ufprog_status spi_nor_disable_qpi_ffh(struct spi_nor *snor);
ufprog_status spi_nor_disable_qpi_f5h(struct spi_nor *snor);
ufprog_status spi_nor_disable_qpi_800003h(struct spi_nor *snor);
ufprog_status spi_nor_disable_qpi_66h_99h(struct spi_nor *snor);

ufprog_status spi_nor_enable_4b_addressing_b7h(struct spi_nor *snor);
ufprog_status spi_nor_disable_4b_addressing_e9h(struct spi_nor *snor);

ufprog_status spi_nor_quad_enable_any(struct spi_nor *snor, const struct spi_nor_reg_access *regacc, uint32_t bit);

ufprog_status spi_nor_quad_enable(struct spi_nor *snor);
ufprog_status spi_nor_setup_addr(struct spi_nor *snor, uint64_t *addr);
ufprog_status spi_nor_4b_addressing_control(struct spi_nor *snor, bool enable);
ufprog_status spi_nor_dpi_control(struct spi_nor *snor, bool enable);
ufprog_status spi_nor_qpi_control(struct spi_nor *snor, bool enable);
ufprog_status spi_nor_chip_soft_reset(struct spi_nor *snor);
ufprog_status spi_nor_select_die(struct spi_nor *snor, uint8_t id);
ufprog_status spi_nor_set_bus_width(struct spi_nor *snor, uint8_t buswidth);

ufprog_status spi_nor_wait_busy(struct spi_nor *snor, uint32_t wait_ms);

bool spi_nor_test_io_opcode(struct spi_nor *snor, const struct spi_nor_io_opcode *opcodes, enum spi_mem_io_type io_type,
			    uint8_t naddr, enum ufprog_spi_data_dir data_dir);

void spi_nor_gen_erase_info(const struct spi_nor_flash_part *part, const struct spi_nor_erase_info *src,
			    struct spi_nor_erase_info *retei);

void spi_nor_fill_erase_region_erasesizes(struct spi_nor *snor, struct spi_nor_erase_region *erg, uint64_t region_size);

ufprog_status spi_nor_reprobe_part(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				   struct spi_nor_flash_part_blank *bp, const struct spi_nor_vendor *vendor,
				   const char *part);

#endif /* _UFPROG_SPI_NOR_CORE_H_ */
