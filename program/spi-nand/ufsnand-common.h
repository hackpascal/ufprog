/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash programmer common part
 */
#pragma once

#ifndef _UFSNAND_COMMON_H_
#define _UFSNAND_COMMON_H_

#include <limits.h>
#include <stdbool.h>
#include <inttypes.h>
#include <ufprog/log.h>
#include <ufprog/cmdarg.h>
#include <ufprog/progbar.h>
#include <ufprog/spi.h>
#include <ufprog/spi-nand.h>
#include <ufprog/ecc.h>
#include <ufprog/ecc-driver.h>
#include <ufprog/bbt-driver.h>
#include <ufprog/bbt-ram.h>
#include <ufprog/ftl-basic.h>
#include <ufprog/ftl-driver.h>

#define UFSNAND_MAX_SPEED				80000000

struct ufsnand_options {
	uint32_t log_level;
	char *last_device;
	uint32_t global_max_speed;
	uint32_t max_speed;
};

struct ufnand_progress_status {
	uint32_t total;
	uint32_t current;
	uint32_t last_percentage;
};

struct ufnand_instance {
	struct nand_chip *chip;
	struct ufprog_nand_bbt *bbt;
	struct ufprog_nand_ftl *ftl;
	struct nand_info info;
	uint64_t ftl_size;
	bool bbt_used;
};

struct ufnand_op_data {
	const struct nand_page_layout *layout;
	bool layout_needs_free;
	uint32_t page_size;
	uint8_t *buf[2];
	uint8_t *map;
	uint8_t *tmp;
};

struct ufnand_rwe_data {
	struct ufprog_ftl_part part;
	ufprog_bool part_set;
	ufprog_bool nospread;
	ufprog_bool verify;
	ufprog_bool erase;
	ufprog_bool raw;
	ufprog_bool oob;
	ufprog_bool fmt;
};

struct ufsnand_instance {
	struct ufnand_instance nand;

	struct ufprog_spi *spi;
	struct spi_nand *snand;
	struct ufprog_nand_ecc_chip *ecc;
	struct spi_nand_info sinfo;
	uint32_t speed;
	uint32_t max_speed;
};

struct ufnand_ftl_callback {
	struct ufprog_ftl_callback cb;
	struct ufnand_progress_status prog;
	struct ufnand_instance *nandinst;
	struct ufnand_rwe_data *rwedata;
	struct ufnand_op_data *opdata;
	union {
		uint8_t *rx;
		const uint8_t *tx;
	} buf;
	uint32_t page;

	bool last_batch;
	uint32_t last_page_padding;
	uint32_t count_left;
};

bool parse_args(struct cmdarg_entry *entries, uint32_t count, int argc, char *argv[], int *next_argc);

ufprog_status load_config(struct ufsnand_options *retcfg, const char *curr_device);
ufprog_status save_config(const struct ufsnand_options *cfg);

ufprog_status open_device(const char *device_name, const char *part, uint32_t max_speed,
			  struct ufsnand_instance *retinst, bool list_only);

ufprog_status open_ecc_chip(const char *ecc_cfg, uint32_t page_size, uint32_t spare_size,
			    struct ufprog_nand_ecc_chip **outecc);
ufprog_status open_bbt(const char *bbt_cfg, struct nand_chip *nand, struct ufprog_nand_bbt **outbbt);
ufprog_status open_ftl(const char *ftl_cfg, struct nand_chip *nand, struct ufprog_nand_bbt *bbt,
		       struct ufprog_nand_ftl **outftl, bool *ret_bbt_used);

uint32_t print_bbt(struct ufnand_instance *nandinst, struct ufprog_nand_bbt *bbt);

void print_speed(uint64_t size, uint64_t time_us);

ufprog_status nand_read(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata,
			const struct ufprog_ftl_part *part, struct ufnand_op_data *opdata, file_mapping fm,
			uint32_t page, uint32_t count);

ufprog_status nand_dump(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata,
			const struct ufprog_ftl_part *part, struct ufnand_op_data *opdata, uint32_t page,
			uint32_t count);

ufprog_status nand_write(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata,
			 const struct ufprog_ftl_part *part, struct ufnand_op_data *opdata, file_mapping fm,
			 uint32_t page, uint32_t count, uint32_t last_page_padding);

ufprog_status nand_verify(struct ufnand_instance *nandinst, struct ufnand_rwe_data *rwedata,
			  const struct ufprog_ftl_part *part, struct ufnand_op_data *opdata, file_mapping fm,
			  uint32_t page, uint32_t count, uint32_t last_page_padding);

ufprog_status nand_erase(struct ufnand_instance *nandinst, const struct ufprog_ftl_part *part, uint32_t page,
			 uint32_t count, bool nospread);

#endif /* _UFSNAND_COMMON_H_ */
