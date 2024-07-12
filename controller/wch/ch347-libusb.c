/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Interface driver for CH347 UART1+SPI_I2C VCP mode using libusb library
 */

#include <malloc.h>
#include <stdbool.h>
#include <ufprog/api_controller.h>
#include <ufprog/lookup_table.h>
#include <ufprog/config.h>
#include <ufprog/libusb.h>
#include <ufprog/log.h>
#include <ufprog/string.h>
#include "ch347.h"

#define CH347_VCP_SPI_VID		0x1a86
#define CH347T_VCP_SPI_PID		0x55db
#define CH347F_VCP_SPI_PID		0x55de
#define CH347T_VCP_SPI_IF		2
#define CH347F_VCP_SPI_IF		4
#define CH347_VCP_EP_OUT		(6 | LIBUSB_ENDPOINT_OUT)
#define CH347_VCP_EP_IN			(6 | LIBUSB_ENDPOINT_IN)

/* So, keep sync with vendor dll */
#define CH347_MAX_PACKET_SIZE		0x1000

struct ch347_libusb_open_info {
	struct libusb_device_handle *dev_handle;
	int interface_number;
};

struct ch34x_handle {
	struct libusb_device_handle *handle;
	int interface_number;
};

const char *UFPROG_API ufprog_plugin_desc(void)
{
	return "WCH CH347 (libusb)";
}

static int UFPROG_API ch347_libusb_try_match_open(void *priv, struct json_object *match, int index)
{
	struct ch347_libusb_open_info *openinfo = priv;
	struct libusb_match_info info;
	bool use_ch347t = true;
	ufprog_status ret;

	ret = libusb_read_config(match, &info, true);
	if (ret) {
		if (index >= 0)
			logm_warn("libusb match#%u is invalid\n", index);
		else
			logm_warn("libusb matching data is invalid\n");
		return 0;
	}

	/* Device model */
	if (json_node_exists(match, "model")) {
		const char *model = NULL;

		ret = json_read_str(match, "model", &model, NULL);
		if (!ret && *model) {
			if (!strcasecmp(model, "ch347t")) {
				use_ch347t = true;
			} else if (!strcasecmp(model, "ch347f")) {
				use_ch347t = false;
			} else {
				logm_warn("Invalid device model '%s'\n", model ? model : "<NULL>");
				return 0;
			}
		}
	}

	if (use_ch347t) {
		info.pid = CH347T_VCP_SPI_PID;
		openinfo->interface_number = CH347T_VCP_SPI_IF;
	} else {
		info.pid = CH347F_VCP_SPI_PID;
		openinfo->interface_number = CH347F_VCP_SPI_IF;
	}

	/* All we need are index and path */
	info.vid = CH347_VCP_SPI_VID;
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

	ret = libusb_open_matched(ufprog_global_libusb_context(), &info, &openinfo->dev_handle);
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
					    struct ufprog_interface **outifdev)
{
	struct ch347_libusb_open_info openinfo;
	struct ufprog_interface *wchdev;
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

	openinfo.dev_handle = NULL;
	openinfo.interface_number = 0;

	STATUS_CHECK_RET(json_array_foreach(config, "match", ch347_libusb_try_match_open, &openinfo, NULL));

	if (!openinfo.dev_handle) {
		logm_errdbg("No matched device opened\n");
		return UFP_DEVICE_NOT_FOUND;
	}

	libusb_set_auto_detach_kernel_driver(openinfo.dev_handle, 1);

	wchdev = calloc(1, sizeof(*wchdev) + sizeof(struct ch34x_handle));
	if (!wchdev) {
		logm_err("No memory for device object\n");
		ret = UFP_NOMEM;
		goto cleanup;
	}

	wchdev->handle = (struct ch34x_handle *)((uintptr_t)wchdev + sizeof(*wchdev));

	wchdev->handle->handle = openinfo.dev_handle;
	wchdev->max_payload_len = CH347_PACKET_LEN - CH347_SPI_CMD_LEN;

	STATUS_CHECK_GOTO_RET(ch347_init(wchdev, thread_safe), ret, cleanup);

	err = libusb_claim_interface(openinfo.dev_handle, openinfo.interface_number);
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

		STATUS_CHECK_GOTO_RET(ch347_spi_init(wchdev, ifcfg), ret, cleanup_handle);
		break;

	/* case IF_I2C: */
	}

	wchdev->handle->interface_number = openinfo.interface_number;

	*outifdev = wchdev;
	return UFP_OK;

cleanup_handle:
	libusb_release_interface(wchdev->handle->handle, openinfo.interface_number);

cleanup:
	libusb_close(openinfo.dev_handle);

	if (wchdev) {
		if (wchdev->lock)
			os_free_mutex(wchdev->lock);

		free(wchdev);
	}

	return ret;
}

ufprog_status UFPROG_API ufprog_device_free(struct ufprog_interface *wchdev)
{
	if (!wchdev)
		return UFP_INVALID_PARAMETER;

	if (wchdev->handle->handle) {
		libusb_release_interface(wchdev->handle->handle, wchdev->handle->interface_number);
		libusb_close(wchdev->handle->handle);
	}

	if (wchdev->lock)
		os_free_mutex(wchdev->lock);

	free(wchdev);

	return UFP_OK;
}

ufprog_status ch347_write(struct ch34x_handle *handle, const void *buf, size_t len, size_t *retlen)
{
	int ret, transferred;

	if (len > CH347_MAX_PACKET_SIZE)
		return UFP_INVALID_PARAMETER;

	ret = libusb_bulk_transfer(handle->handle, CH347_VCP_EP_OUT, (uint8_t *)buf, (int)len, &transferred,
				   CH347_SPI_RW_TIMEOUT);
	if (ret) {
		logm_warn("Incomplete bulk data transfer through usb: %s, %u of %lu written\n",
			  libusb_strerror(ret), transferred, len);
		return UFP_DEVICE_IO_ERROR;
	}

	if (retlen)
		*retlen = len;

	return UFP_OK;
}

ufprog_status ch347_read(struct ch34x_handle *handle, void *buf, size_t len, size_t *retlen)
{
	int ret, transferred;

	if (len > CH347_MAX_PACKET_SIZE)
		return UFP_INVALID_PARAMETER;

	ret = libusb_bulk_transfer(handle->handle, CH347_VCP_EP_IN, buf, (int)len, &transferred, CH347_SPI_RW_TIMEOUT);
	if (ret && ret != LIBUSB_ERROR_TIMEOUT) {
		logm_warn("Failed bulk data transfer through usb: %s, %u read\n", libusb_strerror(ret),
			  transferred);
		return UFP_DEVICE_IO_ERROR;
	}

	if (retlen)
		*retlen = len;

	return UFP_OK;
}
