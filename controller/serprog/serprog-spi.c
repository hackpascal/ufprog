/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI master interface driver for serprog
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ufprog/endian.h>
#include <ufprog/api_spi.h>
#include <ufprog/config.h>
#include <ufprog/osdef.h>
#include <ufprog/log.h>
#include "serprog.h"

#define SERPROG_SPI_IF_MAJOR			1
#define SERPROG_SPI_IF_MINOR			0

static ufprog_status serprog_read(struct ufprog_interface *dev, void *data, size_t len)
{
	ufprog_status ret;
	size_t retlen;

	ret = serial_port_read(dev->port, data, len, &retlen);
	if (ret) {
		logm_err("Failed to read data from serial port\n");
		return ret;
	}

	if (len != retlen) {
		logm_err("Serial port read timed out\n");
		return UFP_TIMEOUT;
	}

	return UFP_OK;
}

static ufprog_status serprog_write(struct ufprog_interface *dev, const void *data, size_t len)
{
	ufprog_status ret;
	size_t retlen;

	ret = serial_port_write(dev->port, data, len, &retlen);
	if (ret) {
		logm_err("Failed to write data to serial port\n");
		return ret;
	}

	if (len != retlen) {
		logm_err("Serial port write timed out\n");
		return UFP_TIMEOUT;
	}

	return UFP_OK;
}

ufprog_status serprog_sync(struct ufprog_interface *dev)
{
	uint8_t resp, cmd = S_CMD_SYNCNOP;

	STATUS_CHECK_RET(serprog_write(dev, &cmd, 1));

	STATUS_CHECK_RET(serprog_read(dev, &resp, 1));

	if (resp != S_NAK) {
		logm_err("Serprog returned wrong response in synchronization response 1\n");
		return UFP_DEVICE_IO_ERROR;
	}

	STATUS_CHECK_RET(serprog_read(dev, &resp, 1));

	if (resp != S_ACK) {
		logm_err("Serprog returned wrong response in synchronization response 2\n");
		return UFP_DEVICE_IO_ERROR;
	}

	return UFP_OK;
}

static ufprog_status serprog_exec(struct ufprog_interface *dev, uint8_t cmd, const void *outdata, size_t outlen,
				  void *indata, size_t inlen, bool check_ack)
{
	uint8_t resp;

	STATUS_CHECK_RET(serprog_write(dev, &cmd, 1));

	if (outlen)
		STATUS_CHECK_RET(serprog_write(dev, outdata, outlen));

	if (check_ack) {
		STATUS_CHECK_RET(serprog_read(dev, &resp, 1));

		if (resp != S_ACK) {
			logm_err("Serprog returned wrong response\n");
			return UFP_DEVICE_IO_ERROR;
		}

		if (inlen)
			STATUS_CHECK_RET(serprog_read(dev, indata, inlen));
	}

	return UFP_OK;
}

static ufprog_status serprog_query(struct ufprog_interface *dev, uint8_t cmd, void *data, size_t len)
{
	return serprog_exec(dev, cmd, NULL, 0, data, len, true);
}

ufprog_status serprog_spi_init(struct ufprog_interface *dev)
{
	uint32_t cmdbitmap, spi_freq;
	char name[17] = { 0 };
	uint8_t data[32];
	uint16_t ver;

	STATUS_CHECK_RET(serprog_sync(dev));

	STATUS_CHECK_RET(serprog_query(dev, S_CMD_Q_IFACE, &ver, 2));
	ver = le16toh(ver);

	STATUS_CHECK_RET(serprog_query(dev, S_CMD_Q_CMDMAP, data, 32));
	cmdbitmap = data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);

	if (cmdbitmap & BIT(S_CMD_Q_PGMNAME)) {
		STATUS_CHECK_RET(serprog_query(dev, S_CMD_Q_PGMNAME, name, 16));
		logm_info("Programmer: %s ver %u\n", name, ver);
	} else {
		logm_info("Programmer: ver %u\n", ver);
	}

	if (cmdbitmap & BIT(S_CMD_Q_BUSTYPE)) {
		STATUS_CHECK_RET(serprog_query(dev, S_CMD_Q_BUSTYPE, data, 1));
		if (!(data[0] & BUS_SPI)) {
			logm_err("This programmer does not support SPI protocol\n");
			return UFP_UNSUPPORTED;
		}

		if (cmdbitmap & BIT(S_CMD_S_BUSTYPE) && hweight8(data[0]) > 1) {
			data[0] = BUS_SPI;
			STATUS_CHECK_RET(serprog_exec(dev, S_CMD_S_BUSTYPE, data, 1, NULL, 0, true));
		}
	}

	if (cmdbitmap & BIT(S_CMD_S_SPI_FREQ)) {
		spi_freq = htole32(UINT32_MAX);
		STATUS_CHECK_RET(serprog_exec(dev, S_CMD_S_SPI_FREQ, &spi_freq, 4, &dev->max_spi_freq, 4, true));
		dev->max_spi_freq = le32toh(dev->max_spi_freq);

		spi_freq = htole32(1);
		STATUS_CHECK_RET(serprog_exec(dev, S_CMD_S_SPI_FREQ, &spi_freq, 4, &dev->min_spi_freq, 4, true));
		dev->min_spi_freq = le32toh(dev->min_spi_freq);

		dev->curr_spi_freq = dev->min_spi_freq;
	}

	if (cmdbitmap & BIT(S_CMD_S_PIN_STATE)) {
		data[0] = 1;
		STATUS_CHECK_RET(serprog_exec(dev, S_CMD_S_PIN_STATE, data, 1, NULL, 0, true));
	}

	return UFP_OK;
}

uint32_t UFPROG_API ufprog_spi_if_version(void)
{
	return MAKE_VERSION(SERPROG_SPI_IF_MAJOR, SERPROG_SPI_IF_MINOR);
}

uint32_t UFPROG_API ufprog_spi_if_caps(void)
{
	return 0;
}

size_t UFPROG_API ufprog_spi_max_read_granularity(void)
{
	return SERPROG_MAX_BUFFER_SIZE;
}

ufprog_status UFPROG_API ufprog_spi_set_speed(struct ufprog_interface *dev, uint32_t hz, uint32_t *rethz)
{
	uint32_t spi_freq;

	if (!dev)
		return UFP_INVALID_PARAMETER;

	spi_freq = htole32(hz);
	STATUS_CHECK_RET(serprog_exec(dev, S_CMD_S_SPI_FREQ, &spi_freq, 4, &dev->curr_spi_freq, 4, true));

	if (rethz)
		*rethz = dev->curr_spi_freq;

	return UFP_OK;
}

uint32_t UFPROG_API ufprog_spi_get_speed(struct ufprog_interface *dev)
{
	if (!dev)
		return 0;

	return dev->curr_spi_freq;
}

ufprog_status UFPROG_API ufprog_spi_set_mode(struct ufprog_interface *dev, uint32_t mode)
{
	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (mode == SPI_MODE_0 || mode == SPI_MODE_3)
		return UFP_OK;

	return UFP_UNSUPPORTED;
}

ufprog_status UFPROG_API ufprog_spi_set_cs_pol(struct ufprog_interface *dev, ufprog_bool positive)
{
	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (!positive)
		return UFP_OK;

	return UFP_UNSUPPORTED;
}

ufprog_status UFPROG_API ufprog_spi_mem_adjust_op_size(struct ufprog_interface *dev, struct ufprog_spi_mem_op *op)
{
	size_t n = 0;

	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (op->data.dir == SPI_DATA_OUT)
		n = op->cmd.len + op->addr.len + op->dummy.len;

	if (op->data.len > dev->buffer_size - n)
		op->data.len = dev->buffer_size - n;

	return UFP_OK;
}

ufprog_bool UFPROG_API ufprog_spi_mem_supports_op(struct ufprog_interface *dev, const struct ufprog_spi_mem_op *op)
{
	size_t n = 0;

	if (!dev)
		return false;

	if (op->cmd.len && (op->cmd.buswidth != 1 || op->cmd.dtr))
		return false;

	n += op->cmd.len;

	if (op->addr.len && (op->addr.buswidth != 1 || op->addr.dtr))
		return false;

	n += op->addr.len;

	if (op->dummy.len && (op->dummy.buswidth != 1 || op->dummy.dtr))
		return false;

	n += op->dummy.len;

	if (op->data.len && (op->data.buswidth != 1 || op->data.dtr))
		return false;

	if (op->data.dir == SPI_DATA_OUT) {
		if (n >= dev->buffer_size && op->data.len)
			return false;
	}

	return true;
}

ufprog_status UFPROG_API ufprog_spi_mem_exec_op(struct ufprog_interface *dev, const struct ufprog_spi_mem_op *op)
{
	size_t nout = 0, nin = 0;
	uint8_t buf[256], resp;
	uint32_t i;

	if (!dev)
		return UFP_INVALID_PARAMETER;

	nout = op->cmd.len + op->addr.len + op->dummy.len;

	if (op->data.dir == SPI_DATA_OUT)
		nout += op->data.len;
	else
		nin = op->data.len;

	buf[0] = S_CMD_O_SPIOP;
	buf[1] = nout & 0xff;
	buf[2] = (nout >> 8) & 0xff;
	buf[3] = (nout >> 16) & 0xff;
	buf[4] = nin & 0xff;
	buf[5] = (nin >> 8) & 0xff;
	buf[6] = (nin >> 16) & 0xff;

	STATUS_CHECK_RET(serprog_write(dev, buf, 7));

	if (op->cmd.len) {
		for (i = 0; i < op->cmd.len; i++)
			buf[i] = (op->cmd.opcode >> i * 8) & 0xff;

		STATUS_CHECK_RET(serprog_write(dev, buf, op->cmd.len));
	}

	if (op->addr.len) {
		for (i = 0; i < op->addr.len; i++)
			buf[i] = (op->addr.val >> (op->addr.len - i - 1) * 8) & 0xff;

		STATUS_CHECK_RET(serprog_write(dev, buf, op->addr.len));
	}

	if (op->dummy.len) {
		memset(buf, 0xff, op->dummy.len);

		STATUS_CHECK_RET(serprog_write(dev, buf, op->dummy.len));
	}

	if (op->data.dir == SPI_DATA_OUT && op->data.len)
		STATUS_CHECK_RET(serprog_write(dev, op->data.buf.tx, op->data.len));

	STATUS_CHECK_RET(serprog_read(dev, &resp, 1));

	if (resp != S_ACK) {
		logm_err("Serprog returned wrong response\n");
		return UFP_DEVICE_IO_ERROR;
	}

	if (op->data.dir == SPI_DATA_IN && op->data.len)
		STATUS_CHECK_RET(serprog_read(dev, op->data.buf.rx, op->data.len));

	return UFP_OK;
}
