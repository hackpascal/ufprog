/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Interface driver for CH347 UART1+SPI_I2C VCP mode using libusb library
 */

#include <malloc.h>
#include <string.h>
#include <ufprog/api_controller.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include <hidapi/hidapi.h>
#include "ch347.h"

#define CH347_HID_SPI_VID		0x1a86
#define CH347_HID_SPI_PID		0x55dc
#define CH347_HID_SPI_IF		1
#define CH347_HID_OUT_REPORT_ID		2
#define CH347_HID_REPORT_SIZE		512
#define CH347_HID_REPORT_HDR_LEN	2

#define CH347_HID_PACKET_SIZE		(CH347_HID_REPORT_SIZE - CH347_HID_REPORT_HDR_LEN - CH347_SPI_CMD_LEN)

struct ch347_hid_open_info {
	struct hid_device_info *hiddevinfo;
	hid_device *hiddev;
};

struct ch34x_handle {
	hid_device *dev;
	uint8_t report[CH347_HID_REPORT_SIZE + 1];
};

ufprog_status UFPROG_API ufprog_driver_init(void)
{
	int ret;

	ret = hid_init();
	if (ret < 0) {
		logm_err("HIDAPI initialization failed: %S\n", hid_error(NULL));
		return UFP_FAIL;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_driver_cleanup(void)
{
	hid_exit();

	return UFP_OK;
}

const char *UFPROG_API ufprog_driver_desc(void)
{
	return "WCH CH347 (HID)";
}

static hid_device *ch347_hid_open(struct hid_device_info *devinfo, const char *hidpath)
{
	const char *valid_hidpath = NULL;
	struct hid_device_info *devp;
	hid_device *hiddev;

	devp = devinfo;
	while (devp) {
		if (devp->vendor_id == CH347_HID_SPI_VID && devp->product_id == CH347_HID_SPI_PID &&
		    devp->interface_number == CH347_HID_SPI_IF) {
			if (!hidpath) {
				valid_hidpath = devp->path;
				break;
			}

			if (!strcmp(devp->path, hidpath)) {
				valid_hidpath = devp->path;
				break;
			}
		}

		devp = devp->next;
	}

	if (!valid_hidpath) {
		if (hidpath)
			logm_warn("No HID device with path '%'s found\n", hidpath);
		return NULL;
	}

	hiddev = hid_open_path(valid_hidpath);
	if (!hiddev)
		logm_err("Unable to open HID device with path '%s': %S\n", valid_hidpath, hid_error(NULL));

	return hiddev;
}

static int UFPROG_API ch347_hid_try_match_open(void *priv, struct json_object *match, int index)
{
	struct ch347_hid_open_info *oi = priv;
	const char *hidpath;
	ufprog_status ret;

	ret = json_read_str(match, "path", &hidpath, NULL);
	if (ret == UFP_JSON_TYPE_INVALID) {
		if (index >= 0)
			logm_warn("Invalid HID path in match#%u\n", index);
		else
			logm_warn("Invalid HID path in matching data\n", index);
		return 0;
	}

	oi->hiddev = ch347_hid_open(oi->hiddevinfo, hidpath);
	if (!oi->hiddev)
		return 0;

	return 1;
}

ufprog_status UFPROG_API ufprog_device_open(uint32_t if_type, struct json_object *config, ufprog_bool thread_safe,
					    struct ufprog_if_dev **outifdev)
{
	struct hid_device_info *hiddevinfo;
	struct ch347_hid_open_info oi;
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

	hiddevinfo = hid_enumerate(CH347_HID_SPI_VID, CH347_HID_SPI_PID);
	if (!hiddevinfo) {
		logm_err("Unable to enumerate HID devices: %S\n", hid_error(NULL));
		return UFP_DEVICE_IO_ERROR;
	}

	oi.hiddevinfo = hiddevinfo;
	oi.hiddev = NULL;

	ret = json_array_foreach(config, "match", ch347_hid_try_match_open, &oi, NULL);
	hid_free_enumeration(hiddevinfo);

	if (ret)
		return ret;

	if (!oi.hiddev) {
		logm_errdbg("No matched device opened\n");
		return UFP_DEVICE_NOT_FOUND;
	}

	wchdev = calloc(1, sizeof(*wchdev) + sizeof(struct ch34x_handle));
	if (!wchdev) {
		logm_err("No memory for device object\n");
		ret = UFP_NOMEM;
		goto cleanup;
	}

	wchdev->handle = (struct ch34x_handle *)((uintptr_t)wchdev + sizeof(*wchdev));

	wchdev->handle->dev = oi.hiddev;
	wchdev->max_payload_len = CH347_HID_PACKET_SIZE;

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
	hid_close(oi.hiddev);

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

	if (wchdev->handle->dev)
		hid_close(wchdev->handle->dev);

	if (wchdev->lock)
		os_free_mutex(wchdev->lock);

	free(wchdev);

	return UFP_OK;
}

ufprog_status ch347_write(struct ch34x_handle *handle, const void *buf, size_t len, size_t *retlen)
{
	int ret;

	if (len > CH347_HID_REPORT_SIZE - CH347_HID_REPORT_HDR_LEN)
		return UFP_INVALID_PARAMETER;

	handle->report[0] = CH347_HID_OUT_REPORT_ID;
	handle->report[1] = len & 0xff;
	handle->report[2] = (len >> 8) & 0xff;
	memcpy(handle->report + CH347_HID_REPORT_HDR_LEN + 1, buf, len);

	ret = hid_write(handle->dev, handle->report, len + CH347_HID_REPORT_HDR_LEN + 1);
	if (ret < 0) {
		logm_err("Failed to write report: %S\n", hid_error(handle->dev));
		return UFP_DEVICE_IO_ERROR;
	}

	if (retlen)
		*retlen = len;

	return UFP_OK;
}

ufprog_status ch347_read(struct ch34x_handle *handle, void *buf, size_t len, size_t *retlen)
{
	uint32_t report_len;
	int ret;

	if (len > CH347_HID_REPORT_SIZE - CH347_HID_REPORT_HDR_LEN)
		len = CH347_HID_REPORT_SIZE - CH347_HID_REPORT_HDR_LEN;

	ret = hid_read(handle->dev, handle->report, len + CH347_HID_REPORT_HDR_LEN + 1);
	if (ret < 0) {
		logm_err("Failed to read report: %S\n", hid_error(handle->dev));
		return UFP_DEVICE_IO_ERROR;
	}

	report_len = handle->report[0] | ((uint32_t)handle->report[1] << 8);

	if (report_len > CH347_HID_REPORT_SIZE - CH347_HID_REPORT_HDR_LEN) {
		logm_err("In report length field is too big: %u returned\n", report_len);
		return UFP_DEVICE_IO_ERROR;
	}

	if (report_len > len) {
		logm_warn("In report is bigger than requested length: %lu returned, only %u requested\n",
			 report_len, len);
	} else if (report_len < len) {
		memset(handle->report + CH347_HID_REPORT_HDR_LEN + report_len, 0, len - report_len);
	}

	memcpy(buf, handle->report + CH347_HID_REPORT_HDR_LEN, len);

	if (retlen)
		*retlen = len;

	return UFP_OK;
}
