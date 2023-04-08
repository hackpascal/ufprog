/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI master interface driver for CH341
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ufprog/api_spi.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include "ch341.h"

#define CH341_SPI_IF_MAJOR			1
#define CH341_SPI_IF_MINOR			0

#define CH341_SPI_OUT_PINS			(CH341_IO0_CS0 | CH341_IO1_CS1 | CH341_IO2_CS2 | \
						 CH341_IO3_SCK | CH341_IO4_DOUT2 | CH341_IO5_MOSI)

static ufprog_status ch341_spi_set_cs(struct ufprog_interface *wchdev, bool activate)
{
	uint8_t packet[4], val = CH341_SPI_OUT_PINS & ~CH341_IO3_SCK;

	if (wchdev->spi_cs_active_high) {
		if (!activate)
			val &= ~(1 << wchdev->spi_cs);
	} else {
		if (activate)
			val &= ~(1 << wchdev->spi_cs);
	}

	packet[0] = CH341_CMD_UIO_STREAM;
	packet[1] = CH341_CMD_UIO_STM_OUT | val;
	packet[2] = CH341_CMD_UIO_STM_DIR | CH341_SPI_OUT_PINS;
	packet[3] = CH341_CMD_UIO_STM_END;

	return ch341_write(wchdev->handle, packet, sizeof(packet), NULL);
}

static ufprog_status ch341_spi_fdx_xfer(struct ufprog_interface *wchdev, const void *tx, void *rx, size_t len)
{
	uint8_t iobuf[CH341_PACKET_LEN];
	const uint8_t *ptx = tx;
	uint8_t *prx = rx;
	size_t chksz;

	while (len) {
		chksz = len;
		if (chksz > CH341_PACKET_LEN - 1)
			chksz = CH341_PACKET_LEN - 1;

		iobuf[0] = CH341_CMD_SPI_STREAM;
		if (ptx) {
			ch341_bitswap(ptx, &iobuf[1], chksz);
			ptx += chksz;
		} else {
			memset(&iobuf[1], 0, chksz);
		}

		STATUS_CHECK_RET(ch341_write(wchdev->handle, iobuf, chksz + 1, NULL));
		STATUS_CHECK_RET(ch341_read(wchdev->handle, iobuf, chksz, NULL));

		if (prx) {
			ch341_bitswap(iobuf, prx, chksz);
			prx += chksz;
		}

		len -= chksz;
	}

	return UFP_OK;
}

ufprog_status ch341_spi_init(struct ufprog_interface *wchdev, struct json_object *config)
{
	if (!json_read_uint32(config, "chip-select", &wchdev->spi_cs, 0)) {
		if (wchdev->spi_cs >= CH341_SPI_MAX_CS) {
			logm_err("SPI: Invalid chip select in configuration.\n");
			return UFP_DEVICE_INVALID_CONFIG;
		}
	}

	ch341_spi_set_cs(wchdev, false);

	return UFP_OK;
}

uint32_t UFPROG_API ufprog_spi_if_version(void)
{
	return MAKE_VERSION(CH341_SPI_IF_MAJOR, CH341_SPI_IF_MINOR);
}

uint32_t UFPROG_API ufprog_spi_if_caps(void)
{
	return 0;
}

size_t UFPROG_API ufprog_spi_max_read_granularity(void)
{
	return SIZE_MAX;
}

ufprog_status UFPROG_API ufprog_spi_set_cs_pol(void *dev, ufprog_bool positive)
{
	struct ufprog_interface *wchdev = dev;

	if (!dev)
		return UFP_INVALID_PARAMETER;

	wchdev->spi_cs_active_high = positive;

	return UFP_OK;
}

static ufprog_status ch341_spi_generic_xfer_one(struct ufprog_interface *wchdev, const struct ufprog_spi_transfer *xfer)
{
 	if (xfer->buswidth > 1 || xfer->dtr) {
		logm_err("SPI: Only single I/O single rate is supported\n");
		return UFP_UNSUPPORTED;
	}

	if (xfer->dir == SPI_DATA_IN)
		return ch341_spi_fdx_xfer(wchdev, NULL, xfer->buf.rx, xfer->len);

	return ch341_spi_fdx_xfer(wchdev, xfer->buf.tx, NULL, xfer->len);
}

ufprog_status UFPROG_API ufprog_spi_generic_xfer(struct ufprog_interface *wchdev,
						 const struct ufprog_spi_transfer *xfers, uint32_t count)
{
	uint8_t wrbuf[CH341_PACKET_LEN - 1];
	bool require_spi_start = true;
	ufprog_status ret = UFP_OK;
	uint32_t i;

	if (!wchdev)
		return UFP_INVALID_PARAMETER;

	os_mutex_lock(wchdev->lock);

	if (count == 2 && xfers[0].dir == SPI_DATA_OUT && xfers[1].dir == SPI_DATA_IN &&
	    xfers[0].len + xfers[1].len <= sizeof(wrbuf) && !(xfers[0].dtr || xfers[1].dtr) &&
	    xfers[0].buswidth == 1 && xfers[1].buswidth == 1) {
		memcpy(wrbuf, xfers[0].buf.tx, xfers[0].len);
		memset(wrbuf + xfers[0].len, 0, xfers[1].len);

		STATUS_CHECK_GOTO_RET(ch341_spi_set_cs(wchdev, true), ret, out);
		STATUS_CHECK_GOTO_RET(ch341_spi_fdx_xfer(wchdev, wrbuf, wrbuf, xfers[0].len + xfers[1].len), ret, out);
		STATUS_CHECK_GOTO_RET(ch341_spi_set_cs(wchdev, false), ret, out);

		memcpy(xfers[1].buf.rx, wrbuf + xfers[0].len, xfers[1].len);
	} else {
		for (i = 0; i < count; i++) {
			if (require_spi_start) {
				STATUS_CHECK_GOTO_RET(ch341_spi_set_cs(wchdev, true), ret, out);
				require_spi_start = false;
			}

			ret = ch341_spi_generic_xfer_one(wchdev, &xfers[i]);
			if (ret) {
				ch341_spi_set_cs(wchdev, false);
				goto out;
			}

			if (xfers[i].end) {
				STATUS_CHECK_GOTO_RET(ch341_spi_set_cs(wchdev, false), ret, out);
				require_spi_start = true;
			}
		}
	}

out:
	os_mutex_unlock(wchdev->lock);

	return ret;
}
