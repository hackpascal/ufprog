/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI abstraction layer
 */
#pragma once

#ifndef _UFPROG_SPI_H_
#define _UFPROG_SPI_H_

#include <ufprog/bits.h>
#include <ufprog/device.h>
#include <ufprog/api_spi.h>

EXTERN_C_BEGIN

struct ufprog_spi;

ufprog_status UFPROG_API ufprog_spi_attach_device(struct ufprog_device *ifdev, struct ufprog_spi **outspi);
ufprog_status UFPROG_API ufprog_spi_open_device(const char *name, ufprog_bool thread_safe, struct ufprog_spi **outspi);
struct ufprog_device *UFPROG_API ufprog_spi_get_device(struct ufprog_spi *spi);
ufprog_status UFPROG_API ufprog_spi_close_device(struct ufprog_spi *spi);

uint32_t UFPROG_API ufprog_spi_if_caps(struct ufprog_spi *spi);
size_t UFPROG_API ufprog_spi_max_read_granularity(struct ufprog_spi *spi);

ufprog_status UFPROG_API ufprog_spi_bus_lock(struct ufprog_spi *spi);
ufprog_status UFPROG_API ufprog_spi_bus_unlock(struct ufprog_spi *spi);

ufprog_status UFPROG_API ufprog_spi_set_cs_pol(struct ufprog_spi *spi, ufprog_bool positive);
ufprog_status UFPROG_API ufprog_spi_set_mode(struct ufprog_spi *spi, uint32_t mode);

ufprog_status UFPROG_API ufprog_spi_set_speed(struct ufprog_spi *spi, uint32_t hz, uint32_t *rethz);
ufprog_status UFPROG_API ufprog_spi_set_speed_closest(struct ufprog_spi *spi, uint32_t freq, uint32_t *retfreq);
uint32_t UFPROG_API ufprog_spi_get_speed(struct ufprog_spi *spi);
ufprog_status UFPROG_API ufprog_spi_get_speed_range(struct ufprog_spi *spi, uint32_t *retlowhz, uint32_t *rethighhz);
uint32_t UFPROG_API ufprog_spi_get_speed_list(struct ufprog_spi *spi, uint32_t *retlist, int32_t count);
ufprog_status UFPROG_API ufprog_spi_get_speed_limit(struct ufprog_spi *spi, uint32_t *retmin, uint32_t *retmax);

ufprog_status UFPROG_API ufprog_spi_set_wp(struct ufprog_spi *spi, ufprog_bool high);
ufprog_status UFPROG_API ufprog_spi_set_hold(struct ufprog_spi *spi, ufprog_bool high);
ufprog_status UFPROG_API ufprog_spi_set_busy_ind(struct ufprog_spi *spi, ufprog_bool active);

ufprog_status UFPROG_API ufprog_spi_power_control(struct ufprog_spi *spi, ufprog_bool on);

ufprog_status UFPROG_API ufprog_spi_generic_xfer(struct ufprog_spi *spi, const struct ufprog_spi_transfer *xfers,
						 uint32_t count);

static inline ufprog_status ufprog_spi_sio_read(struct ufprog_spi *spi, void *data, size_t len)
{
	struct ufprog_spi_transfer xfer = {
		.buf.rx = data,
		.len = len,
		.dir = SPI_DATA_IN,
		.end = 1,
	};

	return ufprog_spi_generic_xfer(spi, &xfer, 1);
}

static inline ufprog_status ufprog_spi_sio_write(struct ufprog_spi *spi, const void *data, size_t len)
{
	struct ufprog_spi_transfer xfer = {
		.buf.tx = data,
		.len = len,
		.dir = SPI_DATA_OUT,
		.end = 1,
	};

	return ufprog_spi_generic_xfer(spi, &xfer, 1);
}

static inline ufprog_status ufprog_spi_sio_write_then_read(struct ufprog_spi *spi, const void *tx_data,
							   size_t tx_len, void *rx_data, size_t rx_len)
{
	struct ufprog_spi_transfer xfer[] = {
		{
			.buf.tx = tx_data,
			.len = tx_len,
			.dir = SPI_DATA_OUT,
		},
		{
			.buf.rx = rx_data,
			.len = rx_len,
			.dir = SPI_DATA_IN,
			.end = 1,
		}
	};

	return ufprog_spi_generic_xfer(spi, xfer, 2);
}

ufprog_status UFPROG_API ufprog_spi_mem_adjust_op_size(struct ufprog_spi *spi, struct ufprog_spi_mem_op *op);
ufprog_bool UFPROG_API ufprog_spi_mem_supports_op(struct ufprog_spi *spi, const struct ufprog_spi_mem_op *op);
ufprog_status UFPROG_API ufprog_spi_mem_exec_op(struct ufprog_spi *spi, const struct ufprog_spi_mem_op *op);
ufprog_status UFPROG_API ufprog_spi_mem_poll_status(struct ufprog_spi *spi, const struct ufprog_spi_mem_op *op,
						    uint16_t mask, uint16_t match, uint32_t initial_delay_us,
						    uint32_t polling_rate_us, uint32_t timeout_ms);

ufprog_bool UFPROG_API ufprog_spi_supports_drive_4io_ones(struct ufprog_spi *spi);
ufprog_status UFPROG_API ufprog_spi_drive_4io_ones(struct ufprog_spi *spi, uint32_t clocks);

uint32_t UFPROG_API ufprog_spi_mem_io_bus_width_info(uint32_t /* enum spi_mem_io_type */ io_type);
const char *UFPROG_API ufprog_spi_mem_io_name(uint32_t /* enum spi_mem_io_type */ io_type);
uint32_t /* enum spi_mem_io_type */ UFPROG_API ufprog_spi_mem_io_name_to_type(const char *name);

#define SPI_MEM_CMD_BW_S				0
#define SPI_MEM_CMD_BW_M				BITS(3, SPI_MEM_CMD_BW_S)
#define SPI_MEM_ADDR_BW_S				4
#define SPI_MEM_ADDR_BW_M				BITS(7, SPI_MEM_ADDR_BW_S)
#define SPI_MEM_DATA_BW_S				8
#define SPI_MEM_DATA_BW_M				BITS(11, SPI_MEM_DATA_BW_S)
#define SPI_MEM_CMD_DTR					BIT(12)
#define SPI_MEM_ADDR_DTR				BIT(13)
#define SPI_MEM_DATA_DTR				BIT(14)


#define DEFINE_SPI_MEM_IO_BW_INFO(_name, _field_name)						\
static inline uint8_t spi_mem_io_info_ ## _name ## _bw(uint32_t info)				\
{												\
	return (uint8_t)FIELD_GET(SPI_MEM_## _field_name ## _BW, info);				\
}												\
												\
static inline uint8_t spi_mem_io_info_## _name ##_dtr(uint32_t info)				\
{												\
	return !!(info & SPI_MEM_## _field_name ##_DTR);					\
}												\
												\
static inline uint8_t spi_mem_io_ ## _name ## _bw(enum spi_mem_io_type io_type)			\
{												\
	if (io_type >= __SPI_MEM_IO_MAX)							\
		return 0;									\
	return spi_mem_io_info_ ## _name ## _bw(ufprog_spi_mem_io_bus_width_info(io_type));	\
}												\
												\
static inline uint8_t spi_mem_io_## _name ##_dtr(enum spi_mem_io_type io_type)			\
{												\
	if (io_type >= __SPI_MEM_IO_MAX)							\
		return 0;									\
	return spi_mem_io_info_## _name ##_dtr(ufprog_spi_mem_io_bus_width_info(io_type));	\
}

DEFINE_SPI_MEM_IO_BW_INFO(cmd, CMD)
DEFINE_SPI_MEM_IO_BW_INFO(addr, ADDR)
DEFINE_SPI_MEM_IO_BW_INFO(data, DATA)

EXTERN_C_END

#endif /* _UFPROG_SPI_H_ */
