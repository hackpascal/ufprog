/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Simple Flash Translation Layer (FTL) internal definitions
 */
#pragma once

#ifndef _UFPROG_NAND_FTL_INTERNAL_H_
#define _UFPROG_NAND_FTL_INTERNAL_H_

#include <ufprog/ftl-driver.h>
#include <ufprog/ftl.h>
#include <plugin-common.h>

struct ufprog_ftl_driver {
	struct plugin plugin;

	api_ftl_create_instance create_instance;
	api_ftl_free_instance free_instance;

	api_ftl_get_size get_size;

	api_ftl_read_page read_page;
	api_ftl_read_pages read_pages;

	api_ftl_write_page write_page;
	api_ftl_write_pages write_pages;

	api_ftl_erase_block erase_block;
	api_ftl_erase_blocks erase_blocks;

	api_ftl_block_checkbad block_checkbad;
	api_ftl_block_markbad block_markbad;

	struct ufprog_lookup_table *instances;
};

struct ufprog_nand_ftl {
	char *name;

	struct ufprog_ftl_driver *driver;
	struct ufprog_ftl_instance *instance;

	struct nand_chip *nand;

	uint32_t ftl_total_pages;
	uint64_t size;

	ufprog_status (*free_ni)(struct ufprog_nand_ftl *ftl);

	uint64_t (*get_size)(struct ufprog_nand_ftl *ftl);

	ufprog_status (*read_page)(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part, uint32_t page,
				   void *buf, bool raw);

	ufprog_status (*read_pages)(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part, uint32_t page,
				    uint32_t count, void *buf, bool raw, uint32_t flags, uint32_t *retcount,
				    struct ufprog_ftl_callback *cb);

	ufprog_status (*write_page)(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part, uint32_t page,
				    const void *buf, bool raw);

	ufprog_status (*write_pages)(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part, uint32_t page,
				     uint32_t count, const void *buf, bool raw, bool ignore_error,
				     uint32_t *retcount, struct ufprog_ftl_callback *cb);

	ufprog_status (*erase_block)(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part, uint32_t page,
				     ufprog_bool spread);

	ufprog_status (*erase_blocks)(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
				      uint32_t block, uint32_t count, ufprog_bool spread, uint32_t *retcount,
				      struct ufprog_ftl_callback *cb);

	ufprog_status (*block_checkbad)(struct ufprog_nand_ftl *ftl, uint32_t block);

	ufprog_status (*block_markbad)(struct ufprog_nand_ftl *ftl, uint32_t block);
};

ufprog_status ufprog_ftl_add_instance(struct ufprog_ftl_driver *drv, const struct ufprog_ftl_instance *inst);
ufprog_status ufprog_ftl_remove_instance(struct ufprog_ftl_driver *drv, const struct ufprog_ftl_instance *inst);

#endif /* _UFPROG_NAND_FTL_INTERNAL_H_ */
