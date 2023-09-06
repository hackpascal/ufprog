/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Bitmap operations
 */
#pragma once

#ifndef _UFPROG_BITMAP_H_
#define _UFPROG_BITMAP_H_

#include <stdint.h>
#include <ufprog/common.h>

EXTERN_C_BEGIN

struct ufprog_bitmap;

enum bitmap_cell_type {
	BM_CELL_TYPE_1B,
	BM_CELL_TYPE_2B,
	BM_CELL_TYPE_4B,
	BM_CELL_TYPE_8B,
	BM_CELL_TYPE_PTR,

	__BM_CELL_TYPE_MAX
};

ufprog_status UFPROG_API bitmap_create(enum bitmap_cell_type cell_type, uint32_t unit_bits, uint32_t unit_count,
				       uint32_t unit_init_val, struct ufprog_bitmap **outbm);
ufprog_status UFPROG_API bitmap_free(struct ufprog_bitmap *bm);

ufprog_status UFPROG_API bitmap_set(struct ufprog_bitmap *bm, uint32_t unit, uint32_t val);
ufprog_status UFPROG_API bitmap_get(struct ufprog_bitmap *bm, uint32_t unit, uint32_t *retval);
ufprog_status UFPROG_API bitmap_reset(struct ufprog_bitmap *bm);

void *UFPROG_API bitmap_data(struct ufprog_bitmap *bm);
size_t UFPROG_API bitmap_data_size(struct ufprog_bitmap *bm);

EXTERN_C_END

#endif /* _UFPROG_BITMAP_H_ */
