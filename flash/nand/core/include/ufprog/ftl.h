/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Simple Flash Translation Layer (FTL)
 */
#pragma once

#ifndef _UFPROG_FTL_H_
#define _UFPROG_FTL_H_

#include <stdint.h>
#include <stdbool.h>
#include <ufprog/api_ftl.h>
#include <ufprog/config.h>

EXTERN_C_BEGIN

struct ufprog_nand_ftl;

ufprog_status UFPROG_API ufprog_ftl_create(const char *drvname, const char *name, struct nand_chip *nand,
					   struct json_object *config, struct ufprog_nand_ftl **outftl);
ufprog_status UFPROG_API ufprog_ftl_free(struct ufprog_nand_ftl *ftl);

uint64_t UFPROG_API ufprog_ftl_get_size(struct ufprog_nand_ftl *ftl);

ufprog_status UFPROG_API ufprog_ftl_read_page(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					      uint32_t page, void *buf, ufprog_bool raw);

ufprog_status UFPROG_API ufprog_ftl_read_pages(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					       uint32_t page, uint32_t count, void *buf, ufprog_bool raw,
					       uint32_t flags, uint32_t *retcount, struct ufprog_ftl_callback *cb);

ufprog_status UFPROG_API ufprog_ftl_write_page(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					       uint32_t page, const void *buf, ufprog_bool raw);

ufprog_status UFPROG_API ufprog_ftl_write_pages(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
						uint32_t page, uint32_t count, const void *buf, ufprog_bool raw,
						ufprog_bool ignore_error, uint32_t *retcount,
						struct ufprog_ftl_callback *cb);

ufprog_status UFPROG_API ufprog_ftl_erase_block(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
						uint32_t page, ufprog_bool spread);

ufprog_status UFPROG_API ufprog_ftl_erase_blocks(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
						 uint32_t block, uint32_t count, ufprog_bool spread, uint32_t *retcount,
						 struct ufprog_ftl_callback *cb);

ufprog_status UFPROG_API ufprog_ftl_block_checkbad(struct ufprog_nand_ftl *ftl, uint32_t block);

ufprog_status UFPROG_API ufprog_ftl_block_markbad(struct ufprog_nand_ftl *ftl, uint32_t block);

EXTERN_C_END

#endif /* _UFPROG_FTL_H_ */
