/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Common implementation for FTDI MPSSE chips
 */

#include <malloc.h>
#include <ufprog/api_controller.h>
#include <ufprog/log.h>
#include "mpsse.h"

#define MPSSE_DRV_VER_MAJOR			1
#define MPSSE_DRV_VER_MINOR			0

static const char *mpsse_chip_models[] = {
	[FT232H] = "FT232H",
	[FT2232C] = "FT2232C",
	[FT2232H] = "FT2232H",
	[FT4232H] = "FT4232H",
};

ufprog_status mpsse_control_loopback(struct ufprog_if_dev *ftdev, bool enable)
{
	uint8_t cmd;

	if (enable)
		cmd = MPSSE_CMD_LOOPBACK_EN;
	else
		cmd = MPSSE_CMD_LOOPBACK_DIS;

	return ftdi_write(ftdev->handle, &cmd, sizeof(cmd));
}

ufprog_status mpsse_control_adaptive_clock(struct ufprog_if_dev *ftdev, bool enable)
{
	uint8_t cmd;

	if (enable)
		cmd = MPSSE_CMD_ADAPTIVE_CLK_EN;
	else
		cmd = MPSSE_CMD_ADAPTIVE_CLK_DIS;

	return ftdi_write(ftdev->handle, &cmd, sizeof(cmd));
}

ufprog_status mpsse_control_3phase_clock(struct ufprog_if_dev *ftdev, bool enable)
{
	uint8_t cmd;

	if (enable)
		cmd = MPSSE_CMD_3PHASE_EN;
	else
		cmd = MPSSE_CMD_3PHASE_DIS;

	return ftdi_write(ftdev->handle, &cmd, sizeof(cmd));
}

ufprog_status mpsse_control_clock_d5(struct ufprog_if_dev *ftdev, bool enable)
{
	uint8_t cmd;

	if (enable)
		cmd = MPSSE_CMD_TCK_D5_EN;
	else
		cmd = MPSSE_CMD_TCK_D5_DIS;

	return ftdi_write(ftdev->handle, &cmd, sizeof(cmd));
}

ufprog_status mpsse_set_gpio(struct ufprog_if_dev *ftdev, uint16_t mask, uint16_t dir, uint16_t val)
{
	ufprog_status retl = UFP_OK, reth = UFP_OK;
	uint16_t new_dir, new_val;
	uint8_t cmd[3];

	mask &= (1 << ftdev->max_gpios) - 1;
	new_dir = (ftdev->gpio_dir & ~mask) | (dir & mask);
	new_val = (ftdev->gpio_out_val & ~mask) | (val & mask);

	os_mutex_lock(ftdev->lock);

	if (((new_dir ^ ftdev->gpio_dir) & 0xff) || ((new_val ^ ftdev->gpio_out_val) & 0xff)) {
		cmd[0] = MPSSE_CMD_SET_BITS_LOW;
		cmd[1] = new_val & 0xff;
		cmd[2] = new_dir & 0xff;

		retl = ftdi_write(ftdev->handle, cmd, sizeof(cmd));
		if (retl)
			logm_err("Failed to set GPIO low bits\n");
	}

	if (((new_dir ^ ftdev->gpio_dir) & 0xff00) || ((new_val ^ ftdev->gpio_out_val) & 0xff00)) {
		cmd[0] = MPSSE_CMD_SET_BITS_HIGH;
		cmd[1] = new_val >> 8;
		cmd[2] = new_dir >> 8;

		reth = ftdi_write(ftdev->handle, cmd, sizeof(cmd));
		if (reth)
			logm_err("Failed to set GPIO high bits\n");
	}

	os_mutex_unlock(ftdev->lock);

	ftdev->gpio_dir = new_dir;
	ftdev->gpio_out_val = new_val;

	if (retl)
		return retl;

	if (reth)
		return reth;

	return UFP_OK;
}

ufprog_status mpsse_set_gpio_input(struct ufprog_if_dev *ftdev, uint8_t gpio)
{
	if (gpio >= ftdev->max_gpios)
		return UFP_UNSUPPORTED;

	return mpsse_set_gpio(ftdev, 1 << gpio, 0, 0);
}

ufprog_status mpsse_set_gpio_output(struct ufprog_if_dev *ftdev, uint8_t gpio, int value)
{
	if (gpio >= ftdev->max_gpios)
		return UFP_UNSUPPORTED;

	return mpsse_set_gpio(ftdev, 1 << gpio, 1 << gpio, (!!value) << gpio);
}

ufprog_status mpsse_get_gpio(struct ufprog_if_dev *ftdev, uint16_t mask, uint16_t *val)
{
	ufprog_status retl = UFP_OK, reth = UFP_OK;
	uint8_t cmd, gpio_val;
	uint16_t all_val = 0;

	mask &= (1 << ftdev->max_gpios) - 1;

	os_mutex_lock(ftdev->lock);

	if (mask & 0xff) {
		cmd = MPSSE_CMD_READ_BITS_LOW;

		retl = ftdi_write(ftdev->handle, &cmd, sizeof(cmd));
		if (!retl) {
			retl = ftdi_read(ftdev->handle, &gpio_val, sizeof(gpio_val));
			if (!retl)
				all_val |= gpio_val;
		}
	}

	if (mask & 0xff00) {
		cmd = MPSSE_CMD_READ_BITS_HIGH;

		retl = ftdi_write(ftdev->handle, &cmd, sizeof(cmd));
		if (!retl) {
			retl = ftdi_read(ftdev->handle, &gpio_val, sizeof(gpio_val));
			if (!retl)
				all_val |= (uint16_t)gpio_val << 8;
		}
	}

	os_mutex_unlock(ftdev->lock);

	*val = all_val & mask;

	if (retl) {
		logm_err("Failed to read GPIO low bits\n");
		return retl;
	}

	if (reth) {
		logm_err("Failed to read GPIO high bits\n");
		return reth;
	}

	return UFP_OK;
}

ufprog_status mpsse_get_gpio_value(struct ufprog_if_dev *ftdev, uint8_t gpio, int *val)
{
	uint16_t gpio_val;

	if (gpio >= ftdev->max_gpios)
		return UFP_UNSUPPORTED;

	STATUS_CHECK_RET(mpsse_get_gpio(ftdev, 1 << gpio, &gpio_val));

	*val = !!gpio_val;

	return UFP_OK;
}

static void mpsse_calc_clock(uint32_t baseclk, uint32_t freq, uint32_t *retfreq, uint16_t *out_div)
{
	uint32_t div;

	if (freq > baseclk / 2) {
		*retfreq = baseclk / 2;
		*out_div = 0;
		return;
	}

	div = (((baseclk / freq) / 2) - 1);
	if (div > MPSSE_MAX_CLK_DIV)
		div = MPSSE_MAX_CLK_DIV;

	*retfreq = baseclk / (div + 1) / 2;
	*out_div = (uint16_t)div;
}

ufprog_status mpsse_set_clock(struct ufprog_if_dev *ftdev, uint32_t freq, uint32_t *retfreq)
{
	uint32_t real_freq_d5, real_freq, diff, diff_d5;
	uint16_t out_div_d5, out_div;
	uint8_t cmd[4], cmdlen;
	ufprog_status ret;

	if (ftdev->three_phase)
		freq = freq * 3 / 2;

	mpsse_calc_clock(MPSSE_BASE_CLK_12M, freq, &real_freq_d5, &out_div_d5);

	if (ftdev->chip != FT2232C) {
		mpsse_calc_clock(MPSSE_BASE_CLK_60M, freq, &real_freq, &out_div);

		diff = freq - real_freq;
		diff_d5 = freq - real_freq_d5;

		if (diff <= diff_d5) {
			cmd[0] = MPSSE_CMD_TCK_D5_DIS;
			cmd[1] = MPSSE_CMD_TCK_DIVISOR;
			cmd[2] = out_div & 0xff;
			cmd[3] = out_div >> 8;

			ftdev->clock_d5 = false;
			ftdev->clock_div = out_div;

			if (retfreq)
				*retfreq = real_freq;
		} else {
			cmd[0] = MPSSE_CMD_TCK_D5_EN;
			cmd[1] = MPSSE_CMD_TCK_DIVISOR;
			cmd[2] = out_div_d5 & 0xff;
			cmd[3] = out_div_d5 >> 8;

			ftdev->clock_d5 = true;
			ftdev->clock_div = out_div_d5;

			if (retfreq)
				*retfreq = real_freq_d5;
		}

		cmdlen = 4;
	} else {
		cmd[0] = MPSSE_CMD_TCK_DIVISOR;
		cmd[1] = out_div_d5 & 0xff;
		cmd[2] = out_div_d5 >> 8;

		ftdev->clock_d5 = false;
		ftdev->clock_div = out_div_d5;

		if (retfreq)
			*retfreq = real_freq_d5;

		cmdlen = 3;
	}

	ret = ftdi_write(ftdev->handle, cmd, cmdlen);
	if (ret)
		logm_err("Failed to set clock divisor\n");

	if (retfreq)
		*retfreq = real_freq_d5;

	return ret;
}

ufprog_status mpsse_get_clock(struct ufprog_if_dev *ftdev, uint32_t *retfreq)
{
	uint32_t baseclk;

	if (ftdev->chip != FT2232C) {
		baseclk = MPSSE_BASE_CLK_60M;
		if (ftdev->clock_d5)
			baseclk /= 5;
	} else {
		baseclk = MPSSE_BASE_CLK_12M;
	}

	*retfreq = baseclk / (ftdev->clock_div + 1) / 2;

	return UFP_OK;
}

ufprog_status mpsse_init(struct ufprog_if_dev *ftdev, bool thread_safe)
{
	ufprog_status ret;
	uint8_t cmd[3];

	if (thread_safe) {
		if (!os_create_mutex(&ftdev->lock)) {
			logm_err("Failed to create lock for thread-safe");
			return UFP_LOCK_FAIL;
		}
	}

	logm_info("Chip is %s\n", mpsse_chip_models[ftdev->chip]);

	switch (ftdev->chip) {
	case FT232H:
	case FT2232C:
	case FT2232H:
		ftdev->max_gpios = 16;
		break;

	case FT4232H:
		ftdev->max_gpios = 8;
		break;

	default:
		; /* Not able to go there */
	}

	ftdi_reset(ftdev->handle);

	ret = ftdi_set_latency_timer(ftdev->handle, 2);
	if (ret)
		logm_warn("Failed to set latency timer\n");

	/* Reset bitmode */
	ret = ftdi_set_bit_mode(ftdev->handle, 0, FTDI_BITMODE_RESET);
	if (ret) {
		logm_err("Failed to reset bitmode\n");
		return ret;
	}

	/* Enable MPSSE mode */
	ret = ftdi_set_bit_mode(ftdev->handle, 0, FTDI_BITMODE_MPSSE);
	if (ret) {
		logm_err("Failed to enable MPSSE mode\n");
		return ret;
	}

	/* Set initial clock */
	STATUS_CHECK_RET(mpsse_set_clock(ftdev, MPSSE_INIT_CLK, NULL));

	mpsse_control_loopback(ftdev, false);
	mpsse_control_adaptive_clock(ftdev, false);

	/* Set all gpios to input(tri-state) */
	cmd[0] = MPSSE_CMD_SET_BITS_LOW;
	cmd[1] = cmd[2] = 0;
	STATUS_CHECK_RET(ftdi_write(ftdev->handle, cmd, sizeof(cmd)));

	if (ftdev->max_gpios == 16) {
		cmd[0] = MPSSE_CMD_SET_BITS_HIGH;
		STATUS_CHECK_RET(ftdi_write(ftdev->handle, cmd, sizeof(cmd)));
	}

	ftdi_purge_all(ftdev->handle);

	ftdev->scratch_buffer = malloc(MPSSE_BUF_LEN);
	if (!ftdev->scratch_buffer) {
		logm_err("No memory for MPSSE buffer\n");
		return UFP_NOMEM;
	}

	return UFP_OK;
}

ufprog_status mpsse_cleanup(struct ufprog_if_dev *ftdev)
{
	/* Reset bitmode */
	ftdi_set_bit_mode(ftdev->handle, 0, FTDI_BITMODE_RESET);

	if (ftdev->scratch_buffer) {
		free(ftdev->scratch_buffer);
		ftdev->scratch_buffer = NULL;
	}

	if (ftdev->lock)
		os_free_mutex(ftdev->lock);

	return UFP_OK;
}

uint32_t UFPROG_API ufprog_driver_version(void)
{
	return MAKE_VERSION(MPSSE_DRV_VER_MAJOR, MPSSE_DRV_VER_MINOR);
}

uint32_t UFPROG_API ufprog_driver_supported_if(void)
{
	/* TODO: add I2C support */
	return IFM_SPI;
}

ufprog_status UFPROG_API ufprog_device_lock(struct ufprog_if_dev *ftdev)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (!ftdev->lock)
		return UFP_OK;

	return os_mutex_lock(ftdev->lock) ? UFP_OK : UFP_LOCK_FAIL;
}

ufprog_status UFPROG_API ufprog_device_unlock(struct ufprog_if_dev *ftdev)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (!ftdev->lock)
		return UFP_OK;

	return os_mutex_unlock(ftdev->lock) ? UFP_OK : UFP_LOCK_FAIL;
}

