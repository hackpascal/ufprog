// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Controller device management
 */

#include <malloc.h>
#include <string.h>
#include <ufprog/lookup_table.h>
#include <ufprog/config.h>
#include <ufprog/device.h>
#include <ufprog/dirs.h>
#include <ufprog/log.h>
#include "controller.h"

struct device_open_data {
	struct ufprog_device *dev;
	ufprog_bool thread_safe;
};

static const char *if_type_str[] = {
	[IF_SPI] = "spi",
	[IF_I2C] = "i2c",
	[IF_NAND] = "nand",
	[IF_SDIO] = "sdio",
};

static int if_type_str_to_value(const char *name)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(if_type_str); i++) {
		if (!strcmp(if_type_str[i], name))
			return i;
	}

	return -1;
}

ufprog_status UFPROG_API ufprog_open_device(struct ufprog_driver *drv, uint32_t if_type, struct json_object *config,
					    ufprog_bool thread_safe, struct ufprog_device **outdev)
{
	struct ufprog_device *dev;
	ufprog_status ret;

	if (!drv || !outdev)
		return UFP_INVALID_PARAMETER;

	*outdev = NULL;

	if (if_type >= __MAX_IF_TYPE) {
		log_err("Invalid interface type %u\n", if_type);
		return UFP_INVALID_PARAMETER;
	}

	dev = malloc(sizeof(*dev));
	if (!dev) {
		log_err("No memory for new interface device\n");
		return UFP_NOMEM;
	}

	dev->name = NULL;
	dev->driver = drv;
	dev->if_type = if_type;

	ret = drv->open_device(if_type, config, thread_safe, &dev->ifdev);
	if (ret) {
		log_err("Failed to open interface device using driver '%s'\n", drv->name);
		free(dev);
		return ret;
	}

	ret = ufprog_driver_add_device(drv, dev->ifdev);
	if (ret) {
		drv->free_device(dev->ifdev);
		free(dev);
		return ret;
	}

	log_info("Opened interface device using driver '%s'\n", dev->driver->name);

	*outdev = dev;
	return UFP_OK;
}

static int dir_enum_devices(void *priv, uint32_t index, const char *dir)
{
	struct json_object *jroot, *jconfig, *jarr;
	const char *driver_name, *if_type_name;
	struct device_open_data *data = priv;
	size_t len, num_if_types, i;
	char *config_path;
	ufprog_status ret;
	int if_type;

	config_path = path_concat(false, strlen(UFPROG_CONFIG_SUFFIX), dir, data->dev->name, NULL);
	if (!config_path)
		return 0;

	len = strlen(config_path);
	memcpy(config_path + len, UFPROG_CONFIG_SUFFIX, strlen(UFPROG_CONFIG_SUFFIX) + 1);

	log_dbg("Trying to load interface device config '%s'\n", config_path);

	ret = json_from_file(config_path, &jroot);
	if (ret) {
		if (ret != UFP_FILE_NOT_EXIST)
			log_errdbg("Failed to load '%s'\n", config_path);

		free(config_path);
		return 0;
	}

	if (json_is_str(jroot, "if_type")) {
		ret = json_read_str(jroot, "if_type", &if_type_name, NULL);
		if (ret) {
			log_err("'if_type' is missing or invalid in device config\n");
			goto cleanup;
		}

		if_type = if_type_str_to_value(if_type_name);

		if (if_type < 0) {
			log_err("Invalid 'if_type' field value specified in device config\n");
			goto cleanup;
		}

		if (data->dev->if_type != (uint32_t)if_type) {
			log_err("Interface type specified in device config is not compatible\n");
			goto cleanup;
		}
	} else {
		if (!json_is_array(jroot, "if_type")) {
			log_err("Invalid 'if_type' field type in device config\n");
			goto cleanup;
		}

		ret = json_read_array(jroot, "if_type", &jarr);
		if (ret) {
			log_err("Failed to get 'if_type' array in device config\n");
			goto cleanup;
		}

		num_if_types = json_array_len(jarr);

		for (i = 0; i < num_if_types; i++) {
			ret = json_array_read_str(jarr, i, &if_type_name, NULL);
			if (ret) {
				log_warn("'if_type' is missing or invalid of array index %u in device config\n", i);
				continue;
			}

			if_type = if_type_str_to_value(if_type_name);

			if (if_type < 0) {
				log_err("Invalid value of 'if_type' array index %u in device config\n", i);
				continue;
			}

			if (data->dev->if_type == (uint32_t)if_type) {
				log_notice("Interface type specified in device config is compatible\n");
				goto valid_if_type;
			}
		}

		log_err("Interface type specified in device config is not compatible\n");
		goto cleanup;
	}

valid_if_type:
	ret = json_read_str(jroot, "driver", &driver_name, NULL);
	if (ret) {
		log_err("'driver' is missing or invalid in device config\n");
		goto cleanup;
	}

	STATUS_CHECK_GOTO(ufprog_load_driver(driver_name, &data->dev->driver), cleanup);

	if (!(data->dev->driver->supported_if & IF_TYPE_BIT(data->dev->if_type))) {
		log_err("Loaded interface driver does not support '%s'\n", if_type_str[data->dev->if_type]);
		goto cleanup;
	}

	ret = json_read_obj(jroot, "config", &jconfig);
	if (ret)
		jconfig = NULL;

	ret = data->dev->driver->open_device(data->dev->if_type, jconfig, data->thread_safe, &data->dev->ifdev);
	if (ret) {
		log_err("Failed to open interface device using '%s'\n", config_path);
		data->dev->ifdev = NULL;
		goto cleanup;
	}

	free(config_path);
	json_free(jroot);
	return 1;

cleanup:
	if (data->dev->driver) {
		ufprog_unload_driver(data->dev->driver);
		data->dev->driver = NULL;
	}

	free(config_path);
	json_free(jroot);
	return 0;
}

ufprog_status UFPROG_API ufprog_open_device_by_name(const char *name, uint32_t if_type, ufprog_bool thread_safe,
						    struct ufprog_device **outdev)
{
	struct device_open_data data;
	struct ufprog_device *dev;

	if (!name || !outdev)
		return UFP_INVALID_PARAMETER;

	*outdev = NULL;

	if (if_type >= __MAX_IF_TYPE) {
		log_err("Invalid interface type %u\n", if_type);
		return UFP_INVALID_PARAMETER;
	}

	dev = calloc(1, sizeof(*dev));
	if (!dev) {
		log_err("No memory for new interface device\n");
		return UFP_NOMEM;
	}

	dev->name = os_strdup(name);
	if (!dev->name) {
		log_err("No memory for loading interface device\n");
		free(dev);
		return UFP_NOMEM;
	}

	dev->if_type = if_type;

	data.dev = dev;
	data.thread_safe = thread_safe;

	dir_enum(DIR_DEVICE, dir_enum_devices, &data);

	if (!dev->ifdev) {
		log_err("No interface device named '%s' could be opened\n", name);
		free(dev->name);
		free(dev);
		return UFP_NOT_EXIST;
	}

	log_info("Opened interface device '%s' using driver '%s'\n", name, dev->driver->name);

	*outdev = dev;
	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_close_device(struct ufprog_device *dev)
{
	ufprog_status ret;

	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (!dev->driver || !dev->ifdev)
		return UFP_INVALID_PARAMETER;

	ufprog_driver_remove_device(dev->driver, dev->ifdev);

	ret = dev->driver->free_device(dev->ifdev);
	if (!ret) {
		free(dev->name);
		free(dev);
	}

	return ret;
}

const char *UFPROG_API ufprog_device_name(struct ufprog_device *dev)
{
	if (!dev)
		return NULL;

	return dev->name;
}

uint32_t UFPROG_API ufprog_device_if_type(struct ufprog_device *dev)
{
	if (!dev)
		return __MAX_IF_TYPE;

	return dev->if_type;
}

struct ufprog_driver *UFPROG_API ufprog_device_get_driver(struct ufprog_device *dev)
{
	if (!dev)
		return NULL;

	return dev->driver;
}

struct ufprog_if_dev *UFPROG_API ufprog_device_get_interface_device(struct ufprog_device *dev)
{
	if (!dev)
		return NULL;

	return dev->ifdev;
}

ufprog_status UFPROG_API ufprog_lock_device(struct ufprog_device *dev)
{
	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (!dev->driver->lock_device)
		return UFP_OK;

	return dev->driver->lock_device(dev->ifdev);
}

ufprog_status UFPROG_API ufprog_unlock_device(struct ufprog_device *dev)
{
	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (!dev->driver->unlock_device)
		return UFP_OK;

	return dev->driver->unlock_device(dev->ifdev);
}

ufprog_status UFPROG_API ufprog_reset_device(struct ufprog_device *dev)
{
	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (!dev->driver->reset_device)
		return UFP_OK;

	return dev->driver->reset_device(dev->ifdev);
}

ufprog_status UFPROG_API ufprog_cancel_transfer(struct ufprog_device *dev)
{
	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (!dev->driver->cancel_transfer)
		return UFP_OK;

	return dev->driver->cancel_transfer(dev->ifdev);
}

ufprog_status UFPROG_API ufprog_set_disconnect_cb(struct ufprog_device *dev, ufprog_dev_disconnect_cb cb, void *priv)
{
	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (!dev->driver->set_disconnect_cb)
		return UFP_OK;

	return dev->driver->set_disconnect_cb(dev->ifdev, cb, priv);
}
