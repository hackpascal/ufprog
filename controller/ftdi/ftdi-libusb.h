/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * libusb wrapper for FTDI library routines
 */
#pragma once

#ifndef _UFPROG_FTDI_LIBUSB_H_
#define _UFPROG_FTDI_LIBUSB_H_

#include <ufprog/libusb.h>

struct ft_handle {
	struct libusb_device_handle *handle;
	uint8_t *in_buffer;
	size_t in_buffer_size;
	uint32_t timeout;
	uint16_t bcd_device;
	uint16_t max_packet_size;
	uint8_t interface_number;
	uint8_t in_ep;
	uint8_t out_ep;

	/* TODO: use ring buffer */
	uint8_t *in_fifo;
	size_t fifo_size;
	size_t fifo_used;
};

ufprog_status ftdi_setup_handle(struct ft_handle *handle, struct libusb_device_handle *dev_handle,
				uint8_t interface_number, uint8_t config_index, size_t max_read_size);
ufprog_status ftdi_cleanup_handle(struct ft_handle *handle);

struct ftdi_libusb_open_info {
	struct libusb_context *ctx;
	struct libusb_device_handle *handle;
	uint32_t interface_number;
};

int ftdi_libusb_try_match_open(void *priv, struct json_object *match, int index);

#endif /* _UFPROG_FTDI_LIBUSB_H_ */
