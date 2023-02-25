/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Controller device management
 */
#pragma once

#ifndef _UFPROG_DEVICE_H_
#define _UFPROG_DEVICE_H_

#include <ufprog/api_controller.h>
#include <ufprog/config.h>
#include <ufprog/driver.h>

EXTERN_C_BEGIN

struct ufprog_device;

ufprog_status UFPROG_API ufprog_open_device(struct ufprog_driver *drv, uint32_t if_type, struct json_object *config,
					    ufprog_bool thread_safe, struct ufprog_device **outdev);
ufprog_status UFPROG_API ufprog_open_device_by_name(const char *name, uint32_t if_type, ufprog_bool thread_safe,
						    struct ufprog_device **outdev);
ufprog_status UFPROG_API ufprog_close_device(struct ufprog_device *dev);

const char *UFPROG_API ufprog_device_name(struct ufprog_device *dev);
uint32_t UFPROG_API ufprog_device_if_type(struct ufprog_device *dev);
struct ufprog_driver *UFPROG_API ufprog_device_get_driver(struct ufprog_device *dev);
struct ufprog_if_dev *UFPROG_API ufprog_device_get_interface_device(struct ufprog_device *dev);

ufprog_status UFPROG_API ufprog_lock_device(struct ufprog_device *dev);
ufprog_status UFPROG_API ufprog_unlock_device(struct ufprog_device *dev);

ufprog_status UFPROG_API ufprog_reset_device(struct ufprog_device *dev);
ufprog_status UFPROG_API ufprog_cancel_transfer(struct ufprog_device *dev);
ufprog_status UFPROG_API ufprog_set_disconnect_cb(struct ufprog_device *dev, ufprog_dev_disconnect_cb cb, void *priv);

EXTERN_C_END

#endif /* _UFPROG_DEVICE_H_ */
