/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Bad block table (BBT) driver interface definitions
 */
#pragma once

#ifndef _UFPROG_API_BBT_H_
#define _UFPROG_API_BBT_H_

#include <ufprog/bits.h>
#include <ufprog/config.h>
#include <ufprog/api_plugin.h>

EXTERN_C_BEGIN

#define BBT_DRIVER_API_VERSION_MAJOR		1
#define BBT_DRIVER_API_VERSION_MINOR		0

/* Always scan whole NAND on probing */
#define BBT_F_FULL_SCAN				BIT(0)

/* Do not allow committing */
#define BBT_F_READ_ONLY				BIT(1)

/* Protect BBT blocks (will be presented as reserved) */
#define BBT_F_PROTECTION			BIT(2)

struct nand_chip;
struct ufprog_bbt_instance;

enum nand_bbt_gen_state {
	BBT_ST_UNKNOWN,
	BBT_ST_GOOD,
	BBT_ST_BAD,
	BBT_ST_ERASED,

	__BBT_ST_MAX
};

#define API_NAME_BBT_CREATE_INSTANCE		"ufprog_bbt_create_instance"
typedef ufprog_status (UFPROG_API *api_bbt_create_instance)(struct nand_chip *nand, struct json_object *config,
							   struct ufprog_bbt_instance **outinst);

#define API_NAME_BBT_FREE_INSTANCE		"ufprog_bbt_free_instance"
typedef ufprog_status (UFPROG_API *api_bbt_free_instance)(struct ufprog_bbt_instance *inst);

#define API_NAME_BBT_REPROBE			"ufprog_bbt_reprobe"
typedef ufprog_status (UFPROG_API *api_bbt_reprobe)(struct ufprog_bbt_instance *inst);

#define API_NAME_BBT_COMMIT			"ufprog_bbt_commit"
typedef ufprog_status (UFPROG_API *api_bbt_commit)(struct ufprog_bbt_instance *inst);

#define API_NAME_BBT_MODIFY_CONFIG		"ufprog_bbt_modify_config"
typedef ufprog_status (UFPROG_API *api_bbt_modify_config)(struct ufprog_bbt_instance *inst, uint32_t clr, uint32_t set);

#define API_NAME_BBT_GET_CONFIG			"ufprog_bbt_get_config"
typedef uint32_t (UFPROG_API *api_bbt_get_config)(struct ufprog_bbt_instance *inst);

#define API_NAME_BBT_GET_STATE			"ufprog_bbt_get_state"
typedef ufprog_status (UFPROG_API *api_bbt_get_state)(struct ufprog_bbt_instance *inst, uint32_t block,
						      uint32_t /* enum nand_bbt_gen_state */ *state);

#define API_NAME_BBT_SET_STATE			"ufprog_bbt_set_state"
typedef ufprog_status (UFPROG_API *api_bbt_set_state)(struct ufprog_bbt_instance *inst, uint32_t block,
						      uint32_t /* enum nand_bbt_gen_state */ state);

#define API_NAME_BBT_IS_RESERVED		"ufprog_bbt_is_reserved"
typedef ufprog_bool (UFPROG_API *api_bbt_is_reserved)(struct ufprog_bbt_instance *inst, uint32_t block);

EXTERN_C_END

#endif /* _UFPROG_API_BBT_H_ */
