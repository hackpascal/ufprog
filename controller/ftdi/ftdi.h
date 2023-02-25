/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * FTDI library abstraction layer
 */
#pragma once

#ifndef _UFPROG_FTDI_H_
#define _UFPROG_FTDI_H_

#include <stdint.h>

#define FTDI_REQUEST_RESET				0x00
#define FTDI_REQUEST_SET_LATENCY_TIMER			0x09
#define FTDI_REQUEST_GET_LATENCY_TIMER			0x0A
#define FTDI_REQUEST_SET_BITMODE			0x0B
#define FTDI_REQUEST_GET_BITMODE			0x0C
#define FTDI_REQUEST_VENDOR_CMD_GET			0x20
#define FTDI_REQUEST_VENDOR_CMD_SET			0x21

#define FTDI_RESET_TYPE_RESET				0x00
#define FTDI_RESET_TYPE_PURGE_RX			0x01
#define FTDI_RESET_TYPE_PURGE_TX			0x02

#define FTDI_BITMODE_RESET				0x00
#define FTDI_BITMODE_BITBANG				0x01
#define FTDI_BITMODE_MPSSE				0x02
#define FTDI_BITMODE_SYNCBB				0x04
#define FTDI_BITMODE_MCU				0x08
#define FTDI_BITMODE_OPTO				0x10

struct ft_handle;

enum ftdi_mpsse_chip {
	FT232H,
	FT2232C,
	FT2232H,
	FT4232H
};

ufprog_status ftdi_reset(struct ft_handle *handle);
ufprog_status ftdi_purge_all(struct ft_handle *handle);
ufprog_status ftdi_read(struct ft_handle *handle, void *buf, size_t len);
ufprog_status ftdi_write(struct ft_handle *handle, const void *buf, size_t len);
ufprog_status ftdi_vendor_cmd_get(struct ft_handle *handle, uint8_t request, void *buf, uint16_t len);
ufprog_status ftdi_vendor_cmd_set(struct ft_handle *handle, uint8_t request, const void *buf, uint16_t len);
ufprog_status ftdi_set_latency_timer(struct ft_handle *handle, uint8_t latency_ms);
ufprog_status ftdi_get_latency_timer(struct ft_handle *handle, uint8_t *platency_ms);
ufprog_status ftdi_set_bit_mode(struct ft_handle *handle, uint8_t mask, uint8_t mode);
ufprog_status ftdi_get_bit_mode(struct ft_handle *handle, uint8_t *pmode);

ufprog_status ftdi_get_mpsse_chip(struct ft_handle *handle, enum ftdi_mpsse_chip *chip);

#endif /* _UFPROG_FTDI_H_ */
