/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * BBT internal definitions
 */
#pragma once

#ifndef _UFPROG_NAND_BBT_INTERNAL_H_
#define _UFPROG_NAND_BBT_INTERNAL_H_

#include <ufprog/bbt-driver.h>
#include <ufprog/bbt.h>
#include <plugin-common.h>

struct ufprog_bbt_driver {
	struct plugin plugin;

	api_bbt_create_instance create_instance;
	api_bbt_free_instance free_instance;

	api_bbt_reprobe reprobe;
	api_bbt_commit commit;

	api_bbt_modify_config modify_config;
	api_bbt_get_config get_config;

	api_bbt_get_state get_state;
	api_bbt_set_state set_state;
	api_bbt_is_reserved is_reserved;

	struct ufprog_lookup_table *instances;
};

struct ufprog_nand_bbt {
	char *name;

	struct ufprog_bbt_driver *driver;
	struct ufprog_bbt_instance *instance;

	ufprog_status (*free_ni)(struct ufprog_nand_bbt *bbt);

	ufprog_status (*reprobe)(struct ufprog_nand_bbt *bbt);
	ufprog_status (*commit)(struct ufprog_nand_bbt *bbt);

	ufprog_status (*modify_config)(struct ufprog_nand_bbt *bbt, uint32_t clr, uint32_t set);
	uint32_t (*get_config)(struct ufprog_nand_bbt *bbt);

	ufprog_status (*get_state)(struct ufprog_nand_bbt *bbt, uint32_t block,
				   uint32_t /* enum nand_bbt_gen_state */ *state);
	ufprog_status (*set_state)(struct ufprog_nand_bbt *bbt, uint32_t block,
				   uint32_t /* enum nand_bbt_gen_state */ state);
	ufprog_bool (*is_reserved)(struct ufprog_nand_bbt *bbt, uint32_t block);
};

ufprog_status ufprog_bbt_add_instance(struct ufprog_bbt_driver *drv, const struct ufprog_bbt_instance *inst);
ufprog_status ufprog_bbt_remove_instance(struct ufprog_bbt_driver *drv, const struct ufprog_bbt_instance *inst);

#endif /* _UFPROG_NAND_BBT_INTERNAL_H_ */
