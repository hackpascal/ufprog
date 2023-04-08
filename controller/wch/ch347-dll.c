/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Interface driver for CH347 using vendor library (Windows only)
 */

#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ufprog/api_controller.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include "ch34x-dll.h"
#include "ch347.h"

struct ch347_dll_open_info {
	uint32_t if_type;
	HANDLE hDevice;
	uint32_t devidx;
};

static const char *chip_modes[] = {
	"UART0+UART1",
	"UART1+SPI+I2C VCP",
	"UART1+SPI+I2C HID",
	"UART1+JTAG+I2C VCP",
};

ufprog_status UFPROG_API ufprog_plugin_init(void)
{
	if (ch347_dll_init())
		return UFP_FAIL;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_plugin_cleanup(void)
{
	ch34x_dll_deinit();

	return UFP_OK;
}

const char *UFPROG_API ufprog_plugin_desc(void)
{
	return "WCH CH347 (DLL)";
}

static HANDLE ch347_test_open(uint32_t if_type, uint32_t devidx, const char *path)
{
	mDeviceInforS DevInfo;
	HANDLE hDevice;

	hDevice = CH34xOpenDevice(devidx);
	if (hDevice == INVALID_HANDLE_VALUE) {
		logm_dbg("Failed to open device %u\n", devidx);
		return INVALID_HANDLE_VALUE;
	}

	if (!CH347GetDeviceInfor(devidx, &DevInfo)) {
		logm_warn("Failed to get device information of device %u\n", devidx);
		goto cleanup_dev;
	}

	if (path) {
		if (strcmp((char *)DevInfo.DevicePath, path)) {
			logm_dbg("Device path mismatch\n");
			goto cleanup_dev;
		}
	}

	switch (if_type) {
	case IF_SPI:
		if (DevInfo.FuncType != CH347_FUNC_SPI_I2C) {
			logm_err("Device %u is not in SPI mode\n", devidx);
			goto cleanup_dev;
		}
		break;

	case IF_I2C:
		if (DevInfo.FuncType != CH347_FUNC_SPI_I2C && DevInfo.FuncType != CH347_FUNC_JTAG_I2C) {
			logm_err("Device %u is not in I2C mode\n", devidx);
			goto cleanup_dev;
		}
		break;
	}

	logm_info("Opened device %u in %s mode\n", devidx, chip_modes[DevInfo.ChipMode & 3]);

	return hDevice;

cleanup_dev:
	CH347CloseDevice(devidx);

	return INVALID_HANDLE_VALUE;
}

static int UFPROG_API ch347_dll_try_match_open(void *priv, struct json_object *match, int index)
{
	struct ch347_dll_open_info *info = priv;
	const char *devpath;
	uint32_t devidx = 0;
	ufprog_status ret;

	if (!json_node_exists(match, "index")) {
		ret = json_read_str(match, "path", &devpath, NULL);
		if (ret == UFP_JSON_TYPE_INVALID) {
			if (index >= 0)
				logm_err("Invalid device path in match#%u\n", index);
			else
				logm_err("Invalid device path in matching data\n");
			return 0;
		}

		for (devidx = 0; devidx < 16; devidx++) {
			info->hDevice = ch347_test_open(info->if_type, devidx, devpath);
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

		info->hDevice = ch347_test_open(info->if_type, devidx, NULL);
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
	struct ch347_dll_open_info oi;
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

	oi.if_type = if_type;
	oi.hDevice = INVALID_HANDLE_VALUE;
	oi.devidx = 0;

	STATUS_CHECK_RET(json_array_foreach(config, "match", ch347_dll_try_match_open, &oi, NULL));

	if (oi.hDevice == INVALID_HANDLE_VALUE) {
		logm_errdbg("No matched device opened\n");
		return UFP_DEVICE_NOT_FOUND;
	}

	/* Maximum timeout */
	CH34xSetTimeout(oi.devidx, CH347_SPI_RW_TIMEOUT, CH347_SPI_RW_TIMEOUT);

	wchdev = calloc(1, sizeof(*wchdev) + sizeof(struct ch34x_handle));
	if (!wchdev) {
		logm_err("No memory for device object\n");
		ret = UFP_NOMEM;
		goto cleanup;
	}

	wchdev->handle = (struct ch34x_handle *)((uintptr_t)wchdev + sizeof(*wchdev));
	wchdev->handle->iIndex = oi.devidx;
	wchdev->max_payload_len = CH347_PACKET_LEN;

	STATUS_CHECK_GOTO_RET(ch347_init(wchdev, thread_safe), ret, cleanup);

	switch (if_type) {
	case IF_SPI:
		ret = json_read_obj(config, "spi", &ifcfg);
		if (ret) {
			logm_err("Invalid configuration for SPI interface\n");
			ret = UFP_DEVICE_INVALID_CONFIG;
			goto cleanup;
		}

		STATUS_CHECK_GOTO_RET(ch347_spi_init(wchdev, ifcfg), ret, cleanup);

		break;

	/* case IF_I2C: */
	}

	*outifdev = wchdev;
	return UFP_OK;

cleanup:
	CH347CloseDevice(oi.devidx);

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

	CH347CloseDevice(wchdev->handle->iIndex);

	if (wchdev->lock)
		os_free_mutex(wchdev->lock);

	free(wchdev);

	return UFP_OK;

}
