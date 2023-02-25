/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * CH341 library abstraction layer
 */
#pragma once

#ifndef _UFPROG_CH341_H_
#define _UFPROG_CH341_H_

#include <stdint.h>
#include <stdbool.h>
#include <ufprog/osdef.h>
#include <ufprog/config.h>

struct ch34x_handle;

#define CH341_USB_VID				0x1A86
#define CH341_USB_PID				0x5512

#define CH341_USB_BULK_ENDPOINT			2
#define CH341_PACKET_LEN			0x20

#define CH341_SPI_MAX_CS			3
#define CH341_RW_TIMEOUT			5000

#define CH341_CMD_SPI_STREAM			0xA8
#define CH341_CMD_UIO_STREAM			0xAB

#define	CH341_CMD_UIO_STM_IN			0x00	/* UIO Interface In (D0~D7) */
#define	CH341_CMD_UIO_STM_DIR			0x40	/* UIO interface Dir (D0~D5) */
#define	CH341_CMD_UIO_STM_OUT			0x80	/* UIO Interface Output (D0~D5) */
#define	CH341_CMD_UIO_STM_END			0x20	/* UIO Interface End Command */

/* CH341 I/O pins */
#define CH341_IO0_CS0				0x01
#define CH341_IO1_CS1				0x02
#define CH341_IO2_CS2				0x04
#define CH341_IO3_SCK				0x08
#define CH341_IO4_DOUT2				0x10
#define CH341_IO5_MOSI				0x20
#define CH341_IO6_DIN2				0x40
#define CH341_IO7_MISO				0x80

struct ufprog_if_dev {
	struct ch34x_handle *handle;

	uint32_t spi_cs;
	ufprog_bool spi_cs_active_high;

	mutex_handle lock;
};

ufprog_status ch341_init(struct ufprog_if_dev *wchdev, bool thread_safe);

ufprog_status ch341_write(struct ch34x_handle *handle, const void *buf, size_t len, size_t *retlen);
ufprog_status ch341_read(struct ch34x_handle *handle, void *buf, size_t len, size_t *retlen);

ufprog_status ch341_spi_init(struct ufprog_if_dev *wchdev, struct json_object *config);

void ch341_bitswap(const uint8_t *buf, uint8_t *out, size_t len);

#endif /* _UFPROG_CH341_H_ */
