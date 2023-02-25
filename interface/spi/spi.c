/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI interface abstraction layer
 */

#include <malloc.h>
#include <stdbool.h>
#include <string.h>
#include <ufprog/osdef.h>
#include <ufprog/log.h>
#include "spi.h"

#define SPI_MEM_IO_INFO(_io_type, _cmd_bits, _addr_bits, _data_bits, _cmd_dtr, _addr_dtr, _data_dtr)	\
	[_io_type] = FIELD_SET(SPI_MEM_CMD_BW, (_cmd_bits)) | FIELD_SET(SPI_MEM_ADDR_BW, (_addr_bits)) | \
		     FIELD_SET(SPI_MEM_DATA_BW, (_data_bits)) | ((_cmd_dtr) ? SPI_MEM_CMD_DTR : 0) | \
		     ((_addr_dtr) ? SPI_MEM_ADDR_DTR : 0) | ((_data_dtr) ? SPI_MEM_DATA_DTR : 0)

static const uint32_t spi_mem_io_bus_width_info[__SPI_MEM_IO_MAX] = {
	SPI_MEM_IO_INFO(SPI_MEM_IO_1_1_1, 1, 1, 1, 0, 0, 0),
	SPI_MEM_IO_INFO(SPI_MEM_IO_1S_1D_1D, 1, 1, 1, 0, 1, 1),
	SPI_MEM_IO_INFO(SPI_MEM_IO_1D_1D_1D, 1, 1, 1, 1, 1, 1),

	SPI_MEM_IO_INFO(SPI_MEM_IO_1_1_2, 1, 1, 2, 0, 0, 0),
	SPI_MEM_IO_INFO(SPI_MEM_IO_1_2_2, 1, 2, 2, 0, 0, 0),
	SPI_MEM_IO_INFO(SPI_MEM_IO_2_2_2, 2, 2, 2, 0, 0, 0),
	SPI_MEM_IO_INFO(SPI_MEM_IO_1S_2D_2D, 1, 2, 2, 0, 1, 1),
	SPI_MEM_IO_INFO(SPI_MEM_IO_2D_2D_2D, 2, 2, 2, 1, 1, 1),

	SPI_MEM_IO_INFO(SPI_MEM_IO_1_1_4, 1, 1, 4, 0, 0, 0),
	SPI_MEM_IO_INFO(SPI_MEM_IO_1_4_4, 1, 4, 4, 0, 0, 0),
	SPI_MEM_IO_INFO(SPI_MEM_IO_4_4_4, 4, 4, 4, 0, 0, 0),
	SPI_MEM_IO_INFO(SPI_MEM_IO_1S_4D_4D, 1, 4, 4, 0, 1, 1),
	SPI_MEM_IO_INFO(SPI_MEM_IO_4D_4D_4D, 4, 4, 4, 1, 1, 1),

	SPI_MEM_IO_INFO(SPI_MEM_IO_1_1_8, 1, 1, 8, 0, 0, 0),
	SPI_MEM_IO_INFO(SPI_MEM_IO_1_8_8, 1, 8, 8, 0, 0, 0),
	SPI_MEM_IO_INFO(SPI_MEM_IO_8_8_8, 8, 8, 8, 0, 0, 0),
	SPI_MEM_IO_INFO(SPI_MEM_IO_1S_8D_8D, 1, 8, 8, 0, 1, 1),
	SPI_MEM_IO_INFO(SPI_MEM_IO_8D_8D_8D, 8, 8, 8, 1, 1, 1),
};

#define SPI_MEM_IO_NAME(_io_type, _name)	[_io_type] = (_name)

static const char *spi_mem_io_name[__SPI_MEM_IO_MAX] = {
	SPI_MEM_IO_NAME(SPI_MEM_IO_1_1_1, "1-1-1"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_1S_1D_1D, "1S-1D-1D"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_1D_1D_1D, "1D-1D-1D"),

	SPI_MEM_IO_NAME(SPI_MEM_IO_1_1_2, "1-1-2"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_1_2_2, "1-2-2"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_2_2_2, "2-2-2"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_1S_2D_2D, "1S-2D-2D"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_2D_2D_2D, "2D-2D-2D"),

	SPI_MEM_IO_NAME(SPI_MEM_IO_1_1_4, "1-1-4"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_1_4_4, "1-4-4"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_4_4_4, "4-4-4"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_1S_4D_4D, "1S-4D-4D"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_4D_4D_4D, "4D-4D-4D"),

	SPI_MEM_IO_NAME(SPI_MEM_IO_1_1_8, "1-1-8"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_1_8_8, "1-8-8"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_8_8_8, "8-8-8"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_1S_8D_8D, "1S-8D-8D"),
	SPI_MEM_IO_NAME(SPI_MEM_IO_8D_8D_8D, "8D-8D-8D"),
};

static void ufprog_spi_get_optional_symbols(struct ufprog_spi *spi)
{
	struct symbol_find_entry optional_symbols[] = {
		FIND_MODULE(API_NAME_SPI_SET_CS_POL, spi->set_cs_pol),
		FIND_MODULE(API_NAME_SPI_SET_MODE, spi->set_mode),
		FIND_MODULE(API_NAME_SPI_SET_SPEED, spi->set_speed),
		FIND_MODULE(API_NAME_SPI_GET_SPEED, spi->get_speed),
		FIND_MODULE(API_NAME_SPI_GET_SPEED_RANGE, spi->get_speed_range),
		FIND_MODULE(API_NAME_SPI_GET_SPEED_LIST, spi->get_speed_list),
		FIND_MODULE(API_NAME_SPI_SET_WP, spi->set_wp),
		FIND_MODULE(API_NAME_SPI_SET_HOLD, spi->set_hold),
		FIND_MODULE(API_NAME_SPI_SET_BUSY_IND, spi->set_busy_ind),
	};

	ufprog_driver_find_module_symbols(ufprog_device_get_driver(spi->dev), optional_symbols,
					  ARRAY_SIZE(optional_symbols),false);
}

static ufprog_status ufprog_spi_get_xfer_symbols(struct ufprog_spi *spi)
{
	api_spi_max_read_granularity spi_if_max_read_granularity;
	api_spi_generic_xfer_max_size generic_xfer_max_size;
	api_spi_if_version spi_if_ver;
	api_spi_if_caps spi_if_caps;
	struct ufprog_driver *drv;
	bool supports_spi_mem;

	struct symbol_find_entry if_basic_symbols[] = {
		FIND_MODULE(API_NAME_SPI_IF_VERSION, spi_if_ver),
		FIND_MODULE(API_NAME_SPI_IF_CAPS, spi_if_caps),
	};

	struct symbol_find_entry optional_symbols[] = {
		FIND_MODULE(API_NAME_SPI_MAX_READ_GRANULARITY, spi_if_max_read_granularity),
		FIND_MODULE(API_NAME_SPI_GENERIC_XFER, spi->generic_xfer),
		FIND_MODULE(API_NAME_SPI_GENERIC_XFER_MAX_SIZE, generic_xfer_max_size),
		FIND_MODULE(API_NAME_SPI_MEM_POLL_STATUS, spi->poll_status),
		FIND_MODULE(API_NAME_SPI_DRIVE_4IO_ONES, spi->drive_4io_ones),
	};

	struct symbol_find_entry spi_mem_symbols[] = {
		FIND_MODULE(API_NAME_SPI_MEM_ADJUST_OP_SIZE, spi->adjust_op_size),
		FIND_MODULE(API_NAME_SPI_MEM_SUPPORTS_OP, spi->supports_op),
		FIND_MODULE(API_NAME_SPI_MEM_EXEC_OP, spi->exec_op),
	};

	drv = ufprog_device_get_driver(spi->dev);

	/* Basic interface API */
	if (!ufprog_driver_find_module_symbols(drv, if_basic_symbols, ARRAY_SIZE(if_basic_symbols), true)) {
		logm_err("Interface driver is missing basic symbols\n");
		return UFP_MODULE_MISSING_SYMBOL;
	}

	spi->if_ver = spi_if_ver();
	if (GET_MAJOR_VERSION(spi->if_ver) != UFPROG_SPI_IF_MAJOR) {
		logm_err("The SPI API version %u is not supported. %u required\n", GET_MAJOR_VERSION(spi->if_ver),
			UFPROG_SPI_IF_MAJOR);
		return UFP_UNSUPPORTED;
	}

	spi->caps = spi_if_caps();

	ufprog_driver_find_module_symbols(drv, optional_symbols, ARRAY_SIZE(optional_symbols), false);

	/* SPI-MEM interface API */
	supports_spi_mem = ufprog_driver_find_module_symbols(drv, spi_mem_symbols, ARRAY_SIZE(spi_mem_symbols), true);

	/* Generic SPI interface API */
	if (spi->generic_xfer) {
		if (generic_xfer_max_size)
			spi->generic_xfer_max_size = generic_xfer_max_size();
		else
			spi->generic_xfer_max_size = SIZE_MAX;
	}

	if (spi_if_max_read_granularity)
		spi->max_read_granularity = spi_if_max_read_granularity();
	else
		spi->max_read_granularity = SIZE_MAX;

	if (!supports_spi_mem && !spi->generic_xfer) {
		logm_err("Interface driver does not support any type of SPI transfers\n");
		return UFP_MODULE_MISSING_SYMBOL;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_attach_device(struct ufprog_device *dev, struct ufprog_spi **outspi)
{
	struct ufprog_spi *spi;
	ufprog_status ret;

	if (!outspi)
		return UFP_INVALID_PARAMETER;

	*outspi = NULL;

	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (ufprog_device_if_type(dev) != IF_SPI)
		return UFP_UNSUPPORTED;

	spi = calloc(1, sizeof(*spi));
	if (!spi) {
		logm_err("No memory for SPI abstraction device\n");
		return UFP_NOMEM;
	}

	spi->xfer_buffer = malloc(UFPROG_SPI_XFER_BUFFER_LEN);
	if (!spi->xfer_buffer) {
		free(spi);
		logm_err("No memory for SPI buffer\n");
		return UFP_NOMEM;
	}

	spi->xfer_buffer_len = UFPROG_SPI_XFER_BUFFER_LEN;
	spi->dev = dev;
	spi->ifdev = ufprog_device_get_interface_device(dev);

	ufprog_spi_get_optional_symbols(spi);

	STATUS_CHECK_GOTO_RET(ufprog_spi_get_xfer_symbols(spi), ret, cleanup);

	if (spi->get_speed_range) {
		ret = spi->get_speed_range(spi->ifdev, &spi->speed_min, &spi->speed_max);
		if (ret) {
			logm_err("Unable to get speed range\n");
			goto cleanup;
		}
	} else if (spi->get_speed_list) {
		spi->num_speeds = spi->get_speed_list(spi->ifdev, NULL, 0);
		if (!spi->num_speeds) {
			logm_err("Unable to get number of available speeds\n");
			ret = UFP_DEVICE_IO_ERROR;
			goto cleanup;
		}

		spi->speed_list = malloc(sizeof(*spi->speed_list) * spi->num_speeds);
		if (!spi->speed_list) {
			logm_err("No memory for available speed list\n");
			ret = UFP_NOMEM;
			goto cleanup;
		}

		spi->get_speed_list(spi->ifdev, spi->speed_list, spi->num_speeds);

		spi->speed_max = spi->speed_list[0];
		spi->speed_min = spi->speed_list[spi->num_speeds - 1];
	}

	*outspi = spi;
	return UFP_OK;

cleanup:
	if (spi->speed_list)
		free(spi->speed_list);

	if (spi->xfer_buffer)
		free(spi->xfer_buffer);

	free(spi);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_open_device(const char *name, ufprog_bool thread_safe, struct ufprog_spi **outspi)
{
	struct ufprog_device *dev;
	ufprog_status ret;

	if (!outspi)
		return UFP_INVALID_PARAMETER;

	*outspi = NULL;

	if (!name)
		return UFP_INVALID_PARAMETER;

	STATUS_CHECK_RET(ufprog_open_device_by_name(name, IF_SPI, thread_safe, &dev));

	ret = ufprog_spi_attach_device(dev, outspi);
	if (ret) {
		ufprog_close_device(dev);
		return ret;
	}

	return UFP_OK;
}

struct ufprog_device *UFPROG_API ufprog_spi_get_device(struct ufprog_spi *spi)
{
	if (!spi)
		return NULL;

	return spi->dev;
}

ufprog_status UFPROG_API ufprog_spi_close_device(struct ufprog_spi *spi)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (spi->dev) {
		STATUS_CHECK_RET(ufprog_close_device(spi->dev));
		spi->dev = NULL;
	}

	free(spi);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_bus_lock(struct ufprog_spi *spi)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	return ufprog_lock_device(spi->dev);
}

ufprog_status UFPROG_API ufprog_spi_bus_unlock(struct ufprog_spi *spi)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	return ufprog_unlock_device(spi->dev);
}

uint32_t UFPROG_API ufprog_spi_if_caps(struct ufprog_spi *spi)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	return spi->caps;
}

size_t UFPROG_API ufprog_spi_max_read_granularity(struct ufprog_spi *spi)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	return spi->max_read_granularity;
}

ufprog_status UFPROG_API ufprog_spi_set_cs_pol(struct ufprog_spi *spi, ufprog_bool positive)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (!spi->set_cs_pol)
		return UFP_UNSUPPORTED;

	return spi->set_cs_pol(spi->ifdev, positive);
}

ufprog_status UFPROG_API ufprog_spi_set_mode(struct ufprog_spi *spi, uint32_t mode)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (!spi->set_mode)
		return UFP_UNSUPPORTED;

	return spi->set_mode(spi->ifdev, mode);
}

ufprog_status UFPROG_API ufprog_spi_set_speed(struct ufprog_spi *spi, uint32_t hz, uint32_t *rethz)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (!spi->set_speed)
		return UFP_UNSUPPORTED;

	return spi->set_speed(spi->ifdev, hz, rethz);
}

ufprog_status UFPROG_API ufprog_spi_set_speed_closest(struct ufprog_spi *spi, uint32_t freq, uint32_t *retfreq)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (!spi->set_speed || !spi->speed_max)
		return UFP_UNSUPPORTED;

	if (freq < spi->speed_min)
		freq = spi->speed_min;

	if (freq > spi->speed_max)
		freq = spi->speed_max;

	return spi->set_speed(spi->ifdev, freq, retfreq);
}

uint32_t UFPROG_API ufprog_spi_get_speed(struct ufprog_spi *spi)
{
	if (!spi)
		return 0;

	if (!spi->set_speed)
		return  0;

	return spi->get_speed(spi->ifdev);
}

ufprog_status UFPROG_API ufprog_spi_get_speed_range(struct ufprog_spi *spi, uint32_t *retlowhz, uint32_t *rethighhz)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (!spi->get_speed_range)
		return UFP_UNSUPPORTED;

	return spi->get_speed_range(spi->ifdev, retlowhz, rethighhz);
}

uint32_t UFPROG_API ufprog_spi_get_speed_list(struct ufprog_spi *spi, uint32_t *retlist, int32_t count)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (!spi->get_speed_list)
		return UFP_UNSUPPORTED;

	return spi->get_speed_list(spi->ifdev, retlist, count);
}

ufprog_status UFPROG_API ufprog_spi_get_speed_limit(struct ufprog_spi *spi, uint32_t *retmin, uint32_t *retmax)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (retmin)
		*retmin = spi->speed_min;

	if (retmax)
		*retmax = spi->speed_max;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_set_wp(struct ufprog_spi *spi, ufprog_bool high)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (!spi->set_wp)
		return UFP_UNSUPPORTED;

	return spi->set_wp(spi->ifdev, high);
}

ufprog_status UFPROG_API ufprog_spi_set_hold(struct ufprog_spi *spi, ufprog_bool high)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (!spi->set_hold)
		return UFP_UNSUPPORTED;

	return spi->set_hold(spi->ifdev, high);
}

ufprog_status UFPROG_API ufprog_spi_set_busy_ind(struct ufprog_spi *spi, ufprog_bool active)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (!spi->set_busy_ind)
		return UFP_UNSUPPORTED;

	return spi->set_busy_ind(spi->ifdev, active);
}

ufprog_status UFPROG_API ufprog_spi_power_control(struct ufprog_spi *spi, ufprog_bool on)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (!spi->power_control) {
		if (on)
			return UFP_OK;
		else
			return UFP_UNSUPPORTED;
	}

	return spi->power_control(spi->ifdev, on);
}

ufprog_status UFPROG_API ufprog_spi_generic_xfer(struct ufprog_spi *spi, const struct ufprog_spi_transfer *xfers,
						 uint32_t count)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (!spi->generic_xfer)
		return UFP_UNSUPPORTED;

	return spi->generic_xfer(spi->ifdev, xfers, count);
}

static ufprog_status ufprog_spi_mem_generic_fill_xfers(struct ufprog_spi *spi, const struct ufprog_spi_mem_op *op,
						       struct ufprog_spi_transfer *xfers, uint32_t *pnxfers,
						       size_t max_buflen, void *buf, size_t *pbuflen,
						       size_t *pdatalen, bool merge_tx_data)
{
	uint32_t i, nxfers = 0, dummy_len;
	size_t datalen = 0, buflen = 0;
	uint8_t *pbuf = buf, bw = 0;
	bool new_xfer = true;
	signed char dtr = -1;

	if (op->cmd.len) {
		if ((dtr >= 0 && dtr != op->cmd.dtr) || (bw && bw != op->cmd.buswidth))
			new_xfer = true;

		dtr = op->cmd.dtr;
		bw = op->cmd.buswidth;

		if (new_xfer) {
			nxfers++;

			if (xfers) {
				xfers[nxfers - 1].buf.tx = pbuf;
				xfers[nxfers - 1].buswidth = bw;
				xfers[nxfers - 1].dtr = dtr;
				xfers[nxfers - 1].dir = SPI_DATA_OUT;
				xfers[nxfers - 1].len = op->cmd.len;
			}
		} else {
			if (xfers)
				xfers[nxfers - 1].len += op->cmd.len;
		}


		if (pbuf)
			*pbuf++ = op->cmd.opcode & 0xff;
		buflen++;

		if (op->cmd.len > 1) {
			if (pbuf)
				*pbuf++ = (op->cmd.opcode >> 8) & 0xff;
			buflen++;
		}

		new_xfer = false;
	}

	if (op->addr.len) {
		if ((dtr >= 0 && dtr != op->addr.dtr) || (bw && bw != op->addr.buswidth))
			new_xfer = true;

		dtr = op->addr.dtr;
		bw = op->addr.buswidth;

		if (new_xfer) {
			nxfers++;

			if (xfers) {
				xfers[nxfers - 1].buf.tx = pbuf;
				xfers[nxfers - 1].buswidth = bw;
				xfers[nxfers - 1].dtr = dtr;
				xfers[nxfers - 1].dir = SPI_DATA_OUT;
				xfers[nxfers - 1].len = op->addr.len;
			}
		} else {
			if (xfers)
				xfers[nxfers - 1].len += op->addr.len;
		}

		if (pbuf) {
			for (i = 0; i < op->addr.len; i++)
				*pbuf++ = (op->addr.val >> (8 * (op->addr.len - i - 1))) & 0xff;
		}

		buflen += op->addr.len;

		new_xfer = false;
	}

	if (op->dummy.len) {
		if ((dtr >= 0 && dtr != op->dummy.dtr) || (bw && bw != op->dummy.buswidth))
			new_xfer = true;

		dtr = op->dummy.dtr;
		bw = op->dummy.buswidth;
		dummy_len = (uint32_t)op->dummy.len * (dtr ? 2 : 1);

		if (new_xfer) {
			nxfers++;

			if (xfers) {
				xfers[nxfers - 1].buf.tx = pbuf;
				xfers[nxfers - 1].buswidth = bw;
				xfers[nxfers - 1].dtr = dtr;
				xfers[nxfers - 1].dir = SPI_DATA_OUT;
				xfers[nxfers - 1].len = dummy_len;
			}
		} else {
			if (xfers)
				xfers[nxfers - 1].len += op->dummy.len;
		}

		if (pbuf) {
			memset(pbuf, 0xff, dummy_len);
			pbuf += dummy_len;
		}
		buflen += dummy_len;

		new_xfer = false;
	}

	if (buflen > spi->generic_xfer_max_size || buflen > max_buflen)
		return UFP_UNSUPPORTED;

	if (op->data.len) {
		if ((dtr >= 0 && dtr != op->data.dtr) || (bw && bw != op->data.buswidth))
			new_xfer = true;

		dtr = op->data.dtr;
		bw = op->data.buswidth;

		if (op->data.len > spi->generic_xfer_max_size)
			datalen = spi->generic_xfer_max_size;
		else
			datalen = op->data.len;

		if (op->data.dir != SPI_DATA_OUT) {
			nxfers++;

			if (xfers) {
				xfers[nxfers - 1].buf.rx = op->data.buf.rx;
				xfers[nxfers - 1].buswidth = bw;
				xfers[nxfers - 1].dtr = dtr;
				xfers[nxfers - 1].dir = SPI_DATA_IN;
				xfers[nxfers - 1].len = datalen;
			}
		} else {
			if (buflen == spi->generic_xfer_max_size || buflen == max_buflen)
				new_xfer = true;

			if (max_buflen - buflen < spi->generic_xfer_max_size / 2)
				new_xfer = true;

			if (merge_tx_data && !new_xfer) {
				if (datalen > spi->generic_xfer_max_size - buflen)
					datalen = spi->generic_xfer_max_size - buflen;

				if (xfers)
					xfers[nxfers - 1].len += datalen;

				if (pbuf)
					memcpy(pbuf, op->data.buf.tx, datalen);
				buflen += datalen;
			} else {
				nxfers++;

				if (xfers) {
					xfers[nxfers - 1].buf.tx = op->data.buf.tx;
					xfers[nxfers - 1].buswidth = bw;
					xfers[nxfers - 1].dtr = dtr;
					xfers[nxfers - 1].dir = SPI_DATA_OUT;
					xfers[nxfers - 1].len = datalen;
				}
			}
		}
	}

	if (xfers)
		xfers[nxfers - 1].end = true;

	if (pdatalen)
		*pdatalen = datalen;

	if (pbuflen)
		*pbuflen = buflen;

	if (pnxfers)
		*pnxfers = nxfers;

	return UFP_OK;
}

static ufprog_status ufprog_spi_mem_generic_adjust_op_size(struct ufprog_spi *spi, struct ufprog_spi_mem_op *op)
{
	ufprog_status ret;
	size_t datalen;

	if (!spi->generic_xfer)
		return UFP_UNSUPPORTED;

	ret = ufprog_spi_mem_generic_fill_xfers(spi, op, NULL, NULL, spi->xfer_buffer_len, NULL, NULL, &datalen, true);
	if (ret) {
		STATUS_CHECK_RET(ufprog_spi_mem_generic_fill_xfers(spi, op, NULL, NULL, spi->xfer_buffer_len, NULL,
								   NULL, &datalen, false));
	}

	op->data.len = datalen;
	return UFP_OK;
}

static ufprog_bool ufprog_spi_mem_generic_supports_op(struct ufprog_spi *spi, const struct ufprog_spi_mem_op *op)
{
	ufprog_status ret;
	bool dtr = false;
	uint32_t bw = 0;

	if (!spi->generic_xfer)
		return UFP_UNSUPPORTED;

	ret = ufprog_spi_mem_generic_fill_xfers(spi, op, NULL, NULL, spi->xfer_buffer_len, NULL, NULL, NULL, true);
	if (ret) {
		if (!ufprog_spi_mem_generic_fill_xfers(spi, op, NULL, NULL, spi->xfer_buffer_len, NULL, NULL, NULL,
						       false))
			return false;
	}

	if (op->cmd.len) {
		if (op->cmd.dtr)
			dtr = true;

		if (op->cmd.buswidth > bw)
			bw = op->cmd.buswidth;
	}

	if (op->addr.len) {
		if (op->addr.dtr)
			dtr = true;

		if (op->addr.buswidth > bw)
			bw = op->addr.buswidth;
	}

	if (op->dummy.len) {
		if (op->dummy.dtr)
			dtr = true;

		if (op->dummy.buswidth > bw)
			bw = op->dummy.buswidth;
	}

	if (op->data.len) {
		if (op->data.dtr)
			dtr = true;

		if (op->data.buswidth > bw)
			bw = op->data.buswidth;
	}

	if (dtr && !(spi->caps & UFP_SPI_GEN_DTR))
		return false;

	switch (bw) {
	case 1:
		break;

	case 2:
		if (!(spi->caps & UFP_SPI_GEN_DUAL))
			return false;
		break;

	case 4:
		if (!(spi->caps & UFP_SPI_GEN_QUAD))
			return false;
		break;

	case 8:
		if (!(spi->caps & UFP_SPI_GEN_OCTAL))
			return false;
		break;

	default:
		return false;
	}

	return true;
}

static ufprog_status ufprog_spi_mem_generic_exec_op(struct ufprog_spi *spi, const struct ufprog_spi_mem_op *op)
{
	struct ufprog_spi_transfer xfers[4];
	ufprog_status ret = UFP_UNSUPPORTED;
	bool try_tx_merge = true;
	uint32_t nxfers = 0;
	size_t buflen = 0;

	if (!spi->generic_xfer)
		return UFP_UNSUPPORTED;

	if (op->cmd.len)
		buflen += op->cmd.len;

	if (op->addr.len)
		buflen += op->addr.len;

	if (op->dummy.len)
		buflen += (uint32_t)op->dummy.len * (op->dummy.dtr ? 2 : 1);

	if (op->data.len) {
		if (op->data.dir == SPI_DATA_OUT)
			buflen += op->data.len;
	}

	if (buflen > spi->xfer_buffer_len)
		try_tx_merge = false;

	if (try_tx_merge) {
		memset(xfers, 0, sizeof(xfers));

		ret = ufprog_spi_mem_generic_fill_xfers(spi, op, xfers, &nxfers, spi->xfer_buffer_len,
							spi->xfer_buffer, &buflen, NULL, true);
	}

	if (ret) {
		memset(xfers, 0, sizeof(xfers));

		ret = ufprog_spi_mem_generic_fill_xfers(spi, op, xfers, &nxfers, spi->xfer_buffer_len,
							spi->xfer_buffer, &buflen, NULL, false);
		if (ret) {
			logm_err("spi-mem operations can not be satisfied\n");
			return ret;
		}
	}

	return spi->generic_xfer(spi->ifdev, xfers, nxfers);
}

static ufprog_status ufprog_spi_mem_generic_poll_status(struct ufprog_spi *spi, const struct ufprog_spi_mem_op *op,
							uint16_t mask, uint16_t match, uint32_t initial_delay_us,
							uint32_t polling_rate_us, uint32_t timeout_ms)
{
	uint64_t end_us;
	uint8_t *buf;
	uint16_t val;

	if (op->data.len < 1 || op->data.len > 2 || op->data.dir != SPI_DATA_IN)
		return UFP_UNSUPPORTED;

	if (!ufprog_spi_mem_supports_op(spi, op))
		return UFP_UNSUPPORTED;

	os_udelay(initial_delay_us);

	buf = op->data.buf.rx;
	end_us = os_get_timer_us() + (uint64_t)timeout_ms * 1000;

	do {
		STATUS_CHECK_RET(ufprog_spi_mem_exec_op(spi, op));

		if (op->data.len == 2)
			val = ((uint16_t)buf[0] << 8) | buf[1];
		else
			val = buf[0];

		if ((val & mask) == match)
			return UFP_OK;

		if (polling_rate_us)
			os_udelay(polling_rate_us);
	} while (os_get_timer_us() <= end_us);

	return UFP_TIMEOUT;
}

ufprog_status UFPROG_API ufprog_spi_mem_adjust_op_size(struct ufprog_spi *spi, struct ufprog_spi_mem_op *op)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (spi->adjust_op_size)
		return spi->adjust_op_size(spi->ifdev, op);

	return ufprog_spi_mem_generic_adjust_op_size(spi, op);
}

ufprog_bool UFPROG_API ufprog_spi_mem_supports_op(struct ufprog_spi *spi, const struct ufprog_spi_mem_op *op)
{
	if (!spi)
		return false;

	if (spi->supports_op)
		return spi->supports_op(spi->ifdev, op);

	return ufprog_spi_mem_generic_supports_op(spi, op);
}

ufprog_status UFPROG_API ufprog_spi_mem_exec_op(struct ufprog_spi *spi, const struct ufprog_spi_mem_op *op)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (spi->exec_op)
		return spi->exec_op(spi->ifdev, op);

	return ufprog_spi_mem_generic_exec_op(spi, op);
}

ufprog_status UFPROG_API ufprog_spi_mem_poll_status(struct ufprog_spi *spi, const struct ufprog_spi_mem_op *op,
						    uint16_t mask, uint16_t match, uint32_t initial_delay_us,
						    uint32_t polling_rate_us, uint32_t timeout_ms)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (spi->poll_status)
		return spi->poll_status(spi->ifdev, op, mask, match, initial_delay_us, polling_rate_us, timeout_ms);

	return ufprog_spi_mem_generic_poll_status(spi, op, mask, match, initial_delay_us, polling_rate_us, timeout_ms);
}

ufprog_bool UFPROG_API ufprog_spi_supports_drive_4io_ones(struct ufprog_spi *spi)
{
	if (!spi)
		return false;

	return !!(spi->drive_4io_ones);
}

ufprog_status UFPROG_API ufprog_spi_drive_4io_ones(struct ufprog_spi *spi, uint32_t clocks)
{
	if (!spi)
		return UFP_INVALID_PARAMETER;

	if (!spi->drive_4io_ones)
		return UFP_UNSUPPORTED;

	return spi->drive_4io_ones(spi->ifdev, clocks);
}

uint32_t UFPROG_API ufprog_spi_mem_io_bus_width_info(uint32_t /* enum spi_mem_io_type */ io_type)
{
	if (io_type >= ARRAY_SIZE(spi_mem_io_bus_width_info))
		return 0;

	return spi_mem_io_bus_width_info[io_type];
}

const char *UFPROG_API ufprog_spi_mem_io_name(uint32_t /* enum spi_mem_io_type */ io_type)
{
	if (io_type >= ARRAY_SIZE(spi_mem_io_name))
		return NULL;

	return spi_mem_io_name[io_type];
}

uint32_t /* enum spi_mem_io_type */ UFPROG_API ufprog_spi_mem_io_name_to_type(const char *name)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(spi_mem_io_name); i++) {
		if (!strcasecmp(spi_mem_io_name[i], name))
			return i;
	}

	return __SPI_MEM_IO_MAX;
}
