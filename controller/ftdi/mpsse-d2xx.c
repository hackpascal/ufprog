/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Interface driver for MPSSE (FT232H/FT2232/FT4232) using D2XX library (Windows only)
 */

#include <malloc.h>
#include <ufprog/api_controller.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include "mpsse.h"
#include "d2xx.h"

ufprog_status UFPROG_API ufprog_driver_init(void)
{
	if (d2xx_init())
		return UFP_FAIL;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_driver_cleanup(void)
{
	d2xx_deinit();

	return UFP_OK;
}

const char *UFPROG_API ufprog_driver_desc(void)
{
	return "FTDI MPSSE (D2XX)";
}

ufprog_status UFPROG_API ufprog_device_open(uint32_t if_type, struct json_object *config, ufprog_bool thread_safe,
					    struct ufprog_if_dev **outifdev)
{
	struct ufprog_if_dev *ftdev;
	struct json_object *ifcfg;
	FT_HANDLE ftHandle = NULL;
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

	ret = ftdi_get_mpsse_chip(ftdev->handle, &ftdev->chip);
	if (ret) {
		if (ret == UFP_UNSUPPORTED)
			logm_err("Unsupported chip model for MPSSE\n");
		else
			logm_err("Unable to get FTDI chip model\n");

		return ret;
	}

	ret = json_read_bool(config, "3-phase-clock", &ftdev->three_phase);
	if (ret) {
		logm_err("Invalid configuration for 3 phase clocking\n");
		return UFP_DEVICE_INVALID_CONFIG;
	}

	STATUS_CHECK_GOTO_RET(mpsse_init(ftdev, thread_safe), ret, cleanup);

	switch (if_type) {
	case IF_SPI:
		ret = json_read_obj(config, "spi", &ifcfg);
		if (ret) {
			logm_err("Invalid configuration for SPI interface\n");
			ret = UFP_DEVICE_INVALID_CONFIG;
			goto cleanup;
		}

		STATUS_CHECK_GOTO_RET(mpsse_spi_init(ftdev, ifcfg), ret, cleanup);
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

ufprog_status UFPROG_API ufprog_device_free(struct ufprog_if_dev *ftdev)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	mpsse_cleanup(ftdev);

	if (ftdev->handle->ftHandle)
		FT_Close(ftdev->handle->ftHandle);

	free(ftdev);

	return UFP_OK;

}
