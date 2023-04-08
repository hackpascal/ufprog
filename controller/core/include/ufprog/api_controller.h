/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Controller basic interface definitions
 */
#pragma once

#ifndef _UFPROG_API_CONTROLLER_H_
#define _UFPROG_API_CONTROLLER_H_

#include <ufprog/bits.h>
#include <ufprog/config.h>
#include <ufprog/api_plugin.h>

EXTERN_C_BEGIN

#define CONTROLLER_DRIVER_API_VERSION_MAJOR	1
#define CONTROLLER_DRIVER_API_VERSION_MINOR	0

struct ufprog_if_dev;

enum ufprog_drv_if_type {
	IF_SPI,
	IF_I2C,
	IF_NAND,
	IF_SDIO,

	__MAX_IF_TYPE
};

#define IFM_SPI					BIT(IF_SPI)
#define IFM_I2C					BIT(IF_I2C)
#define IFM_NAND				BIT(IF_NAND)
#define IFM_SDIO				BIT(IF_SDIO)

#define IF_TYPE_BIT(_t)				(BIT(_t) & (BIT(__MAX_IF_TYPE) - 1))

#define API_NAME_DRV_SUPPORTED_IF		"ufprog_driver_supported_if"
typedef uint32_t (UFPROG_API *api_drv_supported_if)(void);

#define API_NAME_DEVICE_OPEN			"ufprog_device_open"
typedef ufprog_status (UFPROG_API *api_device_open)(uint32_t if_type, struct json_object *config,
						    ufprog_bool thread_safe, struct ufprog_if_dev **outifdev);

#define API_NAME_DEVICE_FREE			"ufprog_device_free"
typedef ufprog_status (UFPROG_API *api_device_free)(struct ufprog_if_dev *ifdev);

#define API_NAME_DEVICE_LOCK			"ufprog_device_lock"
typedef ufprog_status (UFPROG_API *api_device_lock)(struct ufprog_if_dev *ifdev);

#define API_NAME_DEVICE_UNLOCK			"ufprog_device_unlock"
typedef ufprog_status (UFPROG_API *api_device_unlock)(struct ufprog_if_dev *ifdev);

/* Optional base APIs */
#define API_NAME_DEVICE_RESET			"ufprog_device_reset"
typedef ufprog_status (UFPROG_API *api_device_reset)(struct ufprog_if_dev *ifdev);

/* Must be thread-safe */
#define API_NAME_DEVICE_CANCEL_TRANSFER		"ufprog_device_cancel_transfer"
typedef ufprog_status (UFPROG_API *api_device_cancel_transfer)(struct ufprog_if_dev *ifdev);

#define API_NAME_SET_DEVICE_DISCONNECT_CB	"ufprog_set_device_disconnect_cb"
typedef void (UFPROG_API *ufprog_dev_disconnect_cb)(struct ufprog_if_dev *ifdev);
typedef ufprog_status (UFPROG_API *api_set_device_disconnect_cb)(struct ufprog_if_dev *ifdev,
								 ufprog_dev_disconnect_cb cb, void *priv);

EXTERN_C_END

#endif /* _UFPROG_API_CONTROLLER_H_ */
