/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Interface driver for FT4222H using libusb library
 */

#include <malloc.h>
#include <ufprog/api_controller.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include "ftdi-libusb.h"
#include "ft4222h.h"

/* The value should be >= max read size of both single and multi I/O */
#define FT4222_MAX_READ_SIZE			0x10000

const char *UFPROG_API ufprog_plugin_desc(void)
{
	return "FTDI FT4222H (libusb)";
}

ufprog_status UFPROG_API ufprog_device_open(uint32_t if_type, struct json_object *config, ufprog_bool thread_safe,
					    struct ufprog_if_dev **outifdev)
{
	struct ftdi_libusb_open_info oi;
	struct json_object *ifcfg;
	struct ufprog_if_dev *ftdev;
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
						FT4222_MAX_READ_SIZE), ret, cleanup);

	STATUS_CHECK_GOTO_RET(ft4222_init(ftdev, thread_safe), ret, cleanup_handle);

	switch (if_type) {
	case IF_SPI:
		ret = json_read_obj(config, "spi", &ifcfg);
		if (ret) {
			logm_err("Invalid configuration for SPI interface\n");
			ret = UFP_DEVICE_INVALID_CONFIG;
			goto cleanup_handle;
		}

		STATUS_CHECK_GOTO_RET(ft4222_spi_master_init(ftdev, ifcfg), ret, cleanup_handle);
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

	switch (ftdev->hwcaps.function_mode) {
	case FT4222_SPI_MASTER:
		ft4222_spi_master_cleanup(ftdev);
		break;
	}

	ftdi_cleanup_handle(ftdev->handle);

	if (ftdev->handle->handle) {
		libusb_release_interface(ftdev->handle->handle, ftdev->handle->interface_number);
		libusb_close(ftdev->handle->handle);
	}

	if (ftdev->lock)
		os_free_mutex(ftdev->lock);

	free(ftdev);

	return UFP_OK;
}
