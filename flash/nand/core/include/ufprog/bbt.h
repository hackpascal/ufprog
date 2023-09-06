/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * BBT management
 */
#pragma once

#ifndef _UFPROG_BBT_H_
#define _UFPROG_BBT_H_

#include <stdbool.h>
#include <ufprog/api_bbt.h>
#include <ufprog/config.h>

EXTERN_C_BEGIN

struct ufprog_nand_bbt;

ufprog_status UFPROG_API ufprog_bbt_create(const char *drvname, const char *name, struct nand_chip *nand,
					   struct json_object *config, struct ufprog_nand_bbt **outbbt);
ufprog_status UFPROG_API ufprog_bbt_free(struct ufprog_nand_bbt *bbt);

const char *UFPROG_API ufprog_bbt_name(struct ufprog_nand_bbt *bbt);

ufprog_status UFPROG_API ufprog_bbt_reprobe(struct ufprog_nand_bbt *bbt);
ufprog_status UFPROG_API ufprog_bbt_commit(struct ufprog_nand_bbt *bbt);

ufprog_status UFPROG_API ufprog_bbt_modify_config(struct ufprog_nand_bbt *bbt, uint32_t clr, uint32_t set);
uint32_t UFPROG_API ufprog_bbt_get_config(struct ufprog_nand_bbt *bbt);

ufprog_status UFPROG_API ufprog_bbt_get_state(struct ufprog_nand_bbt *bbt, uint32_t block,
					      uint32_t /* enum nand_bbt_gen_state */ *state);
ufprog_status UFPROG_API ufprog_bbt_set_state(struct ufprog_nand_bbt *bbt, uint32_t block,
					      uint32_t /* enum nand_bbt_gen_state */ state);
ufprog_bool UFPROG_API ufprog_bbt_is_reserved(struct ufprog_nand_bbt *bbt, uint32_t block);

static inline ufprog_bool ufprog_bbt_is_unknown(struct ufprog_nand_bbt *bbt, uint32_t block)
{
	uint32_t state;

	if (!ufprog_bbt_get_state(bbt, block, &state))
		return state == BBT_ST_UNKNOWN;

	return false;
}

static inline ufprog_bool ufprog_bbt_is_good(struct ufprog_nand_bbt *bbt, uint32_t block)
{
	uint32_t state;

	if (!ufprog_bbt_get_state(bbt, block, &state))
		return state == BBT_ST_GOOD;

	return false;
}

static inline ufprog_bool ufprog_bbt_is_bad(struct ufprog_nand_bbt *bbt, uint32_t block)
{
	uint32_t state;

	if (!ufprog_bbt_get_state(bbt, block, &state))
		return state == BBT_ST_BAD;

	return false;
}

static inline ufprog_bool ufprog_bbt_is_erased(struct ufprog_nand_bbt *bbt, uint32_t block)
{
	uint32_t state;

	if (!ufprog_bbt_get_state(bbt, block, &state))
		return state == BBT_ST_ERASED;

	return false;
}

EXTERN_C_END

#endif /* _UFPROG_BBT_H_ */
