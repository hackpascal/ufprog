/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Interface driver for CH341 using vendor library (Windows only)
 */

#include <malloc.h>
#include <ufprog/api_controller.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include "ch34x-dll.h"
#include "ch341.h"

struct ch341_dll_open_info {
	HANDLE hDevice;
	uint32_t devidx;
};

ufprog_status UFPROG_API ufprog_driver_init(void)
{
	if (ch341_dll_init())
		return UFP_FAIL;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_driver_cleanup(void)
{
	ch34x_dll_deinit();

	return UFP_OK;
}

const char *UFPROG_API ufprog_driver_desc(void)
{
	return "WCH CH341 (DLL)";
}

static int UFPROG_API ch341_dll_try_match_open(void *priv, struct json_object *match, int index)
{
	struct ch341_dll_open_info *info = priv;
	uint32_t devidx = 0;
	ufprog_status ret;

	if (!json_node_exists(match, "index")) {
		for (devidx = 0; devidx < 16; devidx++) {
			info->hDevice = CH34xOpenDevice(devidx);
			if (info->hDevice != INVALID_HANDLE_VALUE)
				goto success;
		}

		if (index >= 0)
			logm_warn("No device specified by match#%u could be opened\n", index);
		else
			logm_warn("No device specified by matching data could be opened\n");

		return 0;
	} else {
		ret = json_read_uint32(match, "index", &devidx, 0);
		if (ret) {
			if (index >= 0)
				logm_err("Invalid type of device index in match#%u\n", index);
			else
				logm_err("Invalid type of device index in matching data %u\n");
			return 0;
		}

		info->hDevice = CH34xOpenDevice(devidx);
		if (info->hDevice == INVALID_HANDLE_VALUE) {
			logm_warn("Device %u specified by match#%u could be opened\n", devidx, index);
			return 0;
		}
	}

success:
	info->devidx = devidx;
	return 1;
}

ufprog_status UFPROG_API ufprog_device_open(uint32_t if_type, struct json_object *config, ufprog_bool thread_safe,
					    struct ufprog_if_dev **outifdev)
{
	struct ch341_dll_open_info oi;
	struct ufprog_if_dev *wchdev;
	struct json_object *ifcfg;
	ufprog_status ret;

	if (!outifdev)
		return UFP_INVALID_PARAMETER;

	*outifdev = NULL;

	switch (if_type) {
	case IF_SPI:
	/* case IF_I2C: */
		break;

	default:
		return UFP_UNSUPPORTED;
	}

	if (!config) {
		logm_err("Device connection configuration required\n");
		return UFP_DEVICE_MISSING_CONFIG;
	}

	oi.hDevice = INVALID_HANDLE_VALUE;
	oi.devidx = 0;

	STATUS_CHECK_RET(json_array_foreach(config, "match", ch341_dll_try_match_open, &oi, NULL));

	if (oi.hDevice == INVALID_HANDLE_VALUE) {
		logm_errdbg("No matched device opened\n");
		return UFP_DEVICE_NOT_FOUND;
	}

	logm_info("Opened device %u\n", oi.devidx);

	/* Maximum timeout */
	CH34xSetTimeout(oi.devidx, CH341_RW_TIMEOUT, CH341_RW_TIMEOUT);

	wchdev = calloc(1, sizeof(*wchdev) + sizeof(struct ch34x_handle));
	if (!wchdev) {
		logm_err("No memory for device object\n");
		ret = UFP_NOMEM;
		goto cleanup;
	}

	wchdev->handle = (struct ch34x_handle *)((uintptr_t)wchdev + sizeof(*wchdev));
	wchdev->handle->iIndex = oi.devidx;

	STATUS_CHECK_GOTO_RET(ch341_init(wchdev, thread_safe), ret, cleanup);

	switch (if_type) {
	case IF_SPI:
		ret = json_read_obj(config, "spi", &ifcfg);
		if (ret) {
			logm_err("Invalid configuration for SPI interface\n");
			ret = UFP_DEVICE_INVALID_CONFIG;
			goto cleanup;
		}

		STATUS_CHECK_GOTO_RET(ch341_spi_init(wchdev, ifcfg), ret, cleanup);

		break;

	/* case IF_I2C: */
	}

	*outifdev = wchdev;
	return UFP_OK;

cleanup:
	CH341CloseDevice(oi.devidx);

	if (wchdev) {
		if (wchdev->lock)
			os_free_mutex(wchdev->lock);

		free(wchdev);
	}

	return ret;
}

ufprog_status UFPROG_API ufprog_device_free(struct ufprog_if_dev *wchdev)
{
	if (!wchdev)
		return UFP_INVALID_PARAMETER;

	CH341CloseDevice(wchdev->handle->iIndex);

	if (wchdev->lock)
		os_free_mutex(wchdev->lock);

	free(wchdev);

	return UFP_OK;

}
