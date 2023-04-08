/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Common implementation for WCH CH347 chip
 */

#include <ufprog/api_controller.h>
#include <ufprog/log.h>
#include "ch347.h"

#define CH347_DRV_API_VER_MAJOR			1
#define CH347_DRV_API_VER_MINOR			0

ufprog_status ch347_init(struct ufprog_interface *wchdev, bool thread_safe)
{
	if (thread_safe) {
		if (!os_create_mutex(&wchdev->lock)) {
			logm_err("Failed to create lock for thread-safe");
			return UFP_LOCK_FAIL;
		}
	}

	return UFP_OK;
}

uint32_t UFPROG_API ufprog_plugin_api_version(void)
{
	return MAKE_VERSION(CH347_DRV_API_VER_MAJOR, CH347_DRV_API_VER_MINOR);
}

uint32_t UFPROG_API ufprog_controller_supported_if(void)
{
	/* TODO: add I2C support */
	return IFM_SPI;
}

ufprog_status UFPROG_API ufprog_device_lock(struct ufprog_interface *wchdev)
{
	if (!wchdev)
		return UFP_INVALID_PARAMETER;

	if (!wchdev->lock)
		return UFP_OK;

	return os_mutex_lock(wchdev->lock) ? UFP_OK : UFP_LOCK_FAIL;
}

ufprog_status UFPROG_API ufprog_device_unlock(struct ufprog_interface *wchdev)
{
	if (!wchdev)
		return UFP_INVALID_PARAMETER;

	if (!wchdev->lock)
		return UFP_OK;

	return os_mutex_unlock(wchdev->lock) ? UFP_OK : UFP_LOCK_FAIL;
}

