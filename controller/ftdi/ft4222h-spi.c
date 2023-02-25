/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI master interface driver for FT4222H
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ufprog/api_spi.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include "ft4222h.h"

#define FT4222H_SPI_IF_MAJOR			1
#define FT4222H_SPI_IF_MINOR			0

static const struct ft4222_spi_clk_info ft4222_spi_clks[] = {
	{ 40000000, SYS_CLK_80, CLK_DIV_2 },
	{ 30000000, SYS_CLK_60, CLK_DIV_2 },
	{ 24000000, SYS_CLK_48, CLK_DIV_2 },
	{ 20000000, SYS_CLK_80, CLK_DIV_4 },
	{ 15000000, SYS_CLK_60, CLK_DIV_4 },
	{ 12000000, SYS_CLK_48, CLK_DIV_4 },
	{ 10000000, SYS_CLK_80, CLK_DIV_8 },
	{ 7500000, SYS_CLK_60, CLK_DIV_8 },
	{ 6000000, SYS_CLK_48, CLK_DIV_8 },
	{ 5000000, SYS_CLK_80, CLK_DIV_16 },
	{ 3750000, SYS_CLK_60, CLK_DIV_16 },
	{ 3000000, SYS_CLK_48, CLK_DIV_16 },
	{ 2500000, SYS_CLK_80, CLK_DIV_32 },
	{ 1875000, SYS_CLK_60, CLK_DIV_32 },
	{ 1500000, SYS_CLK_48, CLK_DIV_32 },
	{ 1250000, SYS_CLK_80, CLK_DIV_64 },
	{ 937500, SYS_CLK_60, CLK_DIV_64 },
	{ 750000, SYS_CLK_48, CLK_DIV_64 },
	{ 625000, SYS_CLK_80, CLK_DIV_128 },
	{ 468750, SYS_CLK_60, CLK_DIV_128 },
	{ 375000, SYS_CLK_48, CLK_DIV_128 },
	{ 312500, SYS_CLK_80, CLK_DIV_256 },
	{ 234375, SYS_CLK_60, CLK_DIV_256 },
	{ 187500, SYS_CLK_48, CLK_DIV_256 },
	{ 156250, SYS_CLK_80, CLK_DIV_512 },
	{ 117187, SYS_CLK_60, CLK_DIV_512 },
	{ 93750, SYS_CLK_48, CLK_DIV_512 },
	{ 46875, SYS_CLK_24, CLK_DIV_512 }
};

static const uint32_t ft4222_spi_clk_div[] = {
	[CLK_NONE] = 1,
	[CLK_DIV_2] = 2,
	[CLK_DIV_4] = 4,
	[CLK_DIV_8] = 8,
	[CLK_DIV_16] = 16,
	[CLK_DIV_32] = 32,
	[CLK_DIV_64] = 64,
	[CLK_DIV_128] = 128,
	[CLK_DIV_256] = 256,
	[CLK_DIV_512] = 512,
};

static ufprog_status ft4222_spi_end_generic_xfer(struct ufprog_if_dev *ftdev);

static enum ft4222_spi_drive_strength ft4222_num_to_drive_strength(uint32_t val)
{
	if (val < 8)
		return DS_4MA;
	else if (val < 12)
		return DS_8MA;
	else if (val < 16)
		return DS_12MA;
	else
		return DS_16MA;
}

static ufprog_status ft4222_spi_set_io_mode(struct ufprog_if_dev *ftdev, enum ft4222_spi_mode mode)
{
	ufprog_status ret = UFP_OK;
	uint8_t val = mode;

	os_mutex_lock(ftdev->lock);

	if (ftdev->spim.mode != mode) {
		ret = ftdi_vendor_cmd_set(ftdev->handle, 0x42, &val, 1);
		if (ret) {
			logm_err("Failed to set SPI IO mode\n");
			goto out;
		}

		ftdev->spim.mode = mode;
	}

	val = 1;
	ftdi_vendor_cmd_set(ftdev->handle, 0x4A, &val, 1);

out:
	os_mutex_unlock(ftdev->lock);

	return ret;
}

static ufprog_status ft4222_spi_set_cs_pol(struct ufprog_if_dev *ftdev, enum ft4222_spi_pol pol)
{
	ufprog_status ret = UFP_OK;
	uint8_t val = pol;

	os_mutex_lock(ftdev->lock);

	ret = ftdi_vendor_cmd_set(ftdev->handle, 0x43, &val, 1);
	if (ret) {
		logm_err("Failed to set SPI chip select polarity\n");
		goto out;
	}

	ftdev->spim.cs_pol = pol;

out:
	os_mutex_unlock(ftdev->lock);

	return ret;
}

static ufprog_status ft4222_spi_set_clock_divider(struct ufprog_if_dev *ftdev, enum ft4222_spi_clkdiv clkdiv)
{
	ufprog_status ret = UFP_OK;
	uint8_t val = clkdiv;

	os_mutex_lock(ftdev->lock);

	ret = ftdi_vendor_cmd_set(ftdev->handle, 0x44, &val, 1);
	if (ret) {
		logm_err("Failed to set SPI clock divider\n");
		goto out;
	}

	ftdev->spim.clkdiv = clkdiv;

out:
	os_mutex_unlock(ftdev->lock);

	return ret;
}

static ufprog_status ft4222_spi_set_cpol(struct ufprog_if_dev *ftdev, enum ft4222_spi_cpol cpol)
{
	ufprog_status ret = UFP_OK;
	uint8_t val = cpol;

	os_mutex_lock(ftdev->lock);

	ret = ftdi_vendor_cmd_set(ftdev->handle, 0x45, &val, 1);
	if (ret) {
		logm_err("Failed to set SPI CPOL\n");
		goto out;
	}

	ftdev->spim.cpol = cpol;

out:
	os_mutex_unlock(ftdev->lock);

	return ret;
}

static ufprog_status ft4222_spi_set_cpha(struct ufprog_if_dev *ftdev, enum ft4222_spi_cpha cpha)
{
	ufprog_status ret = UFP_OK;
	uint8_t val = cpha;

	os_mutex_lock(ftdev->lock);

	ret = ftdi_vendor_cmd_set(ftdev->handle, 0x46, &val, 1);
	if (ret) {
		logm_err("Failed to set SPI CPHA\n");
		goto out;
	}

	ftdev->spim.cpha = cpha;

out:
	os_mutex_unlock(ftdev->lock);

	return ret;
}

static ufprog_status ft4222_spi_set_sso_map(struct ufprog_if_dev *ftdev, uint32_t sso_map)
{
	ufprog_status ret;
	uint8_t mask, val;

	mask = (1 << ftdev->spim.max_cs) - 1;
	val = sso_map & mask;

	ret = ftdi_vendor_cmd_set(ftdev->handle, 0x48, &val, 1);
	if (ret) {
		logm_err("Failed to set SPI SSO map\n");
		return ret;
	}

	return UFP_OK;
}

static ufprog_status ft4222_spi_reset_transaction(struct ufprog_if_dev *ftdev, uint32_t index)
{
	ufprog_status ret;
	uint8_t val = (uint8_t)index;

	if (index >= ftdev->spim.max_cs) {
		logm_err("Invalid chip select\n");
		return UFP_INVALID_PARAMETER;
	}

	ret = ftdi_vendor_cmd_set(ftdev->handle, 0x49, &val, 1);
	if (ret)
		logm_warn("Failed to reset transaction of SPI index %u\n", index);

	return UFP_OK;
}

static ufprog_status ft4222_spi_set_driving_strength(struct ufprog_if_dev *ftdev, enum ft4222_spi_drive_strength clk,
						     enum ft4222_spi_drive_strength io,
						     enum ft4222_spi_drive_strength sso)
{
	ufprog_status ret;
	uint8_t val;

	val = (uint8_t)(sso | (io << 2) | (clk << 4));

	ret = ftdi_vendor_cmd_set(ftdev->handle, 0xA0, &val, 1);
	if (ret) {
		logm_err("Failed to set SPI driving strength\n");
		return ret;
	}

	return UFP_OK;
}

static ufprog_status ft4222_spi_master_set_clk(struct ufprog_if_dev *ftdev, uint32_t freq, uint32_t *out_freq)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(ft4222_spi_clks); i++) {
		if (freq >= ft4222_spi_clks[i].freq) {
			os_mutex_lock(ftdev->lock);
			ft4222_set_clock(ftdev, ft4222_spi_clks[i].clk);
			ft4222_spi_set_clock_divider(ftdev, ft4222_spi_clks[i].div);
			os_mutex_unlock(ftdev->lock);

			if (out_freq)
				*out_freq = ft4222_spi_clks[i].freq;

			return UFP_OK;
		}
	}

	if (out_freq)
		*out_freq = 0;

	logm_err("Requested SPI clock %uHz is too small\n", freq);

	return UFP_UNSUPPORTED;
}

ufprog_status ft4222_spi_master_init(struct ufprog_if_dev *ftdev, struct json_object *config)
{
	uint32_t ds_clk, ds_io, ds_cs;
	struct json_object *dscfg;

	switch (ftdev->hwcaps.chip_mode) {
	case 0:
		ftdev->spim.max_cs = 1;
		break;

	case 1:
		ftdev->spim.max_cs = 3;
		break;

	case 2:
		ftdev->spim.max_cs = 4;
		break;

	case 3:
		ftdev->spim.max_cs = 1;
		break;

	default:
		ftdev->spim.max_cs = 1;
		logm_warn("Invalid chip mode\n");
	}

	STATUS_CHECK_RET(ft4222_spi_reset_transaction(ftdev, 0));

	STATUS_CHECK_RET(ft4222_set_clock(ftdev, SYS_CLK_24));

	STATUS_CHECK_RET(ft4222_spi_set_io_mode(ftdev, SPI_IO_SINGLE));
	STATUS_CHECK_RET(ft4222_spi_set_clock_divider(ftdev, CLK_DIV_2));
	STATUS_CHECK_RET(ft4222_spi_set_cpol(ftdev, CLK_IDLE_LOW));
	STATUS_CHECK_RET(ft4222_spi_set_cpha(ftdev, CLK_LEADING));
	STATUS_CHECK_RET(ft4222_spi_set_cs_pol(ftdev, CS_ACTIVE_NEGTIVE));

	if (!json_read_uint32(config, "chip-select", &ftdev->spim.curr_cs, 0)) {
		if (ftdev->spim.curr_cs >= ftdev->spim.max_cs) {
			logm_err("Invalid chip select in configuration\n");
			return UFP_DEVICE_INVALID_CONFIG;
		}

		ft4222_spi_set_sso_map(ftdev, 1 << ftdev->spim.curr_cs);
	}

	if (!json_read_obj(config, "drive-strength", &dscfg)) {
		if (json_read_uint32(dscfg, "clk", &ds_clk, 8)) {
			logm_err("Invalid drive strength of CLK in configuration\n");
			return UFP_DEVICE_INVALID_CONFIG;
		}

		if (json_read_uint32(dscfg, "io", &ds_io, 8)) {
			logm_err("Invalid drive strength of IO in configuration\n");
			return UFP_DEVICE_INVALID_CONFIG;
		}

		if (json_read_uint32(dscfg, "cs", &ds_cs, 8)) {
			logm_err("Invalid drive strength of CS in configuration\n");
			return UFP_DEVICE_INVALID_CONFIG;
		}

		ds_clk = ft4222_num_to_drive_strength(ds_clk);
		ds_io = ft4222_num_to_drive_strength(ds_io);
		ds_cs = ft4222_num_to_drive_strength(ds_cs);

		STATUS_CHECK_RET(ft4222_spi_set_driving_strength(ftdev, ds_clk, ds_io, ds_cs));
	}

	STATUS_CHECK_RET(ft4222_set_function(ftdev, FT4222_SPI_MASTER));
	STATUS_CHECK_RET(ft4222_spi_end_generic_xfer(ftdev));

	ftdev->scratch_buffer = malloc(FT4222_MULTIIO_BUF_LEN);
	if (!ftdev->scratch_buffer) {
		logm_err("No memory for scratch buffer\n");
		return UFP_NOMEM;
	}

	return UFP_OK;
}

ufprog_status ft4222_spi_master_cleanup(struct ufprog_if_dev *ftdev)
{
	free(ftdev->scratch_buffer);

	return UFP_OK;
}

uint32_t UFPROG_API ufprog_spi_if_version(void)
{
	return MAKE_VERSION(FT4222H_SPI_IF_MAJOR, FT4222H_SPI_IF_MINOR);
}

uint32_t UFPROG_API ufprog_spi_if_caps(void)
{
	return 0;
}

size_t UFPROG_API ufprog_spi_max_read_granularity(void)
{
	return FT4222_SINGLEIO_XFER_MAX_LEN - 0xf /* Reserve room for opcode/addr/dummy in single mode */;
}

ufprog_status UFPROG_API ufprog_spi_set_cs_pol(struct ufprog_if_dev *ftdev, ufprog_bool positive)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (ftdev->hwcaps.function_mode != FT4222_SPI_MASTER) {
		logm_err("Chip is not in SPI mode\n");
		return UFP_UNSUPPORTED;
	}

	return ft4222_spi_set_cs_pol(ftdev, positive ? CS_ACTIVE_POSTIVE : CS_ACTIVE_NEGTIVE);
}

ufprog_status UFPROG_API ufprog_spi_set_mode(struct ufprog_if_dev *ftdev, uint32_t mode)
{
	ufprog_status ret_cpol, ret_cpha;

	if (!ftdev || mode > 3)
		return UFP_INVALID_PARAMETER;

	if (ftdev->hwcaps.function_mode != FT4222_SPI_MASTER) {
		logm_err("Chip is not in SPI mode\n");
		return UFP_UNSUPPORTED;
	}

	os_mutex_lock(ftdev->lock);
	ret_cpol = ft4222_spi_set_cpol(ftdev, mode & SPI_MODE_CPOL ? CLK_IDLE_HIGH : CLK_IDLE_LOW);
	ret_cpha = ft4222_spi_set_cpha(ftdev, mode & SPI_MODE_CPHA ? CLK_TRAILING : CLK_LEADING);
	os_mutex_unlock(ftdev->lock);

	if (ret_cpol || ret_cpha)
		return UFP_DEVICE_IO_ERROR;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_set_speed(struct ufprog_if_dev *ftdev, uint32_t hz, uint32_t *rethz)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (ftdev->hwcaps.function_mode != FT4222_SPI_MASTER) {
		logm_err("Chip is not in SPI mode\n");
		return UFP_UNSUPPORTED;
	}

	return ft4222_spi_master_set_clk(ftdev, hz, rethz);
}

uint32_t UFPROG_API ufprog_spi_get_speed(struct ufprog_if_dev *ftdev)
{
	if (!ftdev)
		return 0;

	if (ftdev->hwcaps.function_mode != FT4222_SPI_MASTER) {
		logm_err("Chip is not in SPI mode\n");
		return 0;
	}

	return ft4222_sys_clks[ftdev->hwcaps.clk] / ft4222_spi_clk_div[ftdev->spim.clkdiv];
}

uint32_t UFPROG_API ufprog_spi_get_speed_list(struct ufprog_if_dev *ftdev, uint32_t *retlist, uint32_t count)
{
	uint32_t i;

	if (!retlist || !count)
		return ARRAY_SIZE(ft4222_spi_clks);

	if (count > ARRAY_SIZE(ft4222_spi_clks))
		count = ARRAY_SIZE(ft4222_spi_clks);

	for (i = 0; i < count; i++)
		retlist[i] = ft4222_spi_clks[i].freq;

	return ARRAY_SIZE(ft4222_spi_clks);
}

static ufprog_status ft4222_spi_end_generic_xfer(struct ufprog_if_dev *ftdev)
{
	ufprog_status ret;

	ret = ftdi_write(ftdev->handle, NULL, 0);
	if (ret)
		logm_err("Failed to send zero-length packet\n");

	return ret;
}

static ufprog_status ft4222_spi_generic_xfer_one(struct ufprog_if_dev *ftdev, const struct ufprog_spi_transfer *xfer)
{
	const uint8_t *ptx;
	uint16_t chksz;
	uint8_t *prx;
	size_t len;

	if (xfer->buswidth > 1 || xfer->dtr) {
		logm_err("Only single I/O single rate is supported in generic transfer mode\n");
		return UFP_UNSUPPORTED;
	}

	if (xfer->speed)
		STATUS_CHECK_RET(ft4222_spi_master_set_clk(ftdev, xfer->speed, NULL));

	len = xfer->len;

	if (xfer->dir == SPI_DATA_IN) {
		ptx = ftdev->scratch_buffer;
		prx = xfer->buf.rx;

		if (len) {
			if (len > FT4222_SINGLEIO_XFER_MAX_LEN)
				chksz = FT4222_SINGLEIO_XFER_MAX_LEN;
			else
				chksz = (uint16_t)len;

			memset(ftdev->scratch_buffer, 0xff, chksz);
		}
	} else {
		ptx = xfer->buf.tx;
		prx = ftdev->scratch_buffer;
	}

	while (len) {
		if (len > FT4222_SINGLEIO_XFER_MAX_LEN)
			chksz = FT4222_SINGLEIO_XFER_MAX_LEN;
		else
			chksz = (uint16_t)len;

		STATUS_CHECK_RET(ftdi_write(ftdev->handle, ptx, chksz));
		STATUS_CHECK_RET(ftdi_read(ftdev->handle, prx, chksz));

		if (xfer->dir == SPI_DATA_IN)
			prx += chksz;
		else
			ptx += chksz;

		len -= chksz;
	}

	if (xfer->end)
		return ft4222_spi_end_generic_xfer(ftdev);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_generic_xfer(struct ufprog_if_dev *ftdev, const struct ufprog_spi_transfer *xfers,
						 uint32_t count)
{
	ufprog_status ret = UFP_OK;
	uint32_t i;

	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (ftdev->hwcaps.function_mode != FT4222_SPI_MASTER) {
		logm_err("Chip is not in SPI mode\n");
		return UFP_UNSUPPORTED;
	}

	STATUS_CHECK_RET(ft4222_spi_set_io_mode(ftdev, SPI_IO_SINGLE));

	os_mutex_lock(ftdev->lock);

	for (i = 0; i < count; i++) {
		ret = ft4222_spi_generic_xfer_one(ftdev, &xfers[i]);
		if (ret) {
			ft4222_spi_end_generic_xfer(ftdev);
			goto out;
		}
	}

out:
	os_mutex_unlock(ftdev->lock);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_mem_adjust_op_size(struct ufprog_if_dev *ftdev, struct ufprog_spi_mem_op *op)
{
	uint32_t bw = 0;

	if (op->cmd.len) {
		if (bw < op->cmd.buswidth)
			bw = op->cmd.buswidth;
	}

	if (op->addr.len) {
		if (bw < op->addr.buswidth)
			bw = op->addr.buswidth;
	}

	if (op->dummy.len) {
		if (bw < op->dummy.buswidth)
			bw = op->dummy.buswidth;
	}

	if (op->data.len) {
		if (bw < op->data.buswidth)
			bw = op->data.buswidth;
	}

	if (bw <= 1)
		return UFP_OK;

	if (op->data.len > FT4222_MULTIIO_MIO_WR_MAX_LEN)
		op->data.len = FT4222_MULTIIO_MIO_WR_MAX_LEN;

	return UFP_OK;
}

#define FT4222_CHECK_SPI_MEM_OP_PART(_part)					\
	if (op->_part.len) {							\
		if (op->_part.buswidth != 1 && op->_part.buswidth != 2 &&	\
		    op->_part.buswidth != 4)					\
			return UFP_UNSUPPORTED;					\
										\
		if (op->_part.dtr)						\
			return UFP_UNSUPPORTED;					\
										\
		if (op->_part.buswidth < curr_bw)				\
			return UFP_UNSUPPORTED;					\
										\
		if (op->_part.buswidth > 1) {					\
			if (curr_bw > 1 && op->_part.buswidth != curr_bw)	\
				return UFP_UNSUPPORTED;				\
										\
			mio_wr_len += op->_part.len;				\
		} else {							\
			sio_wr_len += op->_part.len;				\
		}								\
										\
		curr_bw = op->_part.buswidth;					\
	}

ufprog_bool UFPROG_API ufprog_spi_mem_supports_op(struct ufprog_if_dev *ftdev, const struct ufprog_spi_mem_op *op)
{
	size_t sio_wr_len = 0, mio_wr_len = 0, mio_rd_len = 0;
	uint32_t curr_bw = 0;

	FT4222_CHECK_SPI_MEM_OP_PART(cmd);
	FT4222_CHECK_SPI_MEM_OP_PART(addr);
	FT4222_CHECK_SPI_MEM_OP_PART(dummy);

	if (op->data.len) {
		if (op->data.buswidth != 1 && op->data.buswidth != 2 && op->data.buswidth != 4)
			return false;

		if (op->data.dtr)
			return false;

		if (op->data.buswidth < curr_bw)
			return false;

		if (op->data.buswidth > 1) {
			if (curr_bw > 1 && op->data.buswidth != curr_bw)
				return false;

			if (op->data.dir == SPI_DATA_IN)
				mio_rd_len += op->data.len;
			else
				mio_wr_len += op->data.len;
		} else {
			if (op->data.dir != SPI_DATA_IN)
				sio_wr_len += op->data.len;
		}
	}

	if (!mio_wr_len && !mio_rd_len) {
		/* Make sure all outgoing bytes can be sent once */
		if (sio_wr_len <= FT4222_SINGLEIO_XFER_MAX_LEN)
			/* Valid single I/O xfer */
			return true;

		return false;
	}

	/* Make sure all outgoing bytes fit in scratch buffer */
	if (sio_wr_len <= FT4222_MULTIIO_SIO_WR_MAX_LEN) {
		/* Valid multi I/O xfer */
		return true;
	}

	return false;
}

ufprog_status UFPROG_API ufprog_spi_mem_exec_op(struct ufprog_if_dev *ftdev, const struct ufprog_spi_mem_op *op)
{
	size_t len, chksz, sio_wr_len = 0, sio_rd_len = 0, mio_wr_len = 0, mio_rd_len = 0;
	bool sio_write_once = false;
	ufprog_status ret = UFP_OK;
	uint8_t *buf, *buf_data;
	const uint8_t *tbuf;
	uint32_t i, bw = 0;

	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	buf = ftdev->scratch_buffer + FT4222_MULTIIO_CMD_LEN;
	buf_data = buf;

	if (op->cmd.len) {
		if (op->cmd.buswidth > 1)
			mio_wr_len += op->cmd.len;
		else
			sio_wr_len += op->cmd.len;

		*buf++ = op->cmd.opcode & 0xff;
		bw = op->cmd.buswidth;
	}

	if (op->addr.len) {
		if (op->addr.buswidth > 1)
			mio_wr_len += op->addr.len;
		else
			sio_wr_len += op->addr.len;

		for (i = 0; i < op->addr.len; i++)
			*buf++ = (op->addr.val >> (8 * (op->addr.len - i - 1))) & 0xff;

		bw = op->addr.buswidth;
	}

	if (op->dummy.len) {
		if (op->dummy.buswidth > 1)
			mio_wr_len += op->dummy.len;
		else
			sio_wr_len += op->dummy.len;

		memset(buf, 0xff, op->dummy.len);
		buf += op->dummy.len;
		bw = op->dummy.buswidth;
	}

	if (op->data.len) {
		if (op->data.dir == SPI_DATA_IN) {
			if (op->data.buswidth > 1) {
				mio_rd_len += op->data.len;
			} else {
				sio_rd_len += op->data.len;

				if (sio_wr_len + sio_rd_len <= FT4222_SINGLEIO_XFER_MAX_LEN - sio_wr_len)
					sio_write_once = true;
			}
		} else {
			if (op->data.buswidth > 1) {
				mio_wr_len += op->data.len;
				memcpy(buf, op->data.buf.tx, op->data.len);
			} else {
				if (op->data.len <= FT4222_SINGLEIO_XFER_MAX_LEN - sio_wr_len) {
					memcpy(buf, op->data.buf.tx, op->data.len);
					sio_wr_len += op->data.len;
					sio_write_once = true;
				}
			}
		}

		bw = op->data.buswidth;
	} else {
		sio_write_once = true;
	}

	os_mutex_lock(ftdev->lock);

	if (bw == 1) {
		/* Do single I/O xfer */
		STATUS_CHECK_GOTO_RET(ft4222_spi_set_io_mode(ftdev, SPI_IO_SINGLE), ret, out);

		/* Optimization for one-time write(-then-read) */
		if (sio_write_once) {
			if (sio_rd_len) {
				/* Set dummy data for receiving */
				memset(buf_data + sio_wr_len, 0xff, sio_rd_len);
			}

			/* Send all outgoing data including dummy data for receiving */
			STATUS_CHECK_GOTO_RET(ftdi_write(ftdev->handle, buf_data, sio_wr_len + sio_rd_len), ret, out);

			/* Discard readback data */
			STATUS_CHECK_GOTO_RET(ftdi_read(ftdev->handle, ftdev->scratch_buffer, sio_wr_len), ret, out);

			if (sio_rd_len) {
				/* Read data */
				STATUS_CHECK_GOTO_RET(ftdi_read(ftdev->handle, op->data.buf.rx, sio_rd_len), ret, out);
			}

			STATUS_CHECK_GOTO_RET(ft4222_spi_end_generic_xfer(ftdev), ret, out);

			goto out;
		}

		/* Send all outgoing data */
		STATUS_CHECK_GOTO_RET(ftdi_write(ftdev->handle, buf_data, sio_wr_len), ret, out);
		/* Discard all readback data */
		STATUS_CHECK_GOTO_RET(ftdi_read(ftdev->handle, ftdev->scratch_buffer, sio_wr_len), ret, out);

		if (op->data.dir == SPI_DATA_IN) {
			if (sio_rd_len > FT4222_SINGLEIO_XFER_MAX_LEN)
				chksz = FT4222_SINGLEIO_XFER_MAX_LEN;
			else
				chksz = sio_rd_len;

			memset(ftdev->scratch_buffer, 0xff, chksz);

			len = op->data.len;
			buf = op->data.buf.rx;

			while (len) {
				if (len > FT4222_SINGLEIO_XFER_MAX_LEN)
					chksz = FT4222_SINGLEIO_XFER_MAX_LEN;
				else
					chksz = len;

				/* Send dummy data for receiving */
				STATUS_CHECK_GOTO_RET(ftdi_write(ftdev->handle, ftdev->scratch_buffer, chksz), ret,
								 out);
				/* Read data */
				STATUS_CHECK_GOTO_RET(ftdi_read(ftdev->handle, buf, chksz), ret, out);

				len -= chksz;
				buf += chksz;
			}
		} else {
			len = op->data.len;
			tbuf = op->data.buf.tx;

			while (len) {
				if (len > FT4222_SINGLEIO_XFER_MAX_LEN)
					chksz = FT4222_SINGLEIO_XFER_MAX_LEN;
				else
					chksz = len;

				/* Send all outgoing data */
				STATUS_CHECK_GOTO_RET(ftdi_write(ftdev->handle, tbuf, chksz), ret, out);
				/* Discard all readback data */
				STATUS_CHECK_GOTO_RET(ftdi_read(ftdev->handle, ftdev->scratch_buffer, chksz), ret, out);

				len -= chksz;
				tbuf += chksz;
			}
		}

		STATUS_CHECK_GOTO_RET(ft4222_spi_end_generic_xfer(ftdev), ret, out);

		goto out;
	}

	/* Do multi I/O xfer */
	STATUS_CHECK_GOTO_RET(ft4222_spi_set_io_mode(ftdev, bw == 4 ? SPI_IO_QUAD : SPI_IO_DUAL), ret, out);

	ftdev->scratch_buffer[0] = (sio_wr_len & 0xF) | 0x80;
	ftdev->scratch_buffer[1] = (mio_wr_len >> 8) & 0xff;
	ftdev->scratch_buffer[2] = mio_wr_len & 0xff;
	ftdev->scratch_buffer[3] = (mio_rd_len >> 8) & 0xff;
	ftdev->scratch_buffer[4] = mio_rd_len & 0xff;

	if (ftdev->hwver.fwver >= 3) {
		/* Send all outgoing data */
		len = FT4222_MULTIIO_CMD_LEN + sio_wr_len + mio_wr_len;
		STATUS_CHECK_GOTO_RET(ftdi_write(ftdev->handle, ftdev->scratch_buffer, len), ret, out);

		if (mio_rd_len)
			/* Read data */
			STATUS_CHECK_GOTO_RET(ftdi_read(ftdev->handle, op->data.buf.rx, mio_rd_len), ret, out);

		goto out;
	}

	/* FT4222H with older revision only supports max buck size per xfer */
	len = FT4222_MULTIIO_CMD_LEN + sio_wr_len + mio_wr_len;
	buf = ftdev->scratch_buffer;

	while (len) {
		if (len > ftdev->max_buck_size)
			chksz = ftdev->max_buck_size;
		else
			chksz = len;

		STATUS_CHECK_GOTO_RET(ftdi_write(ftdev->handle, buf, chksz), ret, out);

		len -= chksz;
		buf += chksz;
	}

	if (!mio_rd_len)
		goto out;

	buf = op->data.buf.rx;
	len = mio_rd_len;

	while (len) {
		if (len > ftdev->max_buck_size)
			chksz = ftdev->max_buck_size;
		else
			chksz = len;

		STATUS_CHECK_GOTO_RET(ftdi_read(ftdev->handle, buf, chksz), ret, out);

		len -= chksz;
		buf += chksz;
	}

out:
	os_mutex_unlock(ftdev->lock);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_drive_4io_ones(struct ufprog_if_dev *ftdev, uint32_t clocks)
{
	uint8_t buf[16], *pbuf;
	ufprog_status ret;
	uint32_t cnt;

	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_NO_CMD,
		SPI_MEM_OP_NO_ADDR,
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_DATA_OUT(clocks / 2, NULL, 4)
	);

	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (!clocks)
		return UFP_OK;

	if (clocks % 2)
		return UFP_UNSUPPORTED;

	cnt = clocks / 2;

	if (cnt <= sizeof(buf))
		pbuf = buf;
	else
		pbuf = malloc(cnt);

	if (!pbuf) {
		logm_err("No memory for temp buffer for driving 0Fh on all I/Os\n");
		return UFP_NOMEM;
	}

	memset(pbuf, 0xff, cnt);
	op.data.buf.tx = pbuf;

	ret = ufprog_spi_mem_exec_op(ftdev, &op);

	if (pbuf != buf)
		free(pbuf);

	return ret;
}
