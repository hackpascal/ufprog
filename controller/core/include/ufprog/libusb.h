/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Common helpers using libusb
 */
#pragma once

#ifndef _UFPROG_LIBUSB_H_
#define _UFPROG_LIBUSB_H_

#include <stdint.h>
#include <ufprog/config.h>
#include <libusb-1.0/libusb.h>

#define USB_PATH_LEVEL				7

struct libusb_match_info {
	uint16_t vid;
	uint16_t pid;
	uint16_t bcd_device;
	ufprog_bool match_bcd_device;
	const char *serial;
	const char *product;
	const char *manufacturer;
	char path[(USB_PATH_LEVEL + 1) * 2];
	ufprog_bool match_path;
	uint32_t index;
};

struct libusb_context *UFPROG_API ufprog_global_libusb_context(void);

ufprog_status UFPROG_API libusb_port_path_to_str(const uint8_t *port_path, uint32_t depth, char *pathstr);
ufprog_status UFPROG_API libusb_open_matched(struct libusb_context *ctx, const struct libusb_match_info *info,
					     struct libusb_device_handle **outhandle);
ufprog_status UFPROG_API libusb_read_config(struct json_object *config, struct libusb_match_info *info,
					    ufprog_bool vidpid_optional);
ufprog_status UFPROG_API libusb_open_by_config(struct libusb_context *ctx, struct json_object *config,
					       struct libusb_device_handle **outhandle);

#endif /* _UFPROG_LIBUSB_H_ */
