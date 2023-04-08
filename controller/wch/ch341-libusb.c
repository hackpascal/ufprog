/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Interface driver for CH341 using libusb library
 */

#include <malloc.h>
#include <ufprog/api_controller.h>
#include <ufprog/config.h>
#include <ufprog/libusb.h>
#include <ufprog/log.h>
#include "ch341.h"

#define CH341_BULK_EP_OUT		(CH341_USB_BULK_ENDPOINT | LIBUSB_ENDPOINT_OUT)
#define CH341_BULK_EP_IN		(CH341_USB_BULK_ENDPOINT | LIBUSB_ENDPOINT_IN)

 /* So, keep sync with vendor dll */
#define CH341_MAX_PACKET_SIZE		0x1000

struct ch34x_handle {
	struct libusb_device_handle *handle;
};

const char *UFPROG_API ufprog_plugin_desc(void)
{
	return "WCH CH341 (libusb)";
}

static int UFPROG_API ch341_libusb_try_match_open(void *priv, struct json_object *match, int index)
{
	struct libusb_device_handle **dev_handle = priv;
	struct libusb_match_info info;
	ufprog_status ret;

	ret = libusb_read_config(match, &info, true);
	if (ret) {
		if (index >= 0)
			logm_warn("libusb match#%u is invalid\n", index);
		else
			logm_warn("libusb matching data is invalid\n");
		return 0;
	}

	/* All we need are index and path */
	info.vid = CH341_USB_VID;
	info.pid = CH341_USB_PID;
	info.bcd_device = 0;
	info.match_bcd_device = false;

	if (info.serial) {
		free((void *)info.serial);
		info.serial = NULL;
	}

	if (info.product) {
		free((void *)info.product);
		info.product = NULL;
	}

	if (info.manufacturer) {
		free((void *)info.manufacturer);
		info.manufacturer = NULL;
	}

	ret = libusb_open_matched(ufprog_global_libusb_context(), &info, dev_handle);
	if (ret) {
		if (index >= 0)
			logm_dbg("Failed to open device specified by match#%u\n", index);
		else
			logm_dbg("Failed to open device specified by matching data\n");
		return 0;
	}

	return 1;
}

ufprog_status UFPROG_API ufprog_device_open(uint32_t if_type, struct json_object *config, ufprog_bool thread_safe,
					    struct ufprog_if_dev **outifdev)
{
	struct libusb_device_handle *dev_handle = NULL;
	struct ufprog_if_dev *wchdev;
	ufprog_status ret = UFP_OK;
	struct json_object *ifcfg;
	int err;

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

	STATUS_CHECK_RET(json_array_foreach(config, "match", ch341_libusb_try_match_open, &dev_handle, NULL));

	if (!dev_handle) {
		logm_errdbg("No matched device opened\n");
		return UFP_DEVICE_NOT_FOUND;
	}

	libusb_set_auto_detach_kernel_driver(dev_handle, 1);

	wchdev = calloc(1, sizeof(*wchdev) + sizeof(struct ch34x_handle));
	if (!wchdev) {
		logm_err("No memory for device object\n");
		ret = UFP_NOMEM;
		goto cleanup;
	}

	wchdev->handle = (struct ch34x_handle *)((uintptr_t)wchdev + sizeof(*wchdev));

	wchdev->handle->handle = dev_handle;

	STATUS_CHECK_GOTO_RET(ch341_init(wchdev, thread_safe), ret, cleanup);

	err = libusb_claim_interface(dev_handle, 0);
	if (err < 0) {
		logm_err("Unable to claim interface: %s\n", libusb_strerror(err));
		goto cleanup;
	}

	switch (if_type) {
	case IF_SPI:
		ret = json_read_obj(config, "spi", &ifcfg);
		if (ret) {
			logm_err("Invalid configuration for SPI interface\n");
			ret = UFP_DEVICE_INVALID_CONFIG;
			goto cleanup_handle;
		}

		STATUS_CHECK_GOTO_RET(ch341_spi_init(wchdev, ifcfg), ret, cleanup_handle);

		break;

	/* case IF_I2C: */
	}

	*outifdev = wchdev;
	return UFP_OK;

cleanup_handle:
	libusb_release_interface(dev_handle, 0);

cleanup:
	libusb_close(dev_handle);

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

	if (wchdev->handle->handle) {
		libusb_release_interface(wchdev->handle->handle, 0);
		libusb_close(wchdev->handle->handle);
	}

	if (wchdev->lock)
		os_free_mutex(wchdev->lock);

	free(wchdev);

	return UFP_OK;
}

ufprog_status ch341_write(struct ch34x_handle *handle, const void *buf, size_t len, size_t *retlen)
{
	int ret, transferred;

	if (len > CH341_MAX_PACKET_SIZE)
		return UFP_INVALID_PARAMETER;

	ret = libusb_bulk_transfer(handle->handle, CH341_BULK_EP_OUT, (uint8_t *)buf, (int)len, &transferred,
				   CH341_RW_TIMEOUT);
	if (ret) {
		logm_warn("Incomplete bulk data transfer through usb: %s, %u of %lu written\n",
			  libusb_strerror(ret), transferred, len);
		return UFP_DEVICE_IO_ERROR;
	}

	if (retlen)
		*retlen = len;

	return UFP_OK;
}

ufprog_status ch341_read(struct ch34x_handle *handle, void *buf, size_t len, size_t *retlen)
{
	int ret, transferred;

	if (len > CH341_MAX_PACKET_SIZE)
		return UFP_INVALID_PARAMETER;

	ret = libusb_bulk_transfer(handle->handle, CH341_BULK_EP_IN, buf, (int)len, &transferred, CH341_RW_TIMEOUT);
	if (ret && ret != LIBUSB_ERROR_TIMEOUT) {
		logm_warn("Failed bulk data transfer through usb: %s, %u read\n", libusb_strerror(ret),
			  transferred);
		return UFP_DEVICE_IO_ERROR;
	}

	if (retlen)
		*retlen = len;

	return UFP_OK;
}
