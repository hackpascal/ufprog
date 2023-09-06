// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * RAM-based BBT implementation
 */

#include <malloc.h>
#include <string.h>
#include <ufprog/log.h>
#include <ufprog/bitmap.h>
#include <ufprog/bbt-ram.h>
#include "internal/bbt-internal.h"
#include "internal/nand-internal.h"

struct nand_bbt_ram {
	struct ufprog_nand_bbt nbbt;
	struct ufprog_bitmap *bm;

	struct nand_chip *nand;
	uint32_t config;
};

static ufprog_status bbt_ram_free(struct ufprog_nand_bbt *bbt)
{
	struct nand_bbt_ram *rbbt = container_of(bbt, struct nand_bbt_ram, nbbt);

	bitmap_free(rbbt->bm);

	free(rbbt);

	return UFP_OK;
}

static enum nand_bbt_gen_state bbt_ram_reprobe_block(struct nand_bbt_ram *rbbt, uint32_t block)
{
	struct nand_chip *nand = rbbt->nand;
	enum nand_bbt_gen_state state;
	ufprog_status ret;

	ret = ufprog_nand_checkbad(nand, NULL, block);
	if (!ret)
		state = BBT_ST_GOOD;
	else if (ret == UFP_FAIL)
		state = BBT_ST_BAD;
	else
		state = BBT_ST_UNKNOWN;

	bitmap_set(rbbt->bm, block, state);

	return state;
}

static ufprog_status bbt_ram_reprobe(struct ufprog_nand_bbt *bbt)
{
	struct nand_bbt_ram *rbbt = container_of(bbt, struct nand_bbt_ram, nbbt);
	enum nand_bbt_gen_state state;
	bool has_checkable = false;
	uint32_t block;

	if (!bbt)
		return UFP_INVALID_PARAMETER;

	if (!(rbbt->config & BBT_F_FULL_SCAN)) {
		bitmap_reset(rbbt->bm);
		return UFP_OK;
	}

	for (block = 0; block < rbbt->nand->maux.block_count; block++) {
		state = bbt_ram_reprobe_block(rbbt, block);
		if (state != BBT_ST_UNKNOWN)
			has_checkable = true;
	}

	if (has_checkable)
		return UFP_OK;

	return UFP_DEVICE_IO_ERROR;
}

static ufprog_status bbt_ram_modify_config(struct ufprog_nand_bbt *bbt, uint32_t clr, uint32_t set)
{
	struct nand_bbt_ram *rbbt = container_of(bbt, struct nand_bbt_ram, nbbt);

	if (!bbt)
		return UFP_INVALID_PARAMETER;

	rbbt->config &= ~clr;
	rbbt->config |= set & BBT_F_FULL_SCAN;

	return UFP_OK;
}

static uint32_t bbt_ram_get_config(struct ufprog_nand_bbt *bbt)
{
	struct nand_bbt_ram *rbbt = container_of(bbt, struct nand_bbt_ram, nbbt);

	if (!bbt)
		return 0;

	return rbbt->config;
}

static ufprog_status bbt_ram_get_state(struct ufprog_nand_bbt *bbt, uint32_t block,
				       uint32_t /* enum nand_bbt_gen_state */ *state)
{
	struct nand_bbt_ram *rbbt = container_of(bbt, struct nand_bbt_ram, nbbt);
	ufprog_status ret;
	uint32_t val;

	if (!bbt)
		return UFP_INVALID_PARAMETER;

	if (block >= rbbt->nand->maux.block_count)
		return UFP_INVALID_PARAMETER;

	ret = bitmap_get(rbbt->bm, block, &val);
	if (ret)
		return ret;

	if (val == BBT_ST_UNKNOWN)
		*state = bbt_ram_reprobe_block(rbbt, block);
	else
		*state = val;

	return UFP_OK;
}

static ufprog_status bbt_ram_set_state(struct ufprog_nand_bbt *bbt, uint32_t block,
				       uint32_t /* enum nand_bbt_gen_state */ state)
{
	struct nand_bbt_ram *rbbt = container_of(bbt, struct nand_bbt_ram, nbbt);

	if (!bbt)
		return UFP_INVALID_PARAMETER;

	if (block >= rbbt->nand->maux.block_count || state >= __BBT_ST_MAX)
		return UFP_INVALID_PARAMETER;

	return bitmap_set(rbbt->bm, block, state);
}

ufprog_status UFPROG_API ufprog_bbt_ram_create(const char *name, struct nand_chip *nand,
					       struct ufprog_nand_bbt **outbbt)
{
	struct nand_bbt_ram *rbbt;
	ufprog_status ret;
	size_t namelen;

	if (!name || !nand || !outbbt)
		return UFP_INVALID_PARAMETER;

	namelen = strlen(name);

	rbbt = calloc(1, sizeof(*rbbt) + namelen + 1);
	if (!rbbt)
		return UFP_NOMEM;

	rbbt->nand = nand;

	ret = bitmap_create(BM_CELL_TYPE_PTR, fls(__BBT_ST_MAX) - 1, nand->maux.block_count, BBT_ST_UNKNOWN, &rbbt->bm);
	if (ret) {
		logm_err("No memory for BBT bitmap\n");
		free(rbbt);
		return ret;
	}

	rbbt->nbbt.name = (char *)rbbt + sizeof(*rbbt);
	memcpy(rbbt->nbbt.name, name, namelen + 1);

	rbbt->nbbt.free_ni = bbt_ram_free;
	rbbt->nbbt.reprobe = bbt_ram_reprobe;
	rbbt->nbbt.modify_config = bbt_ram_modify_config;
	rbbt->nbbt.get_config = bbt_ram_get_config;
	rbbt->nbbt.get_state = bbt_ram_get_state;
	rbbt->nbbt.set_state = bbt_ram_set_state;

	*outbbt = &rbbt->nbbt;

	return UFP_OK;
}
