// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Controller driver management
 */

#include <malloc.h>
#include <string.h>
#include <ufprog/dirs.h>
#include <ufprog/log.h>
#include <ufprog/lookup_table.h>
#include <ufprog/driver.h>
#include "controller.h"

static ufprog_status controller_driver_api_init(struct plugin *plugin, const char *module_path);
static ufprog_status controller_driver_post_init(struct plugin *plugin);

static struct plugin_mgmt *controller_drivers;

int driver_lookup_table_init(void)
{
	if (plugin_mgmt_create("controller", CONTROLLER_DRIVER_DIR_NAME, sizeof(struct ufprog_controller_driver),
			       CONTROLLER_DRIVER_API_VERSION_MAJOR, controller_driver_api_init,
			       controller_driver_post_init, &controller_drivers))
		return -1;

	return 0;
}

static ufprog_status controller_driver_api_init(struct plugin *plugin, const char *module_path)
{
	struct ufprog_controller_driver *drv = (struct ufprog_controller_driver *)plugin;
	api_controller_supported_if fn_supported_if;
	ufprog_bool ret;

	struct symbol_find_entry basic_symbols[] = {
		FIND_MODULE(API_NAME_CONTROLLER_SUPPORTED_IF, fn_supported_if),
		FIND_MODULE(API_NAME_DEVICE_OPEN, drv->open_device),
		FIND_MODULE(API_NAME_DEVICE_FREE, drv->free_device),
	};

	struct symbol_find_entry optional_symbols[] = {
		FIND_MODULE(API_NAME_DEVICE_LOCK, drv->lock_device),
		FIND_MODULE(API_NAME_DEVICE_UNLOCK, drv->unlock_device),
		FIND_MODULE(API_NAME_DEVICE_RESET, drv->reset_device),
		FIND_MODULE(API_NAME_DEVICE_CANCEL_TRANSFER, drv->cancel_transfer),
		FIND_MODULE(API_NAME_SET_DEVICE_DISCONNECT_CB, drv->set_disconnect_cb),
	};

	ret = plugin_find_module_symbols(plugin, basic_symbols, ARRAY_SIZE(basic_symbols), true);
	if (!ret)
		return UFP_FAIL;

	drv->supported_if = fn_supported_if();
	if (!drv->supported_if)
		return UFP_FAIL;

	plugin_find_module_symbols(plugin, optional_symbols, ARRAY_SIZE(optional_symbols), false);

	if ((!!drv->lock_device) ^ (!!drv->unlock_device))
		return UFP_FAIL;

	return UFP_OK;
}

static ufprog_status controller_driver_post_init(struct plugin *plugin)
{
	struct ufprog_controller_driver *drv = (struct ufprog_controller_driver *)plugin;
	ufprog_status ret;

	ret = lookup_table_create(&drv->devices, 0);
	if (ret) {
		log_err("No memory for device management for controller driver '%s'\n", plugin->name);
		return ret;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_load_controller_driver(const char *name, struct ufprog_controller_driver **outdrv)
{
	if (!name || !outdrv)
		return UFP_INVALID_PARAMETER;

	return plugin_load(controller_drivers, name, (struct plugin **)outdrv);
}

uint32_t UFPROG_API ufprog_controller_device_count(struct ufprog_controller_driver *drv)
{
	if (!drv)
		return 0;

	return lookup_table_length(drv->devices);
}

ufprog_status UFPROG_API ufprog_unload_controller_driver(struct ufprog_controller_driver *drv)
{
	uint32_t n;

	if (!drv)
		return UFP_INVALID_PARAMETER;

	n = ufprog_controller_device_count(drv);
	if (n) {
		if (n > 1)
			log_err("There are still devices opened with driver '%s'\n", drv->plugin.name);
		else
			log_err("There is still a device opened with driver '%s'\n", drv->plugin.name);

		return UFP_MODULE_IN_USE;
	}

	return plugin_unload(controller_drivers, (struct plugin *)drv);
}

const char *UFPROG_API ufprog_controller_name(struct ufprog_controller_driver *drv)
{
	if (!drv)
		return NULL;

	return drv->plugin.name;
}

module_handle UFPROG_API ufprog_controller_module(struct ufprog_controller_driver *drv)
{
	if (!drv)
		return NULL;

	return drv->plugin.module;
}

uint32_t UFPROG_API ufprog_controller_version(struct ufprog_controller_driver *drv)
{
	if (!drv)
		return 0;

	return drv->plugin.version;
}

const char *UFPROG_API ufprog_controller_desc(struct ufprog_controller_driver *drv)
{
	if (!drv)
		return NULL;

	return drv->plugin.desc;
}

uint32_t UFPROG_API ufprog_controller_supported_if(struct ufprog_controller_driver *drv)
{
	if (!drv)
		return 0;

	return drv->supported_if;
}

void *UFPROG_API ufprog_controller_find_symbol(struct ufprog_controller_driver *drv, const char *name)
{
	if (!drv || !name)
		return NULL;

	return plugin_find_symbol(&drv->plugin, name);
}

ufprog_bool UFPROG_API ufprog_controller_find_symbols(struct ufprog_controller_driver *drv,
						      struct symbol_find_entry *list, size_t count,
						      ufprog_bool full)
{
	if (!drv)
		return false;

	if (!count)
		return true;

	if (!list)
		return false;

	return plugin_find_module_symbols(&drv->plugin, list, count, full);
}

ufprog_status ufprog_controller_add_device(struct ufprog_controller_driver *drv, const struct ufprog_interface *ifdev)
{
	ufprog_status ret;

	ret = lookup_table_insert_ptr(drv->devices, ifdev);
	if (ret)
		log_err("No memory to insert device to management list\n");

	return ret;
}

ufprog_status ufprog_controller_remove_device(struct ufprog_controller_driver *drv,
					      const struct ufprog_interface *ifdev)
{
	return lookup_table_delete_ptr(drv->devices, ifdev);
}
