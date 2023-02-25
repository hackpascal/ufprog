/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Lookup table
 */
#pragma once

#ifndef _UFPROG_LOOKUP_TABLE_
#define _UFPROG_LOOKUP_TABLE_

#include <ufprog/common.h>

EXTERN_C_BEGIN

struct ufprog_lookup_table;

typedef int (UFPROG_API *ufprog_lookup_table_entry_cb)(void *priv, struct ufprog_lookup_table *tbl, const char *key,
						       void *ptr);

ufprog_status UFPROG_API lookup_table_create(struct ufprog_lookup_table **outtbl, uint32_t init_size);
ufprog_status UFPROG_API lookup_table_destroy(struct ufprog_lookup_table *tbl);
ufprog_status UFPROG_API lookup_table_insert(struct ufprog_lookup_table *tbl, const char *key, const void *ptr);
ufprog_status UFPROG_API lookup_table_insert_ptr(struct ufprog_lookup_table *tbl, const void *ptr);
ufprog_status UFPROG_API lookup_table_delete(struct ufprog_lookup_table *tbl, const char *key);
ufprog_status UFPROG_API lookup_table_delete_ptr(struct ufprog_lookup_table *tbl, const void *ptr);
ufprog_bool UFPROG_API lookup_table_find(struct ufprog_lookup_table *tbl, const char *key, void **retptr);
uint32_t UFPROG_API lookup_table_length(struct ufprog_lookup_table *tbl);
ufprog_status UFPROG_API lookup_table_enum(struct ufprog_lookup_table *tbl, ufprog_lookup_table_entry_cb cb,
					   void *priv);

EXTERN_C_END

#endif /* _UFPROG_LOOKUP_TABLE_ */
