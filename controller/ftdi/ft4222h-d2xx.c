/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Interface driver for FT4222H using D2XX library (Windows only)
 */

#include <malloc.h>
#include <ufprog/api_controller.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include "ft4222h.h"
#include "d2xx.h"

ufprog_status UFPROG_API ufprog_plugin_init(void)
{
	if (d2xx_init())
		return UFP_FAIL;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_plugin_cleanup(void)
{
	d2xx_deinit();

	return UFP_OK;
}

const char *UFPROG_API ufprog_plugin_desc(void)
{
	return "FTDI FT4222H (D2XX)";
}

ufprog_status UFPROG_API ufprog_device_open(uint32_t if_type, struct json_object *config, ufprog_bool thread_safe,
					    struct ufprog_interface **outifdev)
{
	struct ufprog_interface *ftdev;
	FT_HANDLE ftHandle = NULL;
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

	STATUS_CHECK_RET(json_array_foreach(config, "match", ftdi_d2xx_try_match_open, &ftHandle, NULL));

	if (!ftHandle) {
		logm_errdbg("No matched device opened\n");
		return UFP_DEVICE_NOT_FOUND;
	}

	ftdev = calloc(1, sizeof(*ftdev) + sizeof(struct ft_handle));
	if (!ftdev) {
		logm_err("No memory for device object\n");
		ret = UFP_NOMEM;
		goto cleanup;
	}

	ftdev->handle = (struct ft_handle *)((uintptr_t)ftdev + sizeof(*ftdev));
	ftdev->handle->ftHandle = ftHandle;

	STATUS_CHECK_GOTO_RET(ft4222_init(ftdev, thread_safe), ret, cleanup);

	switch (if_type) {
	case IF_SPI:
		ret = json_read_obj(config, "spi", &ifcfg);
		if (ret) {
			logm_err("Invalid configuration for SPI interface\n");
			ret = UFP_DEVICE_INVALID_CONFIG;
			goto cleanup;
		}

		STATUS_CHECK_GOTO_RET(ft4222_spi_master_init(ftdev, ifcfg), ret, cleanup);

		break;

	/* case IF_I2C: */
	}

	*outifdev = ftdev;
	return UFP_OK;

cleanup:
	FT_Close(ftHandle);

	if (ftdev) {
		if (ftdev->lock)
			os_free_mutex(ftdev->lock);

		free(ftdev);
	}

	return ret;
}

ufprog_status UFPROG_API ufprog_device_free(struct ufprog_interface *ftdev)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	switch (ftdev->hwcaps.function_mode) {
	case FT4222_SPI_MASTER:
		ft4222_spi_master_cleanup(ftdev);
		break;
	}

	if (ftdev->handle->ftHandle)
		FT_Close(ftdev->handle->ftHandle);

	if (ftdev->lock)
		os_free_mutex(ftdev->lock);

	free(ftdev);

	return UFP_OK;

}
