/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Common implementation for FTDI FT4222H
 */

#include <ufprog/api_controller.h>
#include <ufprog/log.h>
#include "ft4222h.h"

#define FT4222H_DRV_API_VER_MAJOR		1
#define FT4222H_DRV_API_VER_MINOR		0

const uint32_t ft4222_sys_clks[] = {
	[SYS_CLK_60] = 60000000,
	[SYS_CLK_24] = 24000000,
	[SYS_CLK_48] = 48000000,
	[SYS_CLK_80] = 80000000
};

ufprog_status ft4222_init(struct ufprog_interface *ftdev, bool thread_safe)
{
	ufprog_status ret;

	if (thread_safe) {
		if (!os_create_mutex(&ftdev->lock)) {
			logm_err("Failed to create lock for thread-safe");
			return UFP_LOCK_FAIL;
		}
	}

	ftdi_reset(ftdev->handle);
	ftdi_purge_all(ftdev->handle);

	ret = ftdi_vendor_cmd_get(ftdev->handle, 0, &ftdev->hwver, sizeof(ftdev->hwver));
	if (ret) {
		logm_err("Failed to get chip firmware info\n");
		return ret;
	}

	if (ftdev->hwver.chip_model[0] != 0x42 || ftdev->hwver.chip_model[1] != 0x22) {
		logm_err("Not a FT4222H device\n");
		return UFP_UNSUPPORTED;
	}

	logm_info("Firmware version: %u\n", ftdev->hwver.fwver);

	ret = ftdi_vendor_cmd_get(ftdev->handle, 1, &ftdev->hwcaps, sizeof(ftdev->hwcaps));
	if (ret) {
		logm_err("Failed to get hardware caps\n");
		return ret;
	}

	logm_info("Chip mode: %u\n", ftdev->hwcaps.chip_mode);

	if (ftdev->hwcaps.field_2)
		ftdev->max_buck_size = 64;
	else if (ftdev->hwcaps.chip_mode < 3)
		ftdev->max_buck_size = 256;
	else
		ftdev->max_buck_size = 512;

	ret = ftdi_set_latency_timer(ftdev->handle, 2);
	if (ret)
		logm_warn("Failed to set latency timer\n");

	return UFP_OK;
}

ufprog_status ft4222_get_clock(struct ufprog_interface *ftdev, enum ft4222_clock *clk)
{
	ufprog_status ret = UFP_OK;
	uint8_t val;

	os_mutex_lock(ftdev->lock);

	ret = ftdi_vendor_cmd_get(ftdev->handle, 4, &val, 1);
	if (ret) {
		logm_err("Failed to get clock source\n");
		goto out;
	}

	if (clk)
		*clk = val;

	ftdev->hwcaps.clk = val;

out:
	os_mutex_unlock(ftdev->lock);

	return ret;
}

ufprog_status ft4222_set_clock(struct ufprog_interface *ftdev, enum ft4222_clock clk, bool force)
{
	ufprog_status ret = UFP_OK;
	uint8_t val = clk;

	if (ftdev->hwcaps.clk == clk && !force)
		return ret;

	os_mutex_lock(ftdev->lock);

	ret = ftdi_vendor_cmd_set(ftdev->handle, 4, &val, 1);
	if (ret) {
		logm_err("Failed to set clock source\n");
		goto out;
	}

	ftdev->hwcaps.clk = clk;

out:
	os_mutex_unlock(ftdev->lock);

	return ret;
}

ufprog_status ft4222_set_function(struct ufprog_interface *ftdev, enum ft4222_function func)
{
	ufprog_status ret = UFP_OK;
	uint8_t val = func;

	os_mutex_lock(ftdev->lock);

	ret = ftdi_vendor_cmd_set(ftdev->handle, 5, &val, 1);
	if (ret) {
		logm_err("Failed to set function mode\n");
		goto out;
	}

	ftdev->hwcaps.function_mode = func;

out:
	os_mutex_unlock(ftdev->lock);

	return ret;
}

uint32_t UFPROG_API ufprog_plugin_api_version(void)
{
	return MAKE_VERSION(FT4222H_DRV_API_VER_MAJOR, FT4222H_DRV_API_VER_MINOR);
}

uint32_t UFPROG_API ufprog_controller_supported_if(void)
{
	/* TODO: add I2C support */
	return IFM_SPI;
}

ufprog_status UFPROG_API ufprog_device_lock(struct ufprog_interface *ftdev)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (!ftdev->lock)
		return UFP_OK;

	return os_mutex_lock(ftdev->lock) ? UFP_OK : UFP_LOCK_FAIL;
}

ufprog_status UFPROG_API ufprog_device_unlock(struct ufprog_interface *ftdev)
{
	if (!ftdev)
		return UFP_INVALID_PARAMETER;

	if (!ftdev->lock)
		return UFP_OK;

	return os_mutex_unlock(ftdev->lock) ? UFP_OK : UFP_LOCK_FAIL;
}
