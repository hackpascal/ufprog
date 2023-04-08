/* SPDX-License-Identifier: LGPL-2.1-only */
/*
* Author: Weijie Gao <hackpascal@gmail.com>
*
* Interface driver for MPSSE (FT232H/FT2232/FT4232) using libusb library
*/

#include <malloc.h>
#include <ufprog/api_controller.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include "ftdi-libusb.h"
#include "mpsse.h"

/* The value should be >= max read size of both single and multi I/O */
#define MPSSE_MAX_READ_SIZE			0x10000

const char *UFPROG_API ufprog_plugin_desc(void)
{
	return "FTDI MPSSE (libusb)";
}

ufprog_status UFPROG_API ufprog_device_open(uint32_t if_type, struct json_object *config, ufprog_bool thread_safe,
					    struct ufprog_if_dev **outifdev)
{
	struct ftdi_libusb_open_info oi;
	struct ufprog_if_dev *ftdev;
	struct json_object *ifcfg;
	uint32_t max_interfaces;
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

	oi.ctx = ufprog_global_libusb_context();
	oi.handle = NULL;
	oi.interface_number = 0;

	ret = json_array_foreach(config, "match", ftdi_libusb_try_match_open, &oi, NULL);
	if (ret)
		return ret;

	if (!oi.handle) {
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

	STATUS_CHECK_GOTO_RET(ftdi_setup_handle(ftdev->handle, oi.handle, (uint8_t)oi.interface_number, 0,
						MPSSE_MAX_READ_SIZE), ret, cleanup);

	ret = ftdi_get_mpsse_chip(ftdev->handle, &ftdev->chip);
	if (ret) {
		if (ret == UFP_UNSUPPORTED)
			logm_err("Unsupported chip model for MPSSE\n");
		else
			logm_err("Unable to get FTDI chip model\n");

		return ret;
	}

	switch (ftdev->chip) {
	case FT232H:
		max_interfaces = 1;
		break;

	case FT2232C:
	case FT2232H:
	case FT4232H:
		max_interfaces = 2;
		break;

	default:
		max_interfaces = 0; /* Not able to go there */
	}

	if (ftdev->handle->interface_number >= max_interfaces) {
		logm_err("Selected interface does not support MPSSE\n");
		return UFP_UNSUPPORTED;
	}

	ret = json_read_bool(config, "3-phase-clock", &ftdev->three_phase);
	if (ret) {
		logm_err("Invalid configuration for 3 phase clocking\n");
		return UFP_DEVICE_INVALID_CONFIG;
	}

	STATUS_CHECK_GOTO_RET(mpsse_init(ftdev, thread_safe), ret, cleanup_handle);

	switch (if_type) {
	case IF_SPI:
		ret = json_read_obj(config, "spi", &ifcfg);
		if (ret) {
			logm_err("Invalid configuration for SPI interface\n");
			ret = UFP_DEVICE_INVALID_CONFIG;
			goto cleanup_handle;
		}

		STATUS_CHECK_GOTO_RET(mpsse_spi_init(ftdev, ifcfg), ret, cleanup_handle);

		break;

	/* case IF_I2C: */
	}

	*outifdev = ftdev;
	return UFP_OK;

cleanup_handle:
	libusb_release_interface(oi.handle, oi.interface_number);
	ftdi_cleanup_handle(ftdev->handle);

cleanup:
	libusb_close(oi.handle);

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

	ftdi_cleanup_handle(ftdev->handle);

	if (ftdev->handle->handle) {
		libusb_release_interface(ftdev->handle->handle, ftdev->handle->interface_number);
		libusb_close(ftdev->handle->handle);
	}

	free(ftdev);

	return UFP_OK;
}
