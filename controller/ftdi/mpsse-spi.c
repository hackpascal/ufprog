/* SPDX-License-Identifier: LGPL-2.1-only */
/*
* Author: Weijie Gao <hackpascal@gmail.com>
*
* SPI master interface driver for FTDI MPSSE chips
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ufprog/api_spi.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include "mpsse.h"

#define MPSSE_SPI_IF_MAJOR			1
#define MPSSE_SPI_IF_MINOR			0

static ufprog_status mpsse_spi_stop(struct ufprog_if_dev *ftdev);

ufprog_status mpsse_spi_init(struct ufprog_if_dev *ftdev, struct json_object *config)
{
	uint32_t gpio_mask = 0;

	if (json_node_exists(config, "chip-select")) {
		if (!json_read_uint32(config, "chip-select", &ftdev->spi.cs_pin, 0)) {
			if (ftdev->spi.cs_pin >= ftdev->max_gpios || ftdev->spi.cs_pin < GPIO_CS) {
				logm_err("Invalid chip select pin in configuration\n");
				return UFP_DEVICE_INVALID_CONFIG;
			}

			if (gpio_mask & (1 << ftdev->spi.cs_pin)) {
				logm_err("GPIO of chip select pin is occupied\n");
				return UFP_DEVICE_INVALID_CONFIG;
			}

			gpio_mask |= 1 << ftdev->spi.cs_pin;
		}
	} else {
		ftdev->spi.cs_pin = GPIO_CS;
	}

	if (json_node_exists(config, "busy-led")) {
		if (!json_read_uint32(config, "busy-led", &ftdev->spi.busy_led_pin, 0)) {
			if (ftdev->spi.busy_led_pin >= ftdev->max_gpios || ftdev->spi.busy_led_pin < GPIO_CS) {
				logm_err("Invalid chip select in configuration\n");
				return UFP_DEVICE_INVALID_CONFIG;
			}

			if (gpio_mask & (1 << ftdev->spi.busy_led_pin)) {
				logm_err("GPIO of busy LED is occupied\n");
				return UFP_DEVICE_INVALID_CONFIG;
			}

			gpio_mask |= 1 << ftdev->spi.busy_led_pin;
		}
	}

	if (json_node_exists(config, "wp-pin")) {
		if (!json_read_uint32(config, "wp-pin", &ftdev->spi.wp_pin, 0)) {
			if (ftdev->spi.wp_pin >= ftdev->max_gpios || ftdev->spi.wp_pin < GPIO_CS) {
				logm_err("Invalid WP# pin in configuration\n");
				return UFP_DEVICE_INVALID_CONFIG;
			}

			if (gpio_mask & (1 << ftdev->spi.wp_pin)) {
				logm_err("GPIO of WP# pin is occupied\n");
				return UFP_DEVICE_INVALID_CONFIG;
			}

			gpio_mask |= 1 << ftdev->spi.wp_pin;
		}
	}

	if (json_node_exists(config, "hold-pin")) {
		if (!json_read_uint32(config, "hold-pin", &ftdev->spi.hold_pin, 0)) {
			if (ftdev->spi.hold_pin >= ftdev->max_gpios || ftdev->spi.hold_pin < GPIO_CS) {
				logm_err("Invalid HOLD# pin in configuration\n");
				return UFP_DEVICE_INVALID_CONFIG;
			}

			if (gpio_mask & (1 << ftdev->spi.hold_pin)) {
				logm_err("GPIO of HOLD# pin is occupied\n");
				return UFP_DEVICE_INVALID_CONFIG;
			}

			gpio_mask |= 1 << ftdev->spi.hold_pin;
		}
	}

	json_read_bool(config, "busy-led-active-low", &ftdev->spi.busy_led_active_low);

	if (ftdev->spi.busy_led_pin)
		mpsse_set_gpio_output(ftdev, (uint8_t)ftdev->spi.busy_led_pin, ftdev->spi.busy_led_active_low);

	STATUS_CHECK_RET(mpsse_set_gpio(ftdev,
					MPSSE_PIN(GPIO_SK) | MPSSE_PIN(GPIO_MISO) | MPSSE_PIN(GPIO_MOSI) |
					MPSSE_PIN(ftdev->spi.cs_pin),
					MPSSE_PIN(GPIO_SK) | MPSSE_PIN(GPIO_MOSI) | MPSSE_PIN(ftdev->spi.cs_pin),
					MPSSE_PIN(ftdev->spi.cs_pin)));

	return mpsse_spi_stop(ftdev);
}

uint32_t UFPROG_API ufprog_spi_if_version(void)
{
	return MAKE_VERSION(MPSSE_SPI_IF_MAJOR, MPSSE_SPI_IF_MINOR);
}

uint32_t UFPROG_API ufprog_spi_if_caps(void)
{
	return 0;
}

size_t UFPROG_API ufprog_spi_max_read_granularity(void)
{
	return MPSSE_DATA_SHIFTING_MAX_LEN;
}

ufprog_status UFPROG_API ufprog_spi_set_cs_pol(struct ufprog_if_dev *ftdev, ufprog_bool positive)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	ftdev->spi.cs_active_high = positive;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_set_mode(struct ufprog_if_dev *ftdev, uint32_t mode)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	ftdev->spi.mode = mode;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_set_speed(struct ufprog_if_dev *ftdev, uint32_t hz, uint32_t *rethz)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	return mpsse_set_clock(ftdev, hz, rethz);
}

uint32_t UFPROG_API ufprog_spi_get_speed(struct ufprog_if_dev *ftdev)
{
	uint32_t freq;

	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	mpsse_get_clock(ftdev, &freq);

	return freq;
}

uint32_t UFPROG_API ufprog_spi_get_speed_range(struct ufprog_if_dev *ftdev, uint32_t *retlowhz, uint32_t *rethighhz)
{
	uint32_t baseclk_high, baseclk_low;

	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (ftdev->chip != FT2232C) {
		baseclk_high = MPSSE_BASE_CLK_60M;
		baseclk_low = MPSSE_BASE_CLK_12M;
	} else {
		baseclk_high = baseclk_low = MPSSE_BASE_CLK_12M;
	}

	*rethighhz = baseclk_high / 2;
	*retlowhz = baseclk_high / (MPSSE_MAX_CLK_DIV + 1) / 2;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_set_wp(struct ufprog_if_dev *ftdev, ufprog_bool high)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (ftdev->spi.wp_pin)
		return mpsse_set_gpio_output(ftdev, (uint8_t)ftdev->spi.wp_pin, high);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_set_hold(struct ufprog_if_dev *ftdev, ufprog_bool high)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (ftdev->spi.hold_pin)
		return mpsse_set_gpio_output(ftdev, (uint8_t)ftdev->spi.hold_pin, high);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_set_busy_ind(struct ufprog_if_dev *ftdev, ufprog_bool active)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (ftdev->spi.busy_led_pin)
		return mpsse_set_gpio_output(ftdev, (uint8_t)ftdev->spi.busy_led_pin,
					     active ? !ftdev->spi.busy_led_active_low : ftdev->spi.busy_led_active_low);

	return UFP_OK;
}

static ufprog_status mpsse_spi_start(struct ufprog_if_dev *ftdev)
{
	uint16_t pins_val = 0;

	if (ftdev->spi.mode & SPI_MODE_CPOL)
		pins_val |= MPSSE_PIN(GPIO_SK);

	if (!ftdev->spi.cs_active_high)
		pins_val |= MPSSE_PIN(ftdev->spi.cs_pin);

	STATUS_CHECK_RET(mpsse_set_gpio(ftdev, MPSSE_PIN(GPIO_SK) | MPSSE_PIN(ftdev->spi.cs_pin),
					MPSSE_PIN(GPIO_SK) | MPSSE_PIN(ftdev->spi.cs_pin), pins_val));

	if (ftdev->spi.cs_active_high)
		pins_val |= MPSSE_PIN(ftdev->spi.cs_pin);
	else
		pins_val &= ~MPSSE_PIN(ftdev->spi.cs_pin);

	STATUS_CHECK_RET(mpsse_set_gpio(ftdev, MPSSE_PIN(ftdev->spi.cs_pin), MPSSE_PIN(ftdev->spi.cs_pin), pins_val));

	if (ftdev->spi.mode & SPI_MODE_CPHA) {
		if (ftdev->spi.mode & SPI_MODE_CPOL)
			pins_val &= ~MPSSE_PIN(GPIO_SK);
		else
			pins_val |= MPSSE_PIN(GPIO_SK);

		STATUS_CHECK_RET(mpsse_set_gpio(ftdev, MPSSE_PIN(GPIO_SK), MPSSE_PIN(GPIO_SK), pins_val));
	}

	return UFP_OK;
}

static ufprog_status mpsse_spi_stop(struct ufprog_if_dev *ftdev)
{
	uint16_t pins_val = 0;

	if (ftdev->spi.mode & SPI_MODE_CPHA) {
		if (ftdev->spi.mode & SPI_MODE_CPOL)
			pins_val |= MPSSE_PIN(GPIO_SK);
		else
			pins_val &= ~MPSSE_PIN(GPIO_SK);

		STATUS_CHECK_RET(mpsse_set_gpio(ftdev, MPSSE_PIN(GPIO_SK), MPSSE_PIN(GPIO_SK), pins_val));
	}

	if (ftdev->spi.cs_active_high)
		pins_val &= ~MPSSE_PIN(ftdev->spi.cs_pin);
	else
		pins_val |= MPSSE_PIN(ftdev->spi.cs_pin);

	return mpsse_set_gpio(ftdev, MPSSE_PIN(ftdev->spi.cs_pin), MPSSE_PIN(ftdev->spi.cs_pin), pins_val);
}

static ufprog_status mpsse_spi_read_once(struct ufprog_if_dev *ftdev, uint8_t *buf, uint32_t len)
{
	uint32_t rlen = len - 1;
	uint8_t cmd[3];

	cmd[0] = MPSSE_DO_READ;

	if (ftdev->spi.mode == SPI_MODE_1 || ftdev->spi.mode == SPI_MODE_2)
		cmd[0] |= MPSSE_READ_NEG;

	cmd[1] = rlen & 0xff;
	cmd[2] = (rlen >> 8) & 0xff;

	STATUS_CHECK_RET(ftdi_write(ftdev->handle, cmd, sizeof(cmd)));

	return ftdi_read(ftdev->handle, buf, len);
}

static ufprog_status mpsse_spi_write_once(struct ufprog_if_dev *ftdev, const uint8_t *buf, uint32_t len)
{
	uint8_t *cmd = ftdev->scratch_buffer;
	uint32_t wlen = len - 1;

	cmd[0] = MPSSE_DO_WRITE;

	if (ftdev->spi.mode == SPI_MODE_0 || ftdev->spi.mode == SPI_MODE_3)
		cmd[0] |= MPSSE_WRITE_NEG;

	cmd[1] = wlen & 0xff;
	cmd[2] = (wlen >> 8) & 0xff;

	memcpy(cmd + 3, buf, len);

	return ftdi_write(ftdev->handle, cmd, MPSSE_DATA_SHIFTING_CMD_LEN + len);
}

static ufprog_status mpsse_spi_read(struct ufprog_if_dev *ftdev, void *buf, size_t len)
{
	uint8_t *p = buf;
	size_t chksz;

	while (len) {
		if (len > MPSSE_DATA_SHIFTING_MAX_LEN)
			chksz = MPSSE_DATA_SHIFTING_MAX_LEN;
		else
			chksz = len;

		STATUS_CHECK_RET(mpsse_spi_read_once(ftdev, p, (uint32_t)chksz));

		len -= chksz;
		p += chksz;
	}

	return UFP_OK;
}

static ufprog_status mpsse_spi_write(struct ufprog_if_dev *ftdev, const void *buf, size_t len)
{
	const uint8_t *p = buf;
	size_t chksz;

	while (len) {
		if (len > MPSSE_DATA_SHIFTING_MAX_LEN)
			chksz = MPSSE_DATA_SHIFTING_MAX_LEN;
		else
			chksz = len;

		STATUS_CHECK_RET(mpsse_spi_write_once(ftdev, p, (uint32_t)chksz));

		len -= chksz;
		p += chksz;
	}

	return UFP_OK;
}

static ufprog_status mpsse_spi_generic_xfer_one(struct ufprog_if_dev *ftdev, const struct ufprog_spi_transfer *xfer)
{
	if (xfer->buswidth > 1 || xfer->dtr) {
		logm_err("Only single I/O single rate is supported\n");
		return UFP_UNSUPPORTED;
	}

	if (xfer->speed)
		STATUS_CHECK_RET(mpsse_set_clock(ftdev, xfer->speed, NULL));

	if (xfer->dir == SPI_DATA_IN)
		return mpsse_spi_read(ftdev, xfer->buf.rx, xfer->len);

	return mpsse_spi_write(ftdev, xfer->buf.tx, xfer->len);
}

ufprog_status UFPROG_API ufprog_spi_generic_xfer(struct ufprog_if_dev *ftdev, const struct ufprog_spi_transfer *xfers,
						 uint32_t count)
{
	bool require_spi_start = true;
	ufprog_status ret = UFP_OK;
	uint32_t i;

	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	os_mutex_lock(ftdev->lock);

	for (i = 0; i < count; i++) {
		if (require_spi_start) {
			STATUS_CHECK_GOTO_RET(mpsse_spi_start(ftdev), ret, out);
			require_spi_start = false;
		}

		ret = mpsse_spi_generic_xfer_one(ftdev, &xfers[i]);
		if (ret) {
			mpsse_spi_stop(ftdev);
			goto out;
		}

		if (xfers[i].end) {
			STATUS_CHECK_GOTO_RET(mpsse_spi_stop(ftdev), ret, out);
			require_spi_start = true;
		}
	}

out:
	os_mutex_unlock(ftdev->lock);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_drive_4io_ones(struct ufprog_if_dev *ftdev, uint32_t clocks)
{
	uint16_t mask = MPSSE_PIN(GPIO_MOSI) | MPSSE_PIN(GPIO_MISO), clk_mask = 0;
	ufprog_status ret = UFP_OK;
	uint32_t i;

	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (!clocks)
		return UFP_OK;

	if (ftdev->spi.mode == SPI_MODE_0 || ftdev->spi.mode == SPI_MODE_3)
		clk_mask = MPSSE_PIN(GPIO_SK);

	os_mutex_lock(ftdev->lock);

	STATUS_CHECK_GOTO_RET(mpsse_spi_start(ftdev), ret, out);

	if (ftdev->spi.wp_pin)
		mask |= MPSSE_PIN(ftdev->spi.wp_pin);

	if (ftdev->spi.hold_pin)
		mask |= MPSSE_PIN(ftdev->spi.hold_pin);

	STATUS_CHECK_GOTO_RET(mpsse_set_gpio(ftdev, mask, mask, mask), ret, out);

	for (i = 0; i < clocks; i++) {
		STATUS_CHECK_GOTO_RET(mpsse_set_gpio(ftdev, MPSSE_PIN(GPIO_SK), MPSSE_PIN(GPIO_SK), clk_mask), ret,
						     out);
		STATUS_CHECK_GOTO_RET(mpsse_set_gpio(ftdev, MPSSE_PIN(GPIO_SK), MPSSE_PIN(GPIO_SK),
						     clk_mask ^ MPSSE_PIN(GPIO_SK)), ret, out);
	}

	STATUS_CHECK_GOTO_RET(mpsse_spi_stop(ftdev), ret, out);

out:
	os_mutex_unlock(ftdev->lock);

	return ret;
}
