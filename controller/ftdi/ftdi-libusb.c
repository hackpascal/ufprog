/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * libusb wrapper for FTDI library routines
 */

#include <malloc.h>
#include <stdbool.h>
#include <string.h>
#include <ufprog/log.h>
#include "ftdi-libusb.h"
#include "ftdi.h"

#define FTDI_VENDOR_CMD_OUT_REQTYPE	(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT)
#define FTDI_VENDOR_CMD_IN_REQTYPE	(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN)

#define FTDI_TRANSFER_TIMEOUT				10000

static ufprog_status ftdi_vendor_request(struct libusb_device_handle *dev_handle, uint8_t request, uint16_t value,
					 uint16_t index, void *buf, uint16_t len, uint8_t request_type)
{
	int ret;

	ret = libusb_control_transfer(dev_handle, request_type, request, value, index, buf, len, FTDI_TRANSFER_TIMEOUT);

	if (ret < 0) {
		logm_err("USB control transfer failed: %s\n", libusb_strerror(ret));
		return UFP_DEVICE_IO_ERROR;
	}

	if (ret != len) {
		logm_warn("Incomplete control data transfer through usb: req = 0x%04x, val = 0x%04x, %u of %u xfer'ed\n",
			  request, value, ret, len);
		return UFP_DEVICE_IO_ERROR;
	}

	return UFP_OK;
}

ufprog_status ftdi_vendor_cmd_get(struct ft_handle *handle, uint8_t request, void *buf, uint16_t len)
{
	if (len > 0x80)
		len = 0x80;

	return ftdi_vendor_request(handle->handle, FTDI_REQUEST_VENDOR_CMD_GET, request, handle->interface_number + 1,
				   (void *)buf, len, FTDI_VENDOR_CMD_IN_REQTYPE);
}

ufprog_status ftdi_vendor_cmd_set(struct ft_handle *handle, uint8_t request, const void *buf, uint16_t len)
{
	const uint8_t *p;
	uint16_t val;

	if (len == 1) {
		p = buf;
		val = request | ((uint16_t)*p << 8);
		len = 0;
	} else {
		val = request;
	}

	if (len > 0x80)
		len = 0x80;

	return ftdi_vendor_request(handle->handle, FTDI_REQUEST_VENDOR_CMD_SET, val, handle->interface_number + 1,
				   (void *)buf, len, FTDI_VENDOR_CMD_OUT_REQTYPE);
}

ufprog_status ftdi_reset(struct ft_handle *handle)
{
	return ftdi_vendor_request(handle->handle, FTDI_REQUEST_RESET, FTDI_RESET_TYPE_RESET,
				   handle->interface_number + 1, NULL, 0, FTDI_VENDOR_CMD_OUT_REQTYPE);
}

ufprog_status ftdi_purge_all(struct ft_handle *handle)
{
	ufprog_status ret;
	uint32_t i;

	for (i = 0; i < 6; i++) {
		ret = ftdi_vendor_request(handle->handle, FTDI_REQUEST_RESET, FTDI_RESET_TYPE_PURGE_RX,
					  handle->interface_number + 1, NULL, 0, FTDI_VENDOR_CMD_OUT_REQTYPE);
		if (ret)
			return ret;
	}

	return ftdi_vendor_request(handle->handle, FTDI_REQUEST_RESET, FTDI_RESET_TYPE_PURGE_TX,
				   handle->interface_number + 1, NULL, 0, FTDI_VENDOR_CMD_OUT_REQTYPE);
}

ufprog_status ftdi_set_latency_timer(struct ft_handle *handle, uint8_t latency_ms)
{
	if (latency_ms < 2)
		latency_ms = 2;

	return ftdi_vendor_request(handle->handle, FTDI_REQUEST_SET_LATENCY_TIMER, latency_ms,
				   handle->interface_number + 1, NULL, 0, FTDI_VENDOR_CMD_OUT_REQTYPE);
}

ufprog_status ftdi_get_latency_timer(struct ft_handle *handle, uint8_t *platency_ms)
{
	return ftdi_vendor_request(handle->handle, FTDI_REQUEST_GET_LATENCY_TIMER, 0,
				   handle->interface_number + 1, platency_ms, 1, FTDI_VENDOR_CMD_IN_REQTYPE);
}

ufprog_status ftdi_set_bit_mode(struct ft_handle *handle, uint8_t mask, uint8_t mode)
{
	uint16_t val = ((uint16_t)mode << 8) | mask;

	return ftdi_vendor_request(handle->handle, FTDI_REQUEST_SET_BITMODE, val,
				   handle->interface_number + 1, NULL, 0, FTDI_VENDOR_CMD_OUT_REQTYPE);
}

ufprog_status ftdi_get_bit_mode(struct ft_handle *handle, uint8_t *pmode)
{
	return ftdi_vendor_request(handle->handle, FTDI_REQUEST_GET_BITMODE, 0,
				   handle->interface_number + 1, pmode, 1, FTDI_VENDOR_CMD_IN_REQTYPE);
}

static ufprog_status ftdi_read_raw(struct ft_handle *handle, void *buf, size_t len)
{
	int ret, packet_size, transferred;
	uint8_t *pbuf = buf, *pfifo, *p;
	size_t chksz;

	while (len) {
		ret = libusb_bulk_transfer(handle->handle, handle->in_ep, handle->in_buffer,
					   (int)handle->in_buffer_size, &transferred, handle->timeout);
		if (ret && ret != LIBUSB_ERROR_TIMEOUT) {
			logm_warn("Failed bulk data transfer through usb: %s, %u read\n", libusb_strerror(ret),
				  transferred);
			return UFP_DEVICE_IO_ERROR;
		}

		if (transferred < 2)
			continue;

		p = handle->in_buffer;
		pfifo = handle->in_fifo;
		handle->fifo_used = 0;

		while (transferred) {
			if (transferred >= handle->max_packet_size)
				packet_size = handle->max_packet_size;
			else
				packet_size = transferred;

			if (packet_size > 2) {
				memcpy(pfifo, p + 2, packet_size - 2);
				pfifo += packet_size - 2;
				handle->fifo_used += packet_size - 2;

			}

			transferred -= packet_size;
			p += packet_size;
		}

		if (handle->fifo_used > len)
			chksz = len;
		else
			chksz = handle->fifo_used;

		/* TODO: optimize data copying */
		memcpy(pbuf, handle->in_fifo, chksz);

		if (handle->fifo_used > len) {
			memmove(handle->in_fifo, handle->in_fifo + chksz, handle->fifo_used - chksz);
			handle->fifo_used -= chksz;
		} else {
			handle->fifo_used = 0;
		}

		pbuf += chksz;
		len -= chksz;
	}

	return UFP_OK;
}

ufprog_status ftdi_read(struct ft_handle *handle, void *buf, size_t len)
{
	uint8_t *p = buf;
	size_t chksz;

	if (handle->fifo_used) {
		if (handle->fifo_used >= len)
			chksz = len;
		else
			chksz = handle->fifo_used;

		memcpy(p, handle->in_fifo, chksz);

		if (handle->fifo_used > len) {
			memmove(handle->in_fifo, handle->in_fifo + len, handle->fifo_used - len);
			handle->fifo_used -= len;
		} else {
			handle->fifo_used = 0;
		}

		len -= chksz;
		p += chksz;
		if (!len)
			return UFP_OK;
	}

	return ftdi_read_raw(handle, p, len);
}

ufprog_status ftdi_write(struct ft_handle *handle, const void *buf, size_t len)
{
	int ret, chksz, transferred;
	uint8_t *p = (uint8_t *)buf;

	do {
		if (len > INT_MAX)
			chksz = INT_MAX;
		else
			chksz = (int)len;

		ret = libusb_bulk_transfer(handle->handle, handle->out_ep, p, chksz, &transferred, handle->timeout);
		if (ret) {
			logm_warn("Incomplete bulk data transfer through usb: %s, %u of %u written\n",
				  libusb_strerror(ret), transferred, chksz);
			return UFP_DEVICE_IO_ERROR;
		}

		len -= chksz;
		p += chksz;
	} while (len);

	return UFP_OK;
}

ufprog_status ftdi_setup_handle(struct ft_handle *handle, struct libusb_device_handle *dev_handle,
				uint8_t interface_number, uint8_t config_index, size_t max_read_size)
{
	const struct libusb_interface_descriptor *interface_desc;
	const struct libusb_endpoint_descriptor *ep_desc;
	size_t in_buffer_size, in_fifo_size, max_packets;
	const struct libusb_interface *interface_info;
	struct libusb_config_descriptor *config_desc;
	uint16_t bcd_device, max_packet_size = 0;
	struct libusb_device_descriptor desc;
	bool in_set = false, out_set = false;
	uint8_t i, in_ep = 0, out_ep = 0;
	struct libusb_device *udev;
	int ret;

	udev = libusb_get_device(dev_handle);

	ret = libusb_get_device_descriptor(udev, &desc);
	if (ret < 0) {
		logm_err("Unable to get device descriptor: %s\n", libusb_strerror(ret));
		return UFP_DEVICE_IO_ERROR;
	}

	bcd_device = desc.bcdDevice;

	if (config_index >= desc.bNumConfigurations) {
		logm_err("Device configuration index %u is too large. Only %u available\n", config_index,
			desc.bNumConfigurations);
		return UFP_DEVICE_NOT_FOUND;
	}

	ret = libusb_get_config_descriptor(udev, config_index, &config_desc);
	if (ret < 0) {
		logm_err("Unable to get device configuration %u descriptor: %s\n", config_index, libusb_strerror(ret));
		return UFP_DEVICE_IO_ERROR;
	}

	ret = libusb_set_configuration(dev_handle, config_desc->bConfigurationValue);
	if (ret < 0) {
		logm_err("Unable to set device configuration: %s\n", libusb_strerror(ret));
		libusb_free_config_descriptor(config_desc);
		return UFP_DEVICE_IO_ERROR;
	}

	if (interface_number >= config_desc->bNumInterfaces) {
		logm_err("Device interface %u is invalid. Only %u available\n", interface_number,
			config_desc->bNumInterfaces);
		libusb_free_config_descriptor(config_desc);
		return UFP_DEVICE_NOT_FOUND;
	}

	interface_info = &config_desc->interface[interface_number];

	if (interface_info->num_altsetting > 0) {
		interface_desc = &interface_info->altsetting[0];

		if (interface_desc->bNumEndpoints > 0) {
			for (i = 0; i < interface_desc->bNumEndpoints; i++) {
				ep_desc = &interface_desc->endpoint[i];

				if ((ep_desc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
					if (!max_packet_size)
						max_packet_size = ep_desc->wMaxPacketSize;

					if (!in_set && ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
						in_ep = ep_desc->bEndpointAddress;
						in_set = true;
					}

					if (!out_set && !(ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_IN)) {
						out_ep = ep_desc->bEndpointAddress;
						out_set = true;
					}

					if (in_set && out_set)
						break;
				}
			}
		}
	}

	libusb_free_config_descriptor(config_desc);

	if (in_ep == out_ep) {
		logm_err("Unable to get bulk-type IN/OUT endpoint address\n");
		return UFP_DEVICE_NOT_FOUND;
	}

	if (!max_packet_size) {
		logm_err("Unable to get max packet size of device\n");
		return UFP_DEVICE_NOT_FOUND;
	}

	libusb_set_auto_detach_kernel_driver(dev_handle, 1);

	ret = libusb_claim_interface(dev_handle, interface_number);
	if (ret < 0) {
		logm_err("Unable to claim interface: %s\n", libusb_strerror(ret));
		return UFP_DEVICE_IO_ERROR;
	}

	max_packets = max_read_size / (max_packet_size - 2);
	if (max_read_size % (max_packet_size - 2))
		max_packets++;

	in_buffer_size = max_packet_size * max_packets;
	in_fifo_size = max_packets * (max_packet_size - 2);

	if (in_buffer_size > INT_MAX) {
		logm_err("Max read size is too large for libusb\n");
		libusb_release_interface(dev_handle, interface_number);
		return UFP_DEVICE_NOT_FOUND;
	}

	memset(handle, 0, sizeof(*handle));

	handle->in_buffer = malloc(in_buffer_size + in_fifo_size);
	if (!handle->in_buffer) {
		logm_err("No memory for usb device in buffer\n");
		libusb_release_interface(dev_handle, interface_number);
		return UFP_NOMEM;
	}

	handle->handle = dev_handle;
	handle->in_buffer_size = in_buffer_size;
	handle->bcd_device = bcd_device;
	handle->max_packet_size = max_packet_size;
	handle->interface_number = interface_number;
	handle->timeout = FTDI_TRANSFER_TIMEOUT;
	handle->in_ep = in_ep;
	handle->out_ep = out_ep;

	handle->in_fifo = handle->in_buffer + in_buffer_size;
	handle->fifo_size = in_fifo_size;

	return UFP_OK;
}

ufprog_status ftdi_cleanup_handle(struct ft_handle *handle)
{
	if (handle->in_buffer)
		free(handle->in_buffer);

	return UFP_OK;
}

ufprog_status ftdi_get_mpsse_chip(struct ft_handle *handle, enum ftdi_mpsse_chip *chip)
{
	switch (handle->bcd_device) {
	case 0x500:
		*chip = FT2232C;
		break;
	case 0x700:
		*chip = FT2232H;
		break;
	case 0x800:
		*chip = FT4232H;
		break;
	case 0x900:
		*chip = FT232H;
		break;
	default:
		return UFP_UNSUPPORTED;
	}

	return UFP_OK;
}

int UFPROG_API ftdi_libusb_try_match_open(void *priv, struct json_object *match, int index)
{
	struct ftdi_libusb_open_info *oi = priv;
	ufprog_status ret;

	ret = json_read_uint32(match, "interface", &oi->interface_number, 0);
	if (ret) {
		if (index >= 0)
			logm_warn("Invalid type of device interface number in match#%u\n", index);
		else
			logm_warn("Invalid type of device interface number in matching data\n");
		return 0;
	}

	ret = libusb_open_by_config(oi->ctx, match, &oi->handle);
	if (ret) {
		if (index >= 0)
			logm_dbg("Failed to open device specified by match#%u\n", index);
		else
			logm_dbg("Failed to open device specified by matching data\n");
		return 0;
	}

	return 1;
}
