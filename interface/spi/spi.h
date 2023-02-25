/* SPDX-License-Identifier: LGPL-2.1-only */
/*
* Author: Weijie Gao <hackpascal@gmail.com>
*
* SPI interface internal definitions
*/
#pragma once

#ifndef _UFPROG_SPI_INTERNAL_H_
#define _UFPROG_SPI_INTERNAL_H_

#include <ufprog/spi.h>

#define UFPROG_SPI_XFER_BUFFER_LEN		0x10000

struct ufprog_spi {
	struct ufprog_device *dev;
	struct ufprog_if_dev *ifdev;

	uint32_t if_ver;
	uint32_t caps;

	api_spi_set_cs_pol set_cs_pol;
	api_spi_set_mode set_mode;

	api_spi_set_speed set_speed;
	api_spi_get_speed get_speed;
	api_spi_get_speed_range get_speed_range;
	api_spi_get_speed_list get_speed_list;

	api_spi_set_wp set_wp;
	api_spi_set_hold set_hold;
	api_spi_set_busy_ind set_busy_ind;

	api_spi_power_control power_control;

	api_spi_mem_adjust_op_size adjust_op_size;
	api_spi_mem_supports_op supports_op;
	api_spi_mem_exec_op exec_op;
	api_spi_mem_poll_status poll_status;

	size_t generic_xfer_max_size;
	api_spi_generic_xfer generic_xfer;

	api_spi_drive_4io_ones drive_4io_ones;

	void *xfer_buffer;
	size_t xfer_buffer_len;

	size_t max_read_granularity;

	uint32_t num_speeds;
	uint32_t speed_max;
	uint32_t speed_min;
	uint32_t *speed_list;
};

#endif /* _UFPROG_SPI_INTERNAL_H_ */
