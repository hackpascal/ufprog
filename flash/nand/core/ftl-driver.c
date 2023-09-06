// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Simple Flash Translation Layer (FTL) driver management
 */

#include <malloc.h>
#include <string.h>
#include <ufprog/dirs.h>
#include <ufprog/log.h>
#include <ufprog/lookup_table.h>
#include "internal/ftl-internal.h"

static ufprog_status ftl_driver_api_init(struct plugin *plugin, const char *module_path);
static ufprog_status ftl_driver_post_init(struct plugin *plugin);

static struct plugin_mgmt *ftl_drivers;

int ftl_driver_mgmt_init(void)
{
	if (plugin_mgmt_create("FTL", FTL_DRIVER_DIR_NAME, sizeof(struct ufprog_ftl_driver),
			       FTL_DRIVER_API_VERSION_MAJOR, ftl_driver_api_init, ftl_driver_post_init, &ftl_drivers))
		return -1;

	return 0;
}

void ftl_driver_mgmt_deinit(void)
{
	if (ftl_drivers)
		plugin_mgmt_destroy(ftl_drivers);
}

static ufprog_status ftl_driver_api_init(struct plugin *plugin, const char *module_path)
{
	struct ufprog_ftl_driver *drv = (struct ufprog_ftl_driver *)plugin;
	ufprog_bool ret;

	struct symbol_find_entry basic_symbols[] = {
		FIND_MODULE(API_NAME_FTL_CREATE_INSTANCE, drv->create_instance),
		FIND_MODULE(API_NAME_FTL_FREE_INSTANCE, drv->free_instance),
		FIND_MODULE(API_NAME_FTL_GET_SIZE, drv->get_size),
		FIND_MODULE(API_NAME_FTL_READ_PAGE, drv->read_page),
		FIND_MODULE(API_NAME_FTL_WRITE_PAGE, drv->write_page),
		FIND_MODULE(API_NAME_FTL_ERASE_BLOCK, drv->erase_block),
		FIND_MODULE(API_NAME_FTL_BLOCK_CHECK_BAD, drv->block_checkbad),
		FIND_MODULE(API_NAME_FTL_BLOCK_MARK_BAD, drv->block_markbad),
	};

	struct symbol_find_entry optional_symbols[] = {
		FIND_MODULE(API_NAME_FTL_READ_PAGES, drv->read_pages),
		FIND_MODULE(API_NAME_FTL_WRITE_PAGES, drv->write_pages),
		FIND_MODULE(API_NAME_FTL_ERASE_BLOCKS, drv->erase_blocks),
	};

	ret = plugin_find_module_symbols(plugin, basic_symbols, ARRAY_SIZE(basic_symbols), true);
	if (!ret)
		return UFP_FAIL;

	plugin_find_module_symbols(plugin, optional_symbols, ARRAY_SIZE(optional_symbols), false);

	return UFP_OK;
}

static ufprog_status ftl_driver_post_init(struct plugin *plugin)
{
	struct ufprog_ftl_driver *drv = (struct ufprog_ftl_driver *)plugin;
	ufprog_status ret;

	ret = lookup_table_create(&drv->instances, 0);
	if (ret) {
		log_err("No memory for device management for FTL driver '%s'\n", plugin->name);
		return ret;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_load_ftl_config(const char *name, struct json_object **outconfig)
{
	if (!name || !outconfig)
		return UFP_INVALID_PARAMETER;

	return plugin_config_load(ftl_drivers, name, outconfig);
}

ufprog_status UFPROG_API ufprog_load_ftl_driver(const char *name, struct ufprog_ftl_driver **outdrv)
{
	if (!name || !outdrv)
		return UFP_INVALID_PARAMETER;

	return plugin_load(ftl_drivers, name, (struct plugin **)outdrv);
}

ufprog_status UFPROG_API ufprog_unload_ftl_driver(struct ufprog_ftl_driver *drv)
{
	uint32_t n;

	if (!drv)
		return UFP_INVALID_PARAMETER;

	n = ufprog_ftl_driver_instance_count(drv);
	if (n) {
		if (n > 1)
			log_errdbg("There are still instances opened with driver '%s'\n", drv->plugin.name);
		else
			log_errdbg("There is still an instance opened with driver '%s'\n", drv->plugin.name);

		return UFP_MODULE_IN_USE;
	}

	return plugin_unload(ftl_drivers, (struct plugin *)drv);
}

uint32_t UFPROG_API ufprog_ftl_driver_instance_count(struct ufprog_ftl_driver *drv)
{
	if (!drv)
		return 0;

	return lookup_table_length(drv->instances);
}

const char *UFPROG_API ufprog_ftl_driver_name(struct ufprog_ftl_driver *drv)
{
	if (!drv)
		return NULL;

	return drv->plugin.name;
}

uint32_t UFPROG_API ufprog_ftl_driver_version(struct ufprog_ftl_driver *drv)
{
	if (!drv)
		return 0;

	return drv->plugin.version;
}

uint32_t UFPROG_API ufprog_ftl_driver_api_version(struct ufprog_ftl_driver *drv)
{
	if (!drv)
		return 0;

	return drv->plugin.api_version;
}

const char *UFPROG_API ufprog_ftl_driver_desc(struct ufprog_ftl_driver *drv)
{
	if (!drv)
		return NULL;

	return drv->plugin.desc;
}

ufprog_status ufprog_ftl_add_instance(struct ufprog_ftl_driver *drv, const struct ufprog_ftl_instance *inst)
{
	ufprog_status ret;

	ret = lookup_table_insert_ptr(drv->instances, inst);
	if (ret)
		log_err("No memory to insert FTL instance to management list\n");

	return ret;
}

ufprog_status ufprog_ftl_remove_instance(struct ufprog_ftl_driver *drv, const struct ufprog_ftl_instance *inst)
{
	return lookup_table_delete_ptr(drv->instances, inst);
}
