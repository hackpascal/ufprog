/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Common helpers using libusb
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ufprog/libusb.h>
#include <ufprog/log.h>

static struct libusb_context *global_ctx;

int libusb_global_init(void)
{
	int ret;

	ret = libusb_init(&global_ctx);
	if (ret < 0) {
		log_err("libusb initialization failed: %s\n", libusb_strerror(ret));
		return -1;
	}

	return 0;
}

struct libusb_context *UFPROG_API ufprog_global_libusb_context(void)
{
	return global_ctx;
}

ufprog_status UFPROG_API libusb_port_path_to_str(const uint8_t *port_path, uint32_t depth, char *pathstr)
{
	uint32_t i;

	if (!pathstr)
		return UFP_INVALID_PARAMETER;

	*pathstr = 0;

	if (!port_path)
		return UFP_INVALID_PARAMETER;

	if (depth > USB_PATH_LEVEL)
		depth = USB_PATH_LEVEL;

	for (i = 0; i < depth; i++) {
		snprintf(pathstr, 3, "%02X", port_path[i]);
		pathstr += 2;
	}

	return 0;
}

ufprog_status UFPROG_API libusb_open_matched(struct libusb_context *ctx, const struct libusb_match_info *info,
					     struct libusb_device_handle **outhandle)
{
	static libusb_device_handle *dev_handle, *found_dev_handle = NULL;
	struct libusb_device_descriptor desc;
	uint8_t path[USB_PATH_LEVEL];
	uint32_t index = info->index;
	libusb_device **udevs;
	ssize_t num_udevs, i;
	char str[256];
	int ret;

	if (!outhandle)
		return UFP_INVALID_PARAMETER;

	*outhandle = NULL;

	if (!ctx || !info)
		return UFP_INVALID_PARAMETER;

	num_udevs = libusb_get_device_list(ctx, &udevs);
	if (num_udevs < 0) {
		log_err("Failed to list all usb devices: %s\n", libusb_strerror((int)num_udevs));
		return UFP_FAIL;
	}

	for (i = 0; i < num_udevs; i++) {
		ret = libusb_get_device_descriptor(udevs[i], &desc);
		if (ret < 0) {
			log_errdbg("Failed to get descriptor of device %lu: %s\n", i, libusb_strerror(ret));
			continue;
		}

		if (desc.idVendor != info->vid || desc.idProduct != info->pid) {
			log_errdbg("Skipping device with %04x:%04x which is not matched with %04x:%04x\n",
				   desc.idVendor, desc.idProduct, info->vid, info->pid);
			continue;
		}

		ret = libusb_get_port_numbers(udevs[i], path, USB_PATH_LEVEL);
		if (ret < 0) {
			log_err("Failed to get device port path: %s\n", libusb_strerror(ret));
			continue;
		}

		libusb_port_path_to_str(path, ret, str);

		log_dbg("Found device matches %04x:%04x, port path is %s\n", info->vid, info->pid, str);

		if (info->match_path) {
			if (strcasecmp(str, info->path)) {
				log_dbg("Device port patch mismatch\n");
				continue;
			}
		}

		ret = libusb_open(udevs[i], &dev_handle);
		if (ret < 0) {
			log_errdbg("Failed to open device: %s\n", libusb_strerror(ret));
			continue;
		}

		if (info->serial) {
			ret = libusb_get_string_descriptor_ascii(dev_handle, desc.iSerialNumber, (uint8_t *)str,
								 sizeof(str));
			if (ret < 0) {
				log_err("Failed to get serial number of device\n", libusb_strerror(ret));
				goto udev_cleanup;
			}

			log_dbg("Device serial number: %s\n", str);

			if (strcmp(str, info->serial)) {
				log_dbg("Device serial number mismatch\n");
				goto udev_cleanup;
			}
		}

		if (info->product) {
			ret = libusb_get_string_descriptor_ascii(dev_handle, desc.iProduct, (uint8_t *)str,
								 sizeof(str));
			if (ret < 0) {
				log_err("Failed to get product string of device\n", libusb_strerror(ret));
				goto udev_cleanup;
			}

			log_dbg("Device product string: %s\n", str);

			if (strcmp(str, info->product)) {
				log_dbg("Device product string mismatch\n");
				goto udev_cleanup;
			}
		}

		if (info->manufacturer) {
			ret = libusb_get_string_descriptor_ascii(dev_handle, desc.iManufacturer, (uint8_t *)str,
								 sizeof(str));
			if (ret < 0) {
				log_err("Failed to get manufacturer string of device\n", libusb_strerror(ret));
				goto udev_cleanup;
			}

			log_dbg("Device manufacturer string: %s\n", str);

			if (strcmp(str, info->manufacturer)) {
				log_dbg("Device manufacturer string mismatch\n");
				goto udev_cleanup;
			}
		}

		if (index) {
			index--;
			goto udev_cleanup;
		}

		found_dev_handle = dev_handle;
		break;

	udev_cleanup:
		libusb_close(dev_handle);
	}

	*outhandle = found_dev_handle;
	libusb_free_device_list(udevs, 1);

	return found_dev_handle ? UFP_OK : UFP_DEVICE_NOT_FOUND;
}

ufprog_status UFPROG_API libusb_read_config(struct json_object *config, struct libusb_match_info *info,
					    ufprog_bool vidpid_optional)
{
	ufprog_status ret;
	const char *str;
	uint32_t val;
	size_t len;
	char *end;

	memset(info, 0, sizeof(*info));

	/* Vendor Id */
	if (!json_node_exists(config, "vid")) {
		if (vidpid_optional)
			goto read_pid;

		log_err("Vendor Id (vid) is missing in device connection configuration\n");
		return UFP_DEVICE_MISSING_CONFIG;
	}

	ret = json_read_hex32(config, "vid", &val, 0);
	if (ret) {
		log_err("Vendor Id (vid) is invalid in device connection configuration\n");
		return UFP_DEVICE_INVALID_CONFIG;
	}

	if (val > 0xffff) {
		log_err("Vendor Id (0x%x) is invalid in device connection configuration\n", val);
		return UFP_DEVICE_INVALID_CONFIG;
	}

	info->vid = (uint16_t)val;

read_pid:
	/* Product Id */
	if (!json_node_exists(config, "pid")) {
		if (vidpid_optional)
			goto read_bcd_device;

		log_err("Product Id (pid) is missing in device connection configuration\n");
		return UFP_DEVICE_MISSING_CONFIG;
	}

	ret = json_read_hex32(config, "pid", &val, 0);
	if (ret) {
		log_err("Product Id (pid) is invalid in device connection configuration\n");
		return UFP_DEVICE_INVALID_CONFIG;
	}

	if (val > 0xffff) {
		log_err("Product Id (0x%x) is invalid in device connection configuration\n", val);
		return UFP_DEVICE_INVALID_CONFIG;
	}

	info->pid = (uint16_t)val;

read_bcd_device:
	/* bcdDevice */
	if (json_node_exists(config, "bcd_device")) {
		ret = json_read_hex32(config, "bcddevice", &val, 0);
		if (ret) {
			log_err("bcdDevice (bcd_device) is invalid in device connection configuration\n");
			return UFP_DEVICE_INVALID_CONFIG;
		}

		if (val > 0xffff) {
			log_err("bcdDevice (0x%x) is invalid in device connection configuration\n", val);
			return UFP_DEVICE_INVALID_CONFIG;
		}

		info->bcd_device = (uint16_t)val;
		info->match_bcd_device = true;
	}

	/* Serial number */
	ret = json_read_str(config, "serial", &str, NULL);
	if (ret) {
		if (ret != UFP_NOT_EXIST) {
			log_err("Serial number (serial) is invalid in device connection configuration\n");
			return UFP_DEVICE_INVALID_CONFIG;
		}
	} else {
		if (strlen(str) > 255) {
			log_err("Serial number is too long in device connection configuration\n");
			return UFP_DEVICE_INVALID_CONFIG;
		}

		info->serial = os_strdup(str);
	}

	/* Product string */
	ret = json_read_str(config, "product", &str, NULL);
	if (ret) {
		if (ret != UFP_NOT_EXIST) {
			log_err("Product string (product) is invalid in device connection configuration\n");
			return UFP_DEVICE_INVALID_CONFIG;
		}
	} else {
		if (strlen(str) > 255) {
			log_err("Product string is too long in device connection configuration\n");
			return UFP_DEVICE_INVALID_CONFIG;
		}

		info->product = os_strdup(str);
	}

	/* Manufacturer string */
	ret = json_read_str(config, "manufacturer", &str, NULL);
	if (ret) {
		if (ret != UFP_NOT_EXIST) {
			log_err("Manufacturer string (manufacturer) is invalid in device connection configuration\n");
			ret = UFP_DEVICE_INVALID_CONFIG;
			goto cleanup;
		}
	} else {
		if (strlen(str) > 255) {
			log_err("Manufacturer string is too long in device connection configuration\n");
			ret = UFP_DEVICE_INVALID_CONFIG;
			goto cleanup;
		}

		info->manufacturer = os_strdup(str);
	}

	/* Port path */
	ret = json_read_str(config, "port_path", &str, NULL);
	if (ret) {
		if (ret != UFP_NOT_EXIST) {
			log_err("Port path (port_path) is invalid in device connection configuration\n");
			ret = UFP_DEVICE_INVALID_CONFIG;
			goto cleanup;
		}
	} else {
		len = strlen(str);
		if (len > 2 * USB_PATH_LEVEL || len % 2) {
			log_err("Port path is invalid in device connection configuration\n");
			ret = UFP_DEVICE_INVALID_CONFIG;
			goto cleanup;
		}

		(void)strtoull(str, &end, 16);
		if (*end) {
			log_err("Port path is invalid in device connection configuration\n");
			ret = UFP_DEVICE_INVALID_CONFIG;
			goto cleanup;
		}

		memcpy(info->path, str, len + 1);
		info->match_path = true;
	}

	/* Index */
	ret = json_read_uint32(config, "index", &val, 0);
	if (ret) {
		log_err("Device index (index) is invalid in device connection configuration\n");
		return UFP_DEVICE_INVALID_CONFIG;
	}

	info->index = val;

	return UFP_OK;

cleanup:
	if (info->serial) {
		free((void *)info->serial);
		info->serial = NULL;
	}

	if (info->product) {
		free((void *)info->product);
		info->product = NULL;
	}

	if (info->manufacturer) {
		free((void *)info->manufacturer);
		info->manufacturer = NULL;
	}

	return ret;
}

ufprog_status UFPROG_API libusb_open_by_config(struct libusb_context *ctx, struct json_object *config,
					       struct libusb_device_handle **outhandle)
{
	struct libusb_match_info info;
	ufprog_status ret;

	if (!outhandle)
		return UFP_INVALID_PARAMETER;

	*outhandle = NULL;

	if (!config) {
		log_err("Device connection configuration is required by libusb\n");
		return UFP_DEVICE_MISSING_CONFIG;
	}

	STATUS_CHECK_RET(libusb_read_config(config, &info, false));

	ret = libusb_open_matched(ctx, &info, outhandle);

	if (info.serial)
		free((void *)info.serial);

	if (info.product)
		free((void *)info.product);

	if (info.manufacturer)
		free((void *)info.manufacturer);

	return ret;
}
