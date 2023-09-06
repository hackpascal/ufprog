// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * BBT chip management
 */

#include <stdbool.h>
#include <string.h>
#include <ufprog/log.h>
#include "internal/bbt-internal.h"

ufprog_status UFPROG_API ufprog_bbt_create(const char *drvname, const char *name, struct nand_chip *nand,
					   struct json_object *config, struct ufprog_nand_bbt **outbbt)
{
	struct ufprog_bbt_instance *bbtinst;
	struct ufprog_bbt_driver *drv;
	struct ufprog_nand_bbt *bbt;
	ufprog_status ret;
	size_t namelen;

	if (!drvname || !outbbt || !nand)
		return UFP_INVALID_PARAMETER;

	ret = ufprog_load_bbt_driver(drvname, &drv);
	if (ret)
		return ret;

	ret = drv->create_instance(nand, config, &bbtinst);
	if (ret)
		goto cleanup_unload_driver;

	ret = ufprog_bbt_add_instance(drv, bbtinst);
	if (ret)
		goto cleanup_free_instance;

	if (!name)
		name = drvname;

	namelen = strlen(name);

	bbt = calloc(1, sizeof(*bbt) + namelen + 1);
	if (!bbt) {
		log_err("No memory for BBT\n");
		ret = UFP_NOMEM;
		goto cleanup_remove_instance;
	}

	bbt->name = (char *)bbt + sizeof(*bbt);
	memcpy(bbt->name, name, namelen + 1);

	bbt->driver = drv;
	bbt->instance = bbtinst;

	*outbbt = bbt;
	return UFP_OK;

cleanup_remove_instance:
	ufprog_bbt_remove_instance(drv, bbtinst);

cleanup_free_instance:
	drv->free_instance(bbtinst);

cleanup_unload_driver:
	ufprog_unload_bbt_driver(drv);

	return ret;
}

ufprog_status UFPROG_API ufprog_bbt_free(struct ufprog_nand_bbt *bbt)
{
	if (!bbt)
		return UFP_INVALID_PARAMETER;

	if (bbt->driver && bbt->instance) {
		ufprog_bbt_remove_instance(bbt->driver, bbt->instance);
		bbt->driver->free_instance(bbt->instance);
		ufprog_unload_bbt_driver(bbt->driver);
	} else {
		if (bbt->free_ni)
			return bbt->free_ni(bbt);
	}

	free(bbt);

	return UFP_OK;
}

const char *UFPROG_API ufprog_bbt_name(struct ufprog_nand_bbt *bbt)
{
	if (!bbt)
		return NULL;

	return bbt->name;
}

ufprog_status UFPROG_API ufprog_bbt_reprobe(struct ufprog_nand_bbt *bbt)
{
	if (!bbt)
		return UFP_INVALID_PARAMETER;

	if (bbt->driver && bbt->instance)
		return bbt->driver->reprobe(bbt->instance);

	return bbt->reprobe(bbt);
}

ufprog_status UFPROG_API ufprog_bbt_commit(struct ufprog_nand_bbt *bbt)
{
	if (!bbt)
		return UFP_INVALID_PARAMETER;

	if (bbt->driver && bbt->instance) {
		if (bbt->driver->commit)
			return bbt->driver->commit(bbt->instance);
	}

	if (bbt->commit)
		return bbt->commit(bbt);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_bbt_modify_config(struct ufprog_nand_bbt *bbt, uint32_t clr, uint32_t set)
{
	if (!bbt)
		return UFP_INVALID_PARAMETER;

	if (bbt->driver && bbt->instance) {
		if (bbt->driver->modify_config)
			return bbt->driver->modify_config(bbt->instance, clr, set);
	}

	if (bbt->modify_config)
		return bbt->modify_config(bbt, clr, set);

	return UFP_UNSUPPORTED;
}

uint32_t UFPROG_API ufprog_bbt_get_config(struct ufprog_nand_bbt *bbt)
{
	if (!bbt)
		return UFP_INVALID_PARAMETER;

	if (bbt->driver && bbt->instance) {
		if (bbt->driver->get_config)
			return bbt->driver->get_config(bbt->instance);
	}

	if (bbt->get_config)
		return bbt->get_config(bbt);

	return 0;
}

ufprog_status UFPROG_API ufprog_bbt_get_state(struct ufprog_nand_bbt *bbt, uint32_t block,
					      uint32_t /* enum nand_bbt_gen_state */ *state)
{
	if (!bbt)
		return UFP_INVALID_PARAMETER;

	if (bbt->driver && bbt->instance)
		return bbt->driver->get_state(bbt->instance, block, state);

	return bbt->get_state(bbt, block, state);
}

ufprog_status UFPROG_API ufprog_bbt_set_state(struct ufprog_nand_bbt *bbt, uint32_t block,
					      uint32_t /* enum nand_bbt_gen_state */ state)
{
	if (!bbt)
		return UFP_INVALID_PARAMETER;

	if (state >= __BBT_ST_MAX)
		return UFP_INVALID_PARAMETER;

	if (bbt->driver && bbt->instance)
		return bbt->driver->set_state(bbt->instance, block, state);

	return bbt->set_state(bbt, block, state);
}

ufprog_bool UFPROG_API ufprog_bbt_is_reserved(struct ufprog_nand_bbt *bbt, uint32_t block)
{
	if (!bbt)
		return UFP_INVALID_PARAMETER;

	if (bbt->driver && bbt->instance) {
		if (bbt->driver->is_reserved)
			return bbt->driver->is_reserved(bbt->instance, block);
	}

	if (bbt->is_reserved)
		return bbt->is_reserved(bbt, block);

	return false;
}
