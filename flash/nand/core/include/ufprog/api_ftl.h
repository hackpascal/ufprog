/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Simple Flash Translation Layer (FTL) driver interface definitions
 */
#pragma once

#ifndef _UFPROG_API_FTL_H_
#define _UFPROG_API_FTL_H_

#include <ufprog/bits.h>
#include <ufprog/config.h>
#include <ufprog/api_plugin.h>

EXTERN_C_BEGIN

#define FTL_DRIVER_API_VERSION_MAJOR		1
#define FTL_DRIVER_API_VERSION_MINOR		0

struct nand_chip;
struct ufprog_ftl_instance;

struct ufprog_ftl_part {
	uint32_t base_block;
	uint32_t block_count;
};

struct ufprog_ftl_callback {
	ufprog_status (*pre)(struct ufprog_ftl_callback *cb, uint32_t requested_count);
	ufprog_status (*post)(struct ufprog_ftl_callback *cb, uint32_t actual_count);
	void *buffer;
};

#define API_NAME_FTL_CREATE_INSTANCE		"ufprog_ftl_create_instance"
typedef ufprog_status (UFPROG_API *api_ftl_create_instance)(struct nand_chip *nand, struct json_object *config,
							    struct ufprog_ftl_instance **outinst);

#define API_NAME_FTL_FREE_INSTANCE		"ufprog_ftl_free_instance"
typedef ufprog_status (UFPROG_API *api_ftl_free_instance)(struct ufprog_ftl_instance *inst);

#define API_NAME_FTL_GET_SIZE			"ufprog_ftl_get_size"
typedef uint64_t (UFPROG_API *api_ftl_get_size)(struct ufprog_ftl_instance *inst);

#define API_NAME_FTL_READ_PAGE			"ufprog_ftl_read_page"
typedef ufprog_status (UFPROG_API *api_ftl_read_page)(struct ufprog_ftl_instance *inst,
						      const struct ufprog_ftl_part *part, uint32_t page, void *buf,
						      ufprog_bool raw);

#define API_NAME_FTL_READ_PAGES			"ufprog_ftl_read_pages"
typedef ufprog_status (UFPROG_API *api_ftl_read_pages)(struct ufprog_ftl_instance *inst,
						       const struct ufprog_ftl_part *part, uint32_t page,
						       uint32_t count, void *buf, ufprog_bool raw, uint32_t flags,
						       uint32_t *retcount, struct ufprog_ftl_callback *cb);

#define API_NAME_FTL_WRITE_PAGE			"ufprog_ftl_write_page"
typedef ufprog_status (UFPROG_API *api_ftl_write_page)(struct ufprog_ftl_instance *inst,
						       const struct ufprog_ftl_part *part, uint32_t page,
						       const void *buf, ufprog_bool raw);

#define API_NAME_FTL_WRITE_PAGES		"ufprog_ftl_write_pages"
typedef ufprog_status (UFPROG_API *api_ftl_write_pages)(struct ufprog_ftl_instance *inst,
							const struct ufprog_ftl_part *part, uint32_t page,
							uint32_t count, const void *buf, ufprog_bool raw,
							ufprog_bool ignore_error, uint32_t *retcount,
							struct ufprog_ftl_callback *cb);

#define API_NAME_FTL_ERASE_BLOCK		"ufprog_ftl_erase_block"
typedef ufprog_status (UFPROG_API *api_ftl_erase_block)(struct ufprog_ftl_instance *inst,
							const struct ufprog_ftl_part *part, uint32_t page,
							ufprog_bool spread);

#define API_NAME_FTL_ERASE_BLOCKS		"ufprog_ftl_erase_blocks"
typedef ufprog_status (UFPROG_API *api_ftl_erase_blocks)(struct ufprog_ftl_instance *inst,
							 const struct ufprog_ftl_part *part, uint32_t block,
							 uint32_t count, ufprog_bool spread, uint32_t *retcount,
							 struct ufprog_ftl_callback *cb);

#define API_NAME_FTL_BLOCK_CHECK_BAD		"ufprog_ftl_block_checkbad"
typedef ufprog_status (UFPROG_API *api_ftl_block_checkbad)(struct ufprog_ftl_instance *inst, uint32_t block);

#define API_NAME_FTL_BLOCK_MARK_BAD		"ufprog_ftl_block_markbad"
typedef ufprog_status(UFPROG_API *api_ftl_block_markbad)(struct ufprog_ftl_instance *inst, uint32_t block);

EXTERN_C_END

#endif /* _UFPROG_API_FTL_H_ */
