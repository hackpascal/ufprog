/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Common definitions for FTDI FT4222H
 */
#pragma once

#ifndef _UFPROG_FT4222H_H_
#define _UFPROG_FT4222H_H_

#include <stdint.h>
#include <stdbool.h>
#include <ufprog/osdef.h>
#include <ufprog/config.h>
#include "ftdi.h"

#define FT4222_MULTIIO_CMD_LEN			5
#define FT4222_MULTIIO_SIO_WR_MAX_LEN		0xf
#define FT4222_MULTIIO_MIO_WR_MAX_LEN		0xffff
#define FT4222_MULTIIO_MIO_RD_MAX_LEN		0xffff
#define FT4222_SINGLEIO_XFER_MAX_LEN		0xffff

#define FT4222_MULTIIO_BUF_LEN			(FT4222_MULTIIO_CMD_LEN + FT4222_MULTIIO_SIO_WR_MAX_LEN + \
						 FT4222_MULTIIO_MIO_WR_MAX_LEN)

struct ft4222_hwcaps {
	uint8_t chip_mode;
	uint8_t field_1;
	uint8_t field_2;
	uint8_t field_3;
	uint8_t field_4;
	uint8_t clk;
	uint8_t function_mode;
	uint8_t field_7;
	uint8_t suspend_out; /* bool */
	uint8_t wake_up_interrupt; /* bool */
	uint8_t field_A;
	uint8_t field_B;
	uint8_t field_C;
};

struct ft4222_hwver {
	uint8_t chip_model[2];
	uint8_t fwver;
	uint8_t field_3;
	uint8_t field_4;
	uint8_t field_5;
	uint8_t field_6;
	uint8_t field_7;
	uint8_t field_8;
	uint8_t field_9;
	uint8_t field_A;
	uint8_t field_B;
};

enum ft4222_function {
	FT4222_I2C_MASTER = 1,
	FT4222_I2C_SLAVE,
	FT4222_SPI_MASTER,
	FT4222_SPI_SLAVE,
};

enum ft4222_clock {
	SYS_CLK_60 = 0,
	SYS_CLK_24,
	SYS_CLK_48,
	SYS_CLK_80,
};

enum ft4222_spi_clkdiv {
	CLK_NONE = 0,
	CLK_DIV_2,
	CLK_DIV_4,
	CLK_DIV_8,
	CLK_DIV_16,
	CLK_DIV_32,
	CLK_DIV_64,
	CLK_DIV_128,
	CLK_DIV_256,
	CLK_DIV_512,
};

enum ft4222_spi_mode {
	SPI_IO_NONE = 0,
	SPI_IO_SINGLE = 1,
	SPI_IO_DUAL = 2,
	SPI_IO_QUAD = 4,
};

enum ft4222_spi_cpol {
	CLK_IDLE_LOW = 0,
	CLK_IDLE_HIGH = 1,
};

enum ft4222_spi_cpha {
	CLK_LEADING = 0,
	CLK_TRAILING = 1,
};

enum ft4222_spi_pol {
	CS_ACTIVE_NEGTIVE = 0,
	CS_ACTIVE_POSTIVE,
};

enum ft4222_spi_drive_strength {
	DS_4MA = 0,
	DS_8MA,
	DS_12MA,
	DS_16MA,
};

struct ft4222_spi_master_info {
	uint32_t max_cs;
	uint32_t curr_cs;
	enum ft4222_spi_clkdiv clkdiv;
	enum ft4222_spi_mode mode;
	enum ft4222_spi_cpol cpol;
	enum ft4222_spi_cpha cpha;
	enum ft4222_spi_pol cs_pol;
	enum ft4222_spi_drive_strength ds;
};

struct ft4222_spi_clk_info {
	uint32_t freq;
	enum ft4222_clock clk;
	enum ft4222_spi_clkdiv div;
};

struct ufprog_if_dev {
	struct ft_handle *handle;
	struct ft4222_hwcaps hwcaps;
	struct ft4222_hwver hwver;

	uint32_t max_buck_size;
	uint8_t *scratch_buffer;
	struct ft4222_spi_master_info spim;

	mutex_handle lock;
};

extern const uint32_t ft4222_sys_clks[];

ufprog_status ft4222_init(struct ufprog_if_dev *ftdev, bool thread_safe);
ufprog_status ft4222_get_clock(struct ufprog_if_dev *ftdev, enum ft4222_clock *clk);
ufprog_status ft4222_set_clock(struct ufprog_if_dev *ftdev, enum ft4222_clock clk);
ufprog_status ft4222_set_function(struct ufprog_if_dev *ftdev, enum ft4222_function func);

ufprog_status ft4222_spi_master_init(struct ufprog_if_dev *ftdev, struct json_object *config);
ufprog_status ft4222_spi_master_cleanup(struct ufprog_if_dev *ftdev);

#endif /* _UFPROG_FT4222H_H_ */
