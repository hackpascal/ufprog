/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI master interface driver for CH347
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ufprog/api_spi.h>
#include <ufprog/config.h>
#include <ufprog/endian.h>
#include <ufprog/log.h>
#include "ch347.h"

#define CH347_SPI_IF_MAJOR			1
#define CH347_SPI_IF_MINOR			0

static ufprog_status ch347_spi_write_packet(struct ufprog_interface *wchdev, uint8_t cmd, const void *buf, uint32_t len)
{
	if (len > CH347_MAX_XFER_LEN)
		return UFP_INVALID_PARAMETER;

	wchdev->iobuf[0] = cmd;
	wchdev->iobuf[1] = len & 0xff;
	wchdev->iobuf[2] = (len >> 8) & 0xff;

	memcpy(wchdev->iobuf + CH347_SPI_CMD_LEN, buf, len);

	return ch347_write(wchdev->handle, wchdev->iobuf, CH347_SPI_CMD_LEN + len, NULL);
}

static ufprog_status ch347_spi_read_packet(struct ufprog_interface *wchdev, uint8_t cmd, void *buf, uint32_t len,
					   uint32_t *retlen)
{
	uint32_t packet_len;
	size_t retlen_raw;

	STATUS_CHECK_RET(ch347_read(wchdev->handle, wchdev->iobuf, CH347_SPI_CMD_LEN + len, &retlen_raw));

	if (retlen_raw < CH347_SPI_CMD_LEN) {
		log_err("CH347-DLL SPI: read-back packet is too small.\n");
		return UFP_DEVICE_IO_ERROR;
	}

	if (wchdev->iobuf[0] != cmd) {
		log_err("CH347-DLL SPI: read-back packet cmd mismatch. Expect %02x but got %02x.\n",
			cmd, wchdev->iobuf[0]);
		return UFP_DEVICE_IO_ERROR;
	}

	packet_len = wchdev->iobuf[1] | wchdev->iobuf[2] << 8;
	if (packet_len > retlen_raw - CH347_SPI_CMD_LEN) {
		log_err("CH347-DLL SPI: read-back packet is too small. Payload is incomplete: %luB of %uB returned.\n",
			retlen_raw - CH347_SPI_CMD_LEN, packet_len);
		return UFP_DEVICE_IO_ERROR;
	}

	if (packet_len > len) {
		log_warn("CH347-DLL SPI: read-back packet is too big. Expect %uB but %uB returned.\n", len, packet_len);
		packet_len = len;
	}

	memcpy(buf, wchdev->iobuf + 3, packet_len);

	if (retlen)
		*retlen = packet_len;

	return UFP_OK;
}

static ufprog_status ch347_spi_get_config(struct ufprog_interface *wchdev)
{
	struct ch347_spi_hw_config cfg;
	uint8_t unknown_data = 0x01;
	uint32_t retlen;

	STATUS_CHECK_RET(ch347_spi_write_packet(wchdev, CH347_CMD_INFO_RD, &unknown_data, 1));
	STATUS_CHECK_RET(ch347_spi_read_packet(wchdev, CH347_CMD_INFO_RD, &cfg, sizeof(cfg), &retlen));

	if (retlen != sizeof(wchdev->spicfg)) {
		log_warn("CH347-DLL SPI: incomplete read of spi hw config\n");
		return UFP_DEVICE_IO_ERROR;
	}

	wchdev->spicfg.SPI_Direction = le16toh(cfg.SPI_Direction);
	wchdev->spicfg.SPI_Mode = le16toh(cfg.SPI_Mode);
	wchdev->spicfg.SPI_DataSize = le16toh(cfg.SPI_DataSize);
	wchdev->spicfg.SPI_CPOL = le16toh(cfg.SPI_CPOL);
	wchdev->spicfg.SPI_CPHA = le16toh(cfg.SPI_CPHA);
	wchdev->spicfg.SPI_NSS = le16toh(cfg.SPI_NSS);
	wchdev->spicfg.SPI_BaudRatePrescaler = le16toh(cfg.SPI_BaudRatePrescaler);
	wchdev->spicfg.SPI_FirstBit = le16toh(cfg.SPI_FirstBit);
	wchdev->spicfg.SPI_CRCPolynomial = le16toh(cfg.SPI_CRCPolynomial);
	wchdev->spicfg.SPI_WriteReadInterval = le16toh(cfg.SPI_WriteReadInterval);
	wchdev->spicfg.SPI_OutDefaultData = cfg.SPI_OutDefaultData;
	wchdev->spicfg.OtherCfg = cfg.OtherCfg;
	memcpy(wchdev->spicfg.Reserved, cfg.Reserved, sizeof(cfg.Reserved));

	return UFP_OK;
}

static ufprog_status ch347_spi_set_config(struct ufprog_interface *wchdev)
{
	struct ch347_spi_hw_config cfg;
	uint8_t unknown_data;

	cfg.SPI_Direction = htole16(wchdev->spicfg.SPI_Direction);
	cfg.SPI_Mode = htole16(wchdev->spicfg.SPI_Mode);
	cfg.SPI_DataSize = htole16(wchdev->spicfg.SPI_DataSize);
	cfg.SPI_CPOL = htole16(wchdev->spicfg.SPI_CPOL);
	cfg.SPI_CPHA = htole16(wchdev->spicfg.SPI_CPHA);
	cfg.SPI_NSS = htole16(wchdev->spicfg.SPI_NSS);
	cfg.SPI_BaudRatePrescaler = htole16(wchdev->spicfg.SPI_BaudRatePrescaler);
	cfg.SPI_FirstBit = htole16(wchdev->spicfg.SPI_FirstBit);
	cfg.SPI_CRCPolynomial = htole16(wchdev->spicfg.SPI_CRCPolynomial);
	cfg.SPI_WriteReadInterval = htole16(wchdev->spicfg.SPI_WriteReadInterval);
	cfg.SPI_OutDefaultData = wchdev->spicfg.SPI_OutDefaultData;
	cfg.OtherCfg = wchdev->spicfg.OtherCfg;
	memcpy(cfg.Reserved, wchdev->spicfg.Reserved, sizeof(wchdev->spicfg.Reserved));

	STATUS_CHECK_RET(ch347_spi_write_packet(wchdev, CH347_CMD_SPI_INIT, &cfg, sizeof(cfg)));

	return ch347_spi_read_packet(wchdev, CH347_CMD_SPI_INIT, &unknown_data, 1, NULL);
}

static ufprog_status ch347_spi_set_cs(struct ufprog_interface *wchdev, uint32_t cs, int val, uint16_t autodeactive_us)
{
	uint8_t buf[10] = { 0 };
	uint8_t *entry = cs ? buf + 5 : buf;

	entry[0] = val ? 0xc0 : 0x80;
	if (autodeactive_us) {
		entry[0] |= 0x20;
		entry[3] = autodeactive_us & 0xff;
		entry[4] = autodeactive_us >> 8;
	}

	return ch347_spi_write_packet(wchdev, CH347_CMD_SPI_CONTROL, buf, sizeof(buf));
}

static ufprog_status ch347_spi_set_clk(struct ufprog_interface *wchdev, uint32_t freq, uint32_t *out_freq)
{
	uint32_t prescaler;
	uint32_t tmp_freq;

	for (prescaler = 0; prescaler <= CH347_SPI_MAX_PRESCALER; prescaler++) {
		tmp_freq = CH347_SPI_MAX_FREQ >> prescaler;

		if (freq >= tmp_freq)
			break;
	}

	if (prescaler > CH347_SPI_MAX_PRESCALER) {
		log_err("Requested SPI clock %uHz is too small\n", freq);
		return UFP_UNSUPPORTED;
	}

	wchdev->spicfg.SPI_BaudRatePrescaler = (uint16_t)prescaler * 8;

	if (out_freq)
		*out_freq = tmp_freq;

	return ch347_spi_set_config(wchdev);
}

ufprog_status ch347_spi_init(struct ufprog_interface *wchdev, struct json_object *config)
{
	if (!json_read_uint32(config, "chip-select", &wchdev->spi_cs, 0)) {
		if (wchdev->spi_cs >= CH347_SPI_MAX_CS) {
			log_err("CH347-DLL SPI: Invalid chip select in configuration.\n");
			return UFP_DEVICE_INVALID_CONFIG;
		}
	}

	STATUS_CHECK_RET(ch347_spi_get_config(wchdev));

	/* Default settings */
	wchdev->spicfg.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	wchdev->spicfg.SPI_Mode = SPI_Mode_Master;
	wchdev->spicfg.SPI_DataSize = SPI_DataSize_8b;
	wchdev->spicfg.SPI_CPOL = SPI_CPOL_Low;
	wchdev->spicfg.SPI_CPHA = SPI_CPHA_1Edge;
	wchdev->spicfg.SPI_NSS = SPI_NSS_Software;
	wchdev->spicfg.SPI_FirstBit = SPI_FirstBit_MSB;
	wchdev->spicfg.SPI_WriteReadInterval = 0;
	wchdev->spicfg.SPI_OutDefaultData = 0;
	wchdev->spicfg.SPI_BaudRatePrescaler = CH347_SPI_DFL_PRESCALER * 8;
	wchdev->spicfg.OtherCfg &= 0x3f;

	STATUS_CHECK_RET(ch347_spi_set_config(wchdev));

	return ch347_spi_set_cs(wchdev, wchdev->spi_cs, 1, 0);
}

uint32_t UFPROG_API ufprog_spi_if_version(void)
{
	return MAKE_VERSION(CH347_SPI_IF_MAJOR, CH347_SPI_IF_MINOR);
}

uint32_t UFPROG_API ufprog_spi_if_caps(void)
{
	return 0;
}

size_t UFPROG_API ufprog_spi_max_read_granularity(void)
{
	return SIZE_MAX;
}

ufprog_status UFPROG_API ufprog_spi_set_cs_pol(struct ufprog_interface *wchdev, ufprog_bool positive)
{
	ufprog_status ret;

	if (!wchdev)
		return UFP_INVALID_PARAMETER;

	os_mutex_lock(wchdev->lock);

	if (positive) {
		if (wchdev->spi_cs)
			wchdev->spicfg.OtherCfg |= 0x40;
		else
			wchdev->spicfg.OtherCfg |= 0x80;
	} else {
		if (wchdev->spi_cs)
			wchdev->spicfg.OtherCfg &= ~0x40;
		else
			wchdev->spicfg.OtherCfg &= ~0x80;
	}

	ret = ch347_spi_set_config(wchdev);

	os_mutex_unlock(wchdev->lock);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_set_mode(struct ufprog_interface *wchdev, uint32_t mode)
{
	ufprog_status ret;

	if (!wchdev || mode > 3)
		return UFP_INVALID_PARAMETER;

	os_mutex_lock(wchdev->lock);

	wchdev->spicfg.SPI_CPOL = (mode & SPI_MODE_CPOL) ? SPI_CPOL_High : SPI_CPOL_Low;
	wchdev->spicfg.SPI_CPHA = (mode & SPI_MODE_CPHA) ? SPI_CPHA_2Edge : SPI_CPHA_1Edge;

	ret = ch347_spi_set_config(wchdev);

	os_mutex_unlock(wchdev->lock);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_set_speed(struct ufprog_interface *wchdev, uint32_t hz, uint32_t *rethz)
{
	ufprog_status ret;

	if (!wchdev)
		return UFP_INVALID_PARAMETER;

	os_mutex_lock(wchdev->lock);
	ret = ch347_spi_set_clk(wchdev, hz, rethz);
	os_mutex_unlock(wchdev->lock);

	return ret;
}

uint32_t UFPROG_API ufprog_spi_get_speed(struct ufprog_interface *wchdev)
{
	if (!wchdev)
		return 0;

	return CH347_SPI_MAX_FREQ >> (wchdev->spicfg.SPI_BaudRatePrescaler / 8);
}

uint32_t UFPROG_API ufprog_spi_get_speed_list(struct ufprog_interface *wchdev, uint32_t *retlist, uint32_t count)
{
	uint32_t i;

	if (!retlist || !count)
		return CH347_SPI_MAX_PRESCALER + 1;

	if (count > CH347_SPI_MAX_PRESCALER + 1)
		count = CH347_SPI_MAX_PRESCALER + 1;

	for (i = 0; i < count; i++)
		retlist[i] = CH347_SPI_MAX_FREQ >> i;

	return CH347_SPI_MAX_PRESCALER + 1;
}

static ufprog_status ch347_spi_single_read(struct ufprog_interface *wchdev, void *buf, uint32_t len)
{
	uint32_t chksz, outlen = htole32(len);
	uint8_t *pbuf = buf;

	STATUS_CHECK_RET(ch347_spi_write_packet(wchdev, CH347_CMD_SPI_BLCK_RD, &outlen, sizeof(outlen)));

	while (len) {
		if (len > wchdev->max_payload_len)
			chksz = wchdev->max_payload_len;
		else
			chksz = len;

		STATUS_CHECK_RET(ch347_spi_read_packet(wchdev, CH347_CMD_SPI_BLCK_RD, pbuf, chksz, &chksz));

		pbuf += chksz;
		len -= chksz;
	}

	return UFP_OK;
}

static ufprog_status ch347_spi_read(struct ufprog_interface *wchdev, void *buf, size_t len)
{
	uint8_t *pbuf = buf;
	uint32_t chksz;

	while (len) {
		if (len > UINT32_MAX)
			chksz = UINT32_MAX;
		else
			chksz = (uint32_t)len;

		STATUS_CHECK_RET(ch347_spi_single_read(wchdev, pbuf, chksz));

		pbuf += chksz;
		len -= chksz;
	}

	return UFP_OK;
}

static ufprog_status ch347_spi_single_write(struct ufprog_interface *wchdev, const void *buf, uint32_t len)
{
	const uint8_t *pbuf = buf;
	uint8_t unknown_data;
	uint32_t chksz;

	while (len) {
		if (len > wchdev->max_payload_len)
			chksz = wchdev->max_payload_len;
		else
			chksz = len;

		STATUS_CHECK_RET(ch347_spi_write_packet(wchdev, CH347_CMD_SPI_BLCK_WR, pbuf, chksz));
		STATUS_CHECK_RET(ch347_spi_read_packet(wchdev, CH347_CMD_SPI_BLCK_WR, &unknown_data, 1, NULL));

		pbuf += chksz;
		len -= chksz;
	}

	return UFP_OK;
}

static ufprog_status ch347_spi_write(struct ufprog_interface *wchdev, const void *buf, size_t len)
{
	const uint8_t *pbuf = buf;
	uint32_t chksz;

	while (len) {
		if (len > UINT32_MAX)
			chksz = UINT32_MAX;
		else
			chksz = (uint32_t)len;

		STATUS_CHECK_RET(ch347_spi_single_write(wchdev, pbuf, chksz));

		pbuf += chksz;
		len -= chksz;
	}

	return UFP_OK;
}

static ufprog_status ch347_spi_generic_xfer_one(struct ufprog_interface *wchdev, const struct ufprog_spi_transfer *xfer)
{
	if (xfer->buswidth > 1 || xfer->dtr) {
		log_err("Only single I/O single rate is supported by CH347\n");
		return UFP_UNSUPPORTED;
	}

	if (xfer->speed)
		STATUS_CHECK_RET(ch347_spi_set_clk(wchdev, xfer->speed, NULL));

	if (xfer->dir == SPI_DATA_IN)
		return ch347_spi_read(wchdev, xfer->buf.rx, xfer->len);

	return ch347_spi_write(wchdev, xfer->buf.tx, xfer->len);
}

static ufprog_status ch347_spi_fdx_xfer(struct ufprog_interface *wchdev, void *buf, size_t len)
{
	uint32_t chksz, rdlen, retlen;
	uint8_t *pbuf = buf, *prbuf;

	while (len) {
		if (len > wchdev->max_payload_len)
			chksz = wchdev->max_payload_len;
		else
			chksz = (uint32_t)len;

		STATUS_CHECK_RET(ch347_spi_write_packet(wchdev, CH347_CMD_SPI_RD_WR, pbuf, chksz));

		rdlen = chksz;
		prbuf = pbuf;

		while (rdlen) {
			STATUS_CHECK_RET(ch347_spi_read_packet(wchdev, CH347_CMD_SPI_RD_WR, prbuf, rdlen, &retlen));

			prbuf += retlen;
			rdlen -= retlen;
		}

		pbuf += chksz;
		len -= chksz;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_generic_xfer(struct ufprog_interface *wchdev,
						 const struct ufprog_spi_transfer *xfers, uint32_t count)
{
	bool require_spi_start = true;
	ufprog_status ret = UFP_OK;
	uint32_t i, speed;
	uint8_t wrbuf[16];

	if (!wchdev)
		return UFP_INVALID_PARAMETER;

	os_mutex_lock(wchdev->lock);

	if (count == 2 && xfers[0].dir == SPI_DATA_OUT && xfers[1].dir == SPI_DATA_IN &&
	    xfers[0].len + xfers[1].len <= sizeof(wrbuf) && !(xfers[0].dtr || xfers[1].dtr) &&
	    xfers[0].buswidth == 1 && xfers[1].buswidth == 1) {
		if (xfers[0].speed || xfers[1].speed)
			speed = xfers[0].speed > xfers[1].speed ? xfers[0].speed : xfers[1].speed;
		else
			speed = 0;

		memcpy(wrbuf, xfers[0].buf.tx, xfers[0].len);
		memset(wrbuf + xfers[0].len, wchdev->spicfg.SPI_OutDefaultData, xfers[1].len);

		if (speed)
			ch347_spi_set_clk(wchdev, speed, NULL);

		STATUS_CHECK_GOTO_RET(ch347_spi_set_cs(wchdev, wchdev->spi_cs, 0, 1), ret, out);
		STATUS_CHECK_GOTO_RET(ch347_spi_fdx_xfer(wchdev, wrbuf, xfers[0].len + xfers[1].len), ret, out);
		STATUS_CHECK_GOTO_RET(ch347_spi_set_cs(wchdev, wchdev->spi_cs, 1, 0), ret, out);

		memcpy(xfers[1].buf.rx, wrbuf + xfers[0].len, xfers[1].len);
	} else {
		for (i = 0; i < count; i++) {
			if (require_spi_start) {
				STATUS_CHECK_GOTO_RET(ch347_spi_set_cs(wchdev, wchdev->spi_cs, 0, 0), ret, out);
				require_spi_start = false;
			}

			ret = ch347_spi_generic_xfer_one(wchdev, &xfers[i]);
			if (ret) {
				ch347_spi_set_cs(wchdev, wchdev->spi_cs, 1, 0);
				goto out;
			}

			if (xfers[i].end) {
				STATUS_CHECK_GOTO_RET(ch347_spi_set_cs(wchdev, wchdev->spi_cs, 1, 0), ret, out);
				require_spi_start = true;
			}
		}
	}

out:
	os_mutex_unlock(wchdev->lock);

	return ret;
}
