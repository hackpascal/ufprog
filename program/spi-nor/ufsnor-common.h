/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash programmer common part
 */
#pragma once

#ifndef _UFSNOR_COMMON_H_
#define _UFSNOR_COMMON_H_

#include <limits.h>
#include <stdbool.h>
#include <inttypes.h>
#include <ufprog/log.h>
#include <ufprog/cmdarg.h>
#include <ufprog/spi.h>
#include <ufprog/spi-nor.h>

#define UFSNOR_MAX_SPEED				80000000
#define UFSNOR_READ_GRANULARITY				0x10000
#define UFSNOR_WRITE_GRANULARITY			0x200

struct ufsnor_options {
	uint32_t log_level;
	char *last_device;
	uint32_t global_max_speed;
	uint32_t max_speed;
};

struct ufsnor_instance {
	struct ufprog_spi *spi;
	struct spi_nor *snor;
	struct spi_nor_info info;
	size_t max_read_granularity;
	uint32_t speed;
	uint32_t max_speed;
	uint32_t die_start;
	uint32_t die_count;
};

bool parse_args(struct cmdarg_entry *entries, uint32_t count, int argc, char *argv[], int *next_argc);

ufprog_status load_config(struct ufsnor_options *retcfg, const char *curr_device);
ufprog_status save_config(const struct ufsnor_options *cfg);

ufprog_status open_device(const char *device_name, const char *part, uint32_t max_speed,
			  struct ufsnor_instance *retinst, bool allow_fail);
ufprog_status read_flash(struct ufsnor_instance *inst, uint64_t addr, uint64_t size, void *buf);
ufprog_status dump_flash(struct ufsnor_instance *inst, uint64_t addr, uint64_t size);
ufprog_status verify_flash(struct ufsnor_instance *inst, uint64_t addr, uint64_t size, const void *buf);
ufprog_status erase_flash(struct ufsnor_instance *inst, uint64_t addr, uint64_t size);
ufprog_status write_flash_no_erase(struct ufsnor_instance *inst, uint64_t addr, uint64_t size, const void *buf,
				   bool verify);
ufprog_status write_flash(struct ufsnor_instance *inst, uint64_t addr, size_t size, const void *buf, bool update,
			  bool verify);

#endif /* _UFSNOR_COMMON_H_ */
