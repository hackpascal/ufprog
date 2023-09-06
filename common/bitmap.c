// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Bitmap operations
 */

#include <malloc.h>
#include <string.h>
#include <ufprog/bitmap.h>

struct ufprog_bitmap {
	uint32_t cell_size;
	uint32_t units_per_cell;
	uint32_t unit_size;
	uint32_t unit_count;
	uint32_t unit_mask;
	uint32_t unit_init_val;

	void *data;
	size_t size;
};

static const uint8_t bits_per_cell[__BM_CELL_TYPE_MAX] = {
	[BM_CELL_TYPE_1B] = 8 * sizeof(uint8_t),
	[BM_CELL_TYPE_2B] = 8 * sizeof(uint16_t),
	[BM_CELL_TYPE_4B] = 8 * sizeof(uint32_t),
	[BM_CELL_TYPE_8B] = 8 * sizeof(uint64_t),
	[BM_CELL_TYPE_PTR] = 8 * sizeof(uintptr_t),
};

static uint32_t bitmap_size(uint32_t cell_size, uint32_t unit_size, uint32_t unit_count)
{
	uint32_t num_cells;

	num_cells = (unit_count * unit_size + cell_size - 1) / cell_size;

	return num_cells * cell_size / 8;
}

ufprog_status UFPROG_API bitmap_create(enum bitmap_cell_type cell_type, uint32_t unit_size, uint32_t unit_count,
				       uint32_t unit_init_val, struct ufprog_bitmap **outbm)
{
	struct ufprog_bitmap *bm;
	uint32_t unit_mask;
	size_t bm_size;

	if ((uint32_t)cell_type >= __BM_CELL_TYPE_MAX || !unit_size || !unit_count || !outbm)
		return UFP_INVALID_PARAMETER;

	if (bits_per_cell[cell_type] % unit_size || unit_size >= bits_per_cell[cell_type])
		return UFP_INVALID_PARAMETER;

	unit_mask = (1ULL << unit_size) - 1;

	if (unit_init_val > unit_mask)
		return UFP_INVALID_PARAMETER;

	bm_size = bitmap_size(bits_per_cell[cell_type], unit_size, unit_count);

	bm = malloc(sizeof(*bm) + bm_size);
	if (!bm)
		return UFP_NOMEM;

	bm->data = (void *)((uintptr_t)bm + sizeof(*bm));
	bm->size = bm_size;

	bm->cell_size = bits_per_cell[cell_type];
	bm->unit_size = unit_size;
	bm->unit_count = unit_count;
	bm->unit_mask = unit_mask;
	bm->unit_init_val = unit_init_val;
	bm->units_per_cell = bm->cell_size / bm->unit_size;

	bitmap_reset(bm);

	*outbm = bm;

	return UFP_OK;
}

ufprog_status UFPROG_API bitmap_free(struct ufprog_bitmap *bm)
{
	if (!bm)
		return UFP_INVALID_PARAMETER;

	free(bm);

	return UFP_OK;
}

ufprog_status UFPROG_API bitmap_set(struct ufprog_bitmap *bm, uint32_t unit, uint32_t val)
{
	uint32_t index, shift, val32;
	uint64_t val64;
	uint16_t val16;
	uint8_t val8;

	if (!bm)
		return UFP_INVALID_PARAMETER;

	if (unit >= bm->unit_count || val > bm->unit_mask)
		return UFP_INVALID_PARAMETER;

	index = unit / bm->units_per_cell;
	shift = (unit % bm->units_per_cell) * bm->unit_size;

	switch (bm->cell_size) {
	case 8:
		val8 = ((uint8_t *)bm->data)[index];
		val8 &= ~(bm->unit_mask << shift);
		val8 |= (val << shift);
		((uint8_t *)bm->data)[index] = val8;
		break;

	case 16:
		val16 = ((uint16_t *)bm->data)[index];
		val16 &= ~(bm->unit_mask << shift);
		val16 |= (val << shift);
		((uint16_t *)bm->data)[index] = val16;
		break;

	case 32:
		val32 = ((uint32_t *)bm->data)[index];
		val32 &= ~(bm->unit_mask << shift);
		val32 |= (val << shift);
		((uint32_t *)bm->data)[index] = val32;
		break;

	case 64:
		val64 = ((uint64_t *)bm->data)[index];
		val64 &= ~((uint64_t)bm->unit_mask << shift);
		val64 |= ((uint64_t)val << shift);
		((uint64_t *)bm->data)[index] = val64;
		break;

	default:
		return UFP_INVALID_PARAMETER;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API bitmap_get(struct ufprog_bitmap *bm, uint32_t unit, uint32_t *retval)
{
	uint32_t index, shift;

	if (!bm || !retval)
		return UFP_INVALID_PARAMETER;

	if (unit >= bm->unit_count)
		return UFP_INVALID_PARAMETER;

	index = unit / bm->units_per_cell;
	shift = (unit % bm->units_per_cell) * bm->unit_size;

	switch (bm->cell_size) {
	case 8:
		*retval = (((uint8_t *)bm->data)[index] >> shift) & bm->unit_mask;
		break;

	case 16:
		*retval = (((uint16_t *)bm->data)[index] >> shift) & bm->unit_mask;
		break;

	case 32:

		*retval = (((uint32_t *)bm->data)[index] >> shift) & bm->unit_mask;
		break;

	case 64:
		*retval = (((uint64_t *)bm->data)[index] >> shift) & bm->unit_mask;
		break;

	default:
		return UFP_INVALID_PARAMETER;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API bitmap_reset(struct ufprog_bitmap *bm)
{
	uint64_t fill = 0, *p64;
	uint32_t i, bits, *p32;
	uint16_t *p16;

	if (!bm)
		return UFP_INVALID_PARAMETER;

	bits = bm->cell_size;
	if (bm->unit_size <= 8)
		bits = 8;

	while (bits) {
		fill <<= bm->unit_size;
		fill |= bm->unit_init_val;
		bits -= bm->unit_size;
	}

	if (bm->unit_size <= 8)
		goto fill_byte;

	switch (bm->cell_size) {
	case 8:
	fill_byte:
		memset(bm->data, (uint8_t)fill, bm->size);
		break;

	case 16:
		p16 = (uint16_t *)bm->data;

		for (i = 0; i < bm->size / 2; i++)
			p16[i] = (uint16_t)fill;

		break;

	case 32:
		p32 = (uint32_t *)bm->data;

		for (i = 0; i < bm->size / 4; i++)
			p32[i] = (uint32_t)fill;

		break;

	case 64:
		p64 = (uint64_t *)bm->data;

		for (i = 0; i < bm->size / 8; i++)
			p64[i] = (uint64_t)fill;

		break;
	}

	return UFP_OK;
}

void *UFPROG_API bitmap_data(struct ufprog_bitmap *bm)
{
	if (!bm)
		return NULL;

	return bm->data;
}

size_t UFPROG_API bitmap_data_size(struct ufprog_bitmap *bm)
{
	if (!bm)
		return 0;

	return bm->size;
}
