/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Common definitions for FTDI MPSSE chips
 */
#pragma once

#ifndef _UFPROG_MPSSE_H_
#define _UFPROG_MPSSE_H_

#include <stdint.h>
#include <stdbool.h>
#include <ufprog/osdef.h>
#include <ufprog/config.h>
#include "ftdi.h"

#define MPSSE_DATA_SHIFTING_CMD_LEN			3
#define MPSSE_DATA_SHIFTING_MAX_LEN			0x10000

#define MPSSE_BUF_LEN					(MPSSE_DATA_SHIFTING_CMD_LEN + MPSSE_DATA_SHIFTING_MAX_LEN)

#define MPSSE_BASE_CLK_12M				12000000
#define MPSSE_BASE_CLK_60M				60000000
#define MPSSE_MAX_CLK_DIV				0xffff

#define MPSSE_INIT_CLK					6000000

/* MPSSE basic commands */
#define MPSSE_CMD_SET_BITS_LOW				0x80
#define MPSSE_CMD_READ_BITS_LOW				0x81
#define MPSSE_CMD_SET_BITS_HIGH				0x82
#define MPSSE_CMD_READ_BITS_HIGH			0x83
#define MPSSE_CMD_LOOPBACK_EN				0x84
#define MPSSE_CMD_LOOPBACK_DIS				0x85
#define MPSSE_CMD_TCK_DIVISOR				0x86
#define MPSSE_CMD_TCK_D5_DIS				0x8A
#define MPSSE_CMD_TCK_D5_EN				0x8B
#define MPSSE_CMD_3PHASE_EN				0x8C
#define MPSSE_CMD_3PHASE_DIS				0x8D
#define MPSSE_CMD_ADAPTIVE_CLK_EN			0x96
#define MPSSE_CMD_ADAPTIVE_CLK_DIS			0x97

/* MPSSE data shifting commands */
#define MPSSE_WRITE_NEG					0x01
#define MPSSE_BITMODE					0x02
#define MPSSE_READ_NEG					0x04
#define MPSSE_LSB					0x08
#define MPSSE_DO_WRITE					0x10
#define MPSSE_DO_READ					0x20
#define MPSSE_WRITE_TMS					0x40

enum mpsse_gpio {
	GPIO_SK,
	GPIO_MOSI,
	GPIO_MISO,
	GPIO_CS,
	GPIO_L0,
	GPIO_L1,
	GPIO_L2,
	GPIO_L3,
	GPIO_H0,
	GPIO_H1,
	GPIO_H2,
	GPIO_H3,
	GPIO_H4,
	GPIO_H5,
	GPIO_H6,
	GPIO_H7,
};

#define MPSSE_PIN(_p)					BIT(_p)

struct mpsse_spi_info {
	uint32_t mode;
	ufprog_bool cs_active_high;

	uint32_t cs_pin;
	uint32_t wp_pin;
	uint32_t hold_pin;

	uint32_t busy_led_pin;
	ufprog_bool busy_led_active_low;
};

struct ufprog_interface {
	struct ft_handle *handle;
	enum ftdi_mpsse_chip chip;

	uint8_t *scratch_buffer;
	uint8_t max_gpios;

	uint16_t gpio_dir;
	uint16_t gpio_out_val;

	ufprog_bool three_phase;
	bool clock_d5;
	uint16_t clock_div;

	struct mpsse_spi_info spi;

	mutex_handle lock;
};

ufprog_status mpsse_init(struct ufprog_interface *ftdev, bool thread_safe);
ufprog_status mpsse_cleanup(struct ufprog_interface *ftdev);

ufprog_status mpsse_control_loopback(struct ufprog_interface *ftdev, bool enable);
ufprog_status mpsse_control_adaptive_clock(struct ufprog_interface *ftdev, bool enable);
ufprog_status mpsse_control_3phase_clock(struct ufprog_interface *ftdev, bool enable);
ufprog_status mpsse_control_clock_d5(struct ufprog_interface *ftdev, bool enable);

ufprog_status mpsse_set_gpio(struct ufprog_interface *ftdev, uint16_t mask, uint16_t dir, uint16_t val);
ufprog_status mpsse_set_gpio_input(struct ufprog_interface *ftdev, uint8_t gpio);
ufprog_status mpsse_set_gpio_output(struct ufprog_interface *ftdev, uint8_t gpio, int value);
ufprog_status mpsse_get_gpio(struct ufprog_interface *ftdev, uint16_t mask, uint16_t *val);
ufprog_status mpsse_get_gpio_value(struct ufprog_interface *ftdev, uint8_t gpio, int *val);

ufprog_status mpsse_set_clock(struct ufprog_interface *ftdev, uint32_t freq, uint32_t *retfreq);
ufprog_status mpsse_get_clock(struct ufprog_interface *ftdev, uint32_t *retfreq);

ufprog_status mpsse_spi_init(struct ufprog_interface *ftdev, struct json_object *config);

#endif /* _UFPROG_MPSSE_H_ */
