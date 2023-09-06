// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ECC driver management
 */

#include <malloc.h>
#include <string.h>
#include <ufprog/dirs.h>
#include <ufprog/log.h>
#include <ufprog/lookup_table.h>
#include "internal/ecc-internal.h"

static ufprog_status ecc_driver_api_init(struct plugin *plugin, const char *module_path);
static ufprog_status ecc_driver_post_init(struct plugin *plugin);

static struct plugin_mgmt *ecc_drivers;

int ecc_driver_mgmt_init(void)
{
	if (plugin_mgmt_create("ECC", ECC_DRIVER_DIR_NAME, sizeof(struct ufprog_ecc_driver),
			       ECC_DRIVER_API_VERSION_MAJOR, ecc_driver_api_init, ecc_driver_post_init, &ecc_drivers))
		return -1;

	return 0;
}

void ecc_driver_mgmt_deinit(void)
{
	if (ecc_drivers)
		plugin_mgmt_destroy(ecc_drivers);
}

static ufprog_status ecc_driver_api_init(struct plugin *plugin, const char *module_path)
{
	struct ufprog_ecc_driver *drv = (struct ufprog_ecc_driver *)plugin;
	ufprog_bool ret;

	struct symbol_find_entry basic_symbols[] = {
		FIND_MODULE(API_NAME_ECC_CREATE_INSTANCE, drv->create_instance),
		FIND_MODULE(API_NAME_ECC_FREE_INSTANCE, drv->free_instance),
		FIND_MODULE(API_NAME_ECC_GET_CONFIG, drv->get_config),
		FIND_MODULE(API_NAME_ECC_ENCODE_PAGE, drv->encode_page),
		FIND_MODULE(API_NAME_ECC_DECODE_PAGE, drv->decode_page),
		FIND_MODULE(API_NAME_ECC_GET_STATUS, drv->get_status),
		FIND_MODULE(API_NAME_ECC_GET_PAGE_LAYOUT, drv->get_page_layout),
	};

	struct symbol_find_entry optional_symbols[] = {
		FIND_MODULE(API_NAME_ECC_GET_BBM_CONFIG, drv->get_bbm_config),
		FIND_MODULE(API_NAME_ECC_CONVERT_PAGE_LAYOUT, drv->convert_page_layout),
	};

	ret = plugin_find_module_symbols(plugin, basic_symbols, ARRAY_SIZE(basic_symbols), true);
	if (!ret)
		return UFP_FAIL;

	plugin_find_module_symbols(plugin, optional_symbols, ARRAY_SIZE(optional_symbols), false);

	return UFP_OK;
}

static ufprog_status ecc_driver_post_init(struct plugin *plugin)
{
	struct ufprog_ecc_driver *drv = (struct ufprog_ecc_driver *)plugin;
	ufprog_status ret;

	ret = lookup_table_create(&drv->instances, 0);
	if (ret) {
		log_err("No memory for device management for ECC driver '%s'\n", plugin->name);
		return ret;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_load_ecc_config(const char *name, struct json_object **outconfig)
{
	if (!name || !outconfig)
		return UFP_INVALID_PARAMETER;

	return plugin_config_load(ecc_drivers, name, outconfig);
}

ufprog_status UFPROG_API ufprog_load_ecc_driver(const char *name, struct ufprog_ecc_driver **outdrv)
{
	if (!name || !outdrv)
		return UFP_INVALID_PARAMETER;

	return plugin_load(ecc_drivers, name, (struct plugin **)outdrv);
}

ufprog_status UFPROG_API ufprog_unload_ecc_driver(struct ufprog_ecc_driver *drv)
{
	uint32_t n;

	if (!drv)
		return UFP_INVALID_PARAMETER;

	n = ufprog_ecc_driver_instance_count(drv);
	if (n) {
		if (n > 1)
			log_errdbg("There are still instances opened with driver '%s'\n", drv->plugin.name);
		else
			log_errdbg("There is still an instance opened with driver '%s'\n", drv->plugin.name);

		return UFP_MODULE_IN_USE;
	}

	return plugin_unload(ecc_drivers, (struct plugin *)drv);
}

uint32_t UFPROG_API ufprog_ecc_driver_instance_count(struct ufprog_ecc_driver *drv)
{
	if (!drv)
		return 0;

	return lookup_table_length(drv->instances);
}

const char *UFPROG_API ufprog_ecc_driver_name(struct ufprog_ecc_driver *drv)
{
	if (!drv)
		return NULL;

	return drv->plugin.name;
}

uint32_t UFPROG_API ufprog_ecc_driver_version(struct ufprog_ecc_driver *drv)
{
	if (!drv)
		return 0;

	return drv->plugin.version;
}

uint32_t UFPROG_API ufprog_ecc_driver_api_version(struct ufprog_ecc_driver *drv)
{
	if (!drv)
		return 0;

	return drv->plugin.api_version;
}

const char *UFPROG_API ufprog_ecc_driver_desc(struct ufprog_ecc_driver *drv)
{
	if (!drv)
		return NULL;

	return drv->plugin.desc;
}

ufprog_status ufprog_ecc_add_instance(struct ufprog_ecc_driver *drv, const struct ufprog_ecc_instance *inst)
{
	ufprog_status ret;

	ret = lookup_table_insert_ptr(drv->instances, inst);
	if (ret)
		log_err("No memory to insert ECC instance to management list\n");

	return ret;
}

ufprog_status ufprog_ecc_remove_instance(struct ufprog_ecc_driver *drv, const struct ufprog_ecc_instance *inst)
{
	return lookup_table_delete_ptr(drv->instances, inst);
}
