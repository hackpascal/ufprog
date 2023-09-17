/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash core definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NAND_CORE_H_
#define _UFPROG_SPI_NAND_CORE_H_

#include <stdint.h>
#include <stdbool.h>
#include <ufprog/spi.h>
#include <ufprog/spi-nand.h>
#include <ufprog/spi-nand-opcode.h>
#include <ufprog/onfi-param-page.h>
#include <ecc-internal.h>
#include "vendor.h"
#include "part.h"

#define SNAND_ID_PROBE_RETRIES			3

#define SNAND_SPEED_LOW				10000000
#define SNAND_SPEED_HIGH			60000000

#define SNAND_RESET_WAIT_US			1000000
#define SNAND_POLL_MAX_US			5000000
#define SNAND_POLL_WARN_US			1000000

#define SNAND_MAX_DIES				4

#define SNAND_SINGLE_OP(_opcode)				\
	SPI_MEM_OP(						\
		SPI_MEM_OP_CMD(_opcode, 1),			\
		SPI_MEM_OP_NO_ADDR,				\
		SPI_MEM_OP_NO_DUMMY,				\
		SPI_MEM_OP_NO_DATA				\
	);

#define SNAND_READ_ID_OP(_len, _ndummy, _id)			\
	SPI_MEM_OP(						\
		SPI_MEM_OP_CMD(SNAND_CMD_READID, 1),		\
		SPI_MEM_OP_NO_ADDR,				\
		SPI_MEM_OP_DUMMY(_ndummy, 1),			\
		SPI_MEM_OP_DATA_IN(_len, _id, 1)		\
	);

#define SNAND_READ_ID_ADDR_OP(_len, _addr, _id)			\
	SPI_MEM_OP(						\
		SPI_MEM_OP_CMD(SNAND_CMD_READID, 1),		\
		SPI_MEM_OP_ADDR(1, _addr, 1),			\
		SPI_MEM_OP_NO_DUMMY,				\
		SPI_MEM_OP_DATA_IN(_len, _id, 1)		\
	);

#define SNAND_GET_FEATURE_OP(_addr, _data)			\
	SPI_MEM_OP(						\
		SPI_MEM_OP_CMD(SNAND_CMD_GET_FEATURE, 1),	\
		SPI_MEM_OP_ADDR(1, _addr, 1),			\
		SPI_MEM_OP_NO_DUMMY,				\
		SPI_MEM_OP_DATA_IN(1, _data, 1)			\
	);

#define SNAND_SET_FEATURE_OP(_addr, _data)			\
	SPI_MEM_OP(						\
		SPI_MEM_OP_CMD(SNAND_CMD_SET_FEATURE, 1),	\
		SPI_MEM_OP_ADDR(1, _addr, 1),			\
		SPI_MEM_OP_NO_DUMMY,				\
		SPI_MEM_OP_DATA_OUT(1, _data, 1)		\
	);

#define SNAND_SELECT_DIE_OP(_dieidx)				\
	SPI_MEM_OP(						\
		SPI_MEM_OP_CMD(SNAND_CMD_SELECT_DIE, 1),	\
		SPI_MEM_OP_NO_ADDR,				\
		SPI_MEM_OP_NO_DUMMY,				\
		SPI_MEM_OP_DATA_OUT(1, _dieidx, 1)		\
	);

#define SNAND_PAGE_OP(_cmd, _addr)				\
	SPI_MEM_OP(						\
		SPI_MEM_OP_CMD(_cmd, 1),			\
		SPI_MEM_OP_ADDR(3, _addr, 1),			\
		SPI_MEM_OP_NO_DUMMY,				\
		SPI_MEM_OP_NO_DATA				\
	);

struct spi_nand_param {
	char vendor[NAND_VENDOR_MODEL_LEN];
	char model[NAND_VENDOR_MODEL_LEN];
	uint8_t onfi[ONFI_PARAM_PAGE_SIZE];

	uint32_t flags;
	uint32_t vendor_flags;
	uint32_t max_speed;

	uint32_t max_pp_time_us;
	uint32_t max_be_time_us;
	uint32_t max_r_time_us;
};

struct spi_nand_ext_param {
	struct spi_nand_flash_part_ops ops;
};

struct spi_nand_onfi {
	uint8_t data[ONFI_PARAM_PAGE_SIZE];
	bool valid;
};

struct spi_nand_state {
	uint32_t speed_low;
	uint32_t speed_high;

	uint8_t curr_die;

	struct spi_nand_io_opcode rd_opcode;
	uint32_t rd_io_info;

	struct spi_nand_io_opcode pl_opcode;
	struct spi_nand_io_opcode upd_opcode;
	uint32_t pl_io_info;

	uint8_t cfg[SNAND_MAX_DIES];

	bool ecc_enabled;
	bool ecc_warn_once;
	uint32_t ecc_steps;

	uint8_t seq_rd_feature_addr;
	uint8_t seq_rd_crbsy_mask;
};

struct spi_nand {
	struct nand_chip nand;
	struct ufprog_nand_ecc_chip ecc;

	struct ufprog_spi *spi;
	uint32_t max_speed;
	uint32_t allowed_io_caps;
	uint32_t config;

	struct spi_nand_onfi onfi;
	struct spi_nand_param param;
	struct spi_nand_ext_param ext_param;

	struct spi_nand_state state;

	ufprog_status ecc_ret;
	struct nand_ecc_status *ecc_status;

	uint8_t *scratch_buffer;
};

ufprog_status spi_nand_set_low_speed(struct spi_nand *snand);
ufprog_status spi_nand_set_high_speed(struct spi_nand *snand);

ufprog_status spi_nand_issue_single_opcode(struct spi_nand *snand, uint8_t opcode);
ufprog_status spi_nand_get_feature(struct spi_nand *snand, uint32_t addr, uint8_t *retval);
ufprog_status spi_nand_set_feature(struct spi_nand *snand, uint32_t addr, uint8_t val);

ufprog_status spi_nand_read_status(struct spi_nand *snand, uint8_t *retval);
ufprog_status spi_nand_wait_busy(struct spi_nand *snand, uint32_t wait_ms, uint8_t *retsr);
uint8_t spi_nand_get_config(struct spi_nand *snand);
ufprog_status spi_nand_update_config(struct spi_nand *snand, uint8_t clr, uint8_t set);
ufprog_status spi_nand_ondie_ecc_control(struct spi_nand *snand, bool enable);
ufprog_status spi_nand_otp_control(struct spi_nand *snand, bool enable);

ufprog_status spi_nand_write_enable(struct spi_nand *snand);
ufprog_status spi_nand_write_disable(struct spi_nand *snand);

ufprog_status spi_nand_select_die_c2h(struct spi_nand *snand, uint32_t dieidx);

void spi_nand_reset_ecc_status(struct spi_nand *snand);

bool spi_nand_probe_onfi_generic(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp, uint32_t page,
				 bool fill_all);

ufprog_status spi_nand_reprobe_part(struct spi_nand *snand, struct spi_nand_flash_part_blank *bp,
				    const struct spi_nand_vendor *vendor, const char *part);

ufprog_status spi_nand_page_op(struct spi_nand *snand, uint32_t page, uint8_t cmd);

ufprog_status spi_nand_read_cache_custom(struct spi_nand *snand, const struct spi_nand_io_opcode *opcode,
					 uint32_t io_info, uint32_t column, uint32_t len, void *data);
ufprog_status spi_nand_read_cache_single(struct spi_nand *snand, uint32_t column, uint32_t len, void *data);

ufprog_status spi_nand_program_load_custom(struct spi_nand *snand, const struct spi_nand_io_opcode *pl_opcode,
					   const struct spi_nand_io_opcode *upd_opcode, uint32_t io_info,
					   uint32_t column, uint32_t len, const void *data);
ufprog_status spi_nand_program_load_single(struct spi_nand *snand, uint32_t column, uint32_t len, const void *data);

ufprog_status spi_nand_read_uid_otp(struct spi_nand *snand, uint32_t page, void *data, uint32_t *retlen);

#endif /* _UFPROG_SPI_NAND_CORE_H_ */
