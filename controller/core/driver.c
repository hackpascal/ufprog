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

static struct ufprog_lookup_table *loaded_drivers;

int driver_lookup_table_init(void)
{
	if (lookup_table_create(&loaded_drivers, 0))
		return -1;

	return 0;
}

static bool ufprog_driver_check(struct ufprog_driver *drv)
{
	api_drv_version fn_driver_version;
	api_drv_desc fn_driver_desc;
	api_drv_supported_if fn_supported_if;
	api_device_open fn_open_device;
	api_device_free fn_free_device;
	ufprog_bool ret;

	struct symbol_find_entry basic_symbols[] = {
		FIND_MODULE(API_NAME_DRV_VERSION, fn_driver_version),
		FIND_MODULE(API_NAME_DRV_DESC, fn_driver_desc),
		FIND_MODULE(API_NAME_DRV_SUPPORTED_IF, fn_supported_if),
		FIND_MODULE(API_NAME_DEVICE_OPEN, fn_open_device),
		FIND_MODULE(API_NAME_DEVICE_FREE, fn_free_device),
	};

	struct symbol_find_entry optional_symbols[] = {
		FIND_MODULE(API_NAME_DRV_INIT, drv->init),
		FIND_MODULE(API_NAME_DRV_CLEANUP, drv->cleanup),
		FIND_MODULE(API_NAME_DEVICE_LOCK, drv->lock_device),
		FIND_MODULE(API_NAME_DEVICE_UNLOCK, drv->unlock_device),
		FIND_MODULE(API_NAME_DEVICE_RESET, drv->reset_device),
		FIND_MODULE(API_NAME_DEVICE_CANCEL_TRANSFER, drv->cancel_transfer),
		FIND_MODULE(API_NAME_SET_DEVICE_DISCONNECT_CB, drv->set_disconnect_cb),
	};

	ret = os_find_module_symbols(drv->module, basic_symbols, ARRAY_SIZE(basic_symbols), true);
	if (ret)
		return false;

	drv->supported_if = fn_supported_if();
	if (!drv->supported_if)
		return false;

	drv->version = fn_driver_version();
	drv->desc = fn_driver_desc();

	drv->open_device = fn_open_device;
	drv->free_device = fn_free_device;

	os_find_module_symbols(drv->module, optional_symbols, ARRAY_SIZE(optional_symbols), false);

	if ((!!drv->lock_device) ^ (!!drv->unlock_device))
		return false;

	return true;
}

static int UFPROG_API dir_enum_drivers(void *priv, uint32_t index, const char *dir)
{
	struct ufprog_driver *drv = priv;
	char *module_path;
	ufprog_status ret;
	int eret = 0;
	size_t len;

	module_path = path_concat(false, strlen(MODULE_SUFFIX), dir, CONTROLLER_DRIVER_DIR_NAME, drv->name, NULL);
	if (!module_path)
		return 0;

	len = strlen(module_path);
	memcpy(module_path + len, MODULE_SUFFIX, strlen(MODULE_SUFFIX) + 1);

	log_dbg("Trying loading interface driver '%s'\n", module_path);

	ret = os_load_module(module_path, &drv->module);
	if (ret) {
		if (ret == UFP_FILE_NOT_EXIST)
			log_dbg("'%s' does not exist\n", module_path);

		free(module_path);
		return 0;
	}

	if (ufprog_driver_check(drv)) {
		log_notice("'%s' loaded as interface driver\n", module_path);
		eret = 1;
	} else {
		log_err("'%s' is not a valid ufprog interface driver\n", module_path);
		os_unload_module(drv->module);
		drv->module = NULL;
	}

	free(module_path);
	return eret;
}

ufprog_status UFPROG_API ufprog_load_driver(const char *name, struct ufprog_driver **outdrv)
{
	struct ufprog_driver *drv;
	ufprog_status ret;

	if (!name || !outdrv)
		return UFP_INVALID_PARAMETER;

	if (lookup_table_find(loaded_drivers, name, (void **)&drv)) {
		*outdrv = drv;
		return UFP_OK;
	}

	*outdrv = NULL;

	drv = calloc(1, sizeof(*drv));
	if (!drv) {
		log_err("No memory for loading interface driver\n");
		return UFP_NOMEM;
	}

	drv->name = os_strdup(name);
	if (!drv->name) {
		log_err("No memory for loading interface driver\n");
		free(drv);
		return UFP_NOMEM;
	}

	dir_enum(DIR_PLUGIN, dir_enum_drivers, drv);

	if (!drv->module) {
		log_err("No interface driver module named '%s' could be loaded\n", name);
		free(drv->name);
		free(drv);
		return UFP_NOT_EXIST;
	}

	if (drv->init) {
		ret = drv->init();
		if (ret) {
			log_err("Interface driver '%s' initialization failed\n", name);
			ret = UFP_MODULE_INIT_FAIL;
			goto cleanup;
		}
	}

	ret = lookup_table_insert(loaded_drivers, name, drv);
	if (ret) {
		log_err("No memory for maintain interface driver\n");
		goto cleanup_module;
	}

	if (lookup_table_create(&drv->devices, 0)) {
		log_err("No memory for device management for driver\n");
		goto cleanup_module_remove;
	}

	log_info("Loaded interface driver %s %u.%u\n", drv->desc,
		 GET_MAJOR_VERSION(drv->version), GET_MINOR_VERSION(drv->version));

	*outdrv = drv;
	return UFP_OK;

cleanup_module_remove:
	lookup_table_delete(loaded_drivers, name);

cleanup_module:
	if (drv->cleanup)
		drv->cleanup();

cleanup:
	os_unload_module(drv->module);
	free(drv->name);
	free(drv);

	return ret;
}

uint32_t UFPROG_API ufprog_driver_device_count(struct ufprog_driver *drv)
{
	if (!drv)
		return 0;

	return lookup_table_length(drv->devices);
}

ufprog_status UFPROG_API ufprog_unload_driver(struct ufprog_driver *drv)
{
	ufprog_status ret;
	uint32_t n;

	if (!drv)
		return UFP_INVALID_PARAMETER;

	n = ufprog_driver_device_count(drv);
	if (n) {
		if (n > 1)
			log_err("There are still devices opened with driver '%s'\n", drv->name);
		else
			log_err("There is still a device opened with driver '%s'\n", drv->name);

		return UFP_MODULE_IN_USE;
	}

	lookup_table_delete(loaded_drivers, drv->name);

	if (drv->cleanup) {
		ret = drv->cleanup();
		if (ret)
			log_warn("Interface driver '%s' cleanup failed\n", drv->name);
	}

	os_unload_module(drv->module);
	free(drv->name);

	return UFP_OK;
}

const char *UFPROG_API ufprog_driver_name(struct ufprog_driver *drv)
{
	if (!drv)
		return NULL;

	return drv->name;
}

module_handle UFPROG_API ufprog_driver_module(struct ufprog_driver *drv)
{
	if (!drv)
		return NULL;

	return drv->module;
}

uint32_t UFPROG_API ufprog_driver_version(struct ufprog_driver *drv)
{
	if (!drv)
		return 0;

	return drv->version;
}

const char *UFPROG_API ufprog_driver_desc(struct ufprog_driver *drv)
{
	if (!drv)
		return NULL;

	return drv->desc;
}

uint32_t UFPROG_API ufprog_driver_supported_if(struct ufprog_driver *drv)
{
	if (!drv)
		return 0;

	return drv->supported_if;
}

void *UFPROG_API ufprog_driver_find_symbol(struct ufprog_driver *drv, const char *name)
{
	if (!drv || !name)
		return NULL;

	return os_find_module_symbol(drv->module, name);
}

ufprog_bool UFPROG_API ufprog_driver_find_module_symbols(struct ufprog_driver *drv, struct symbol_find_entry *list,
							 size_t count, ufprog_bool full)
{
	if (!drv)
		return false;

	if (!count)
		return true;

	if (!list)
		return false;

	return os_find_module_symbols(drv->module, list, count, full) == UFP_OK;
}

ufprog_status ufprog_driver_add_device(struct ufprog_driver *drv, const struct ufprog_if_dev *ifdev)
{
	ufprog_status ret;

	ret = lookup_table_insert_ptr(drv->devices, ifdev);
	if (ret)
		log_err("No memory to insert device to management list\n");

	return ret;
}

ufprog_status ufprog_driver_remove_device(struct ufprog_driver *drv, const struct ufprog_if_dev *ifdev)
{
	return lookup_table_delete_ptr(drv->devices, ifdev);
}
