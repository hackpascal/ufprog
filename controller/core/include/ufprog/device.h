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

struct ufprog_controller_device;

ufprog_status UFPROG_API ufprog_controller_open_device(struct ufprog_controller_driver *drv, uint32_t if_type,
						       struct json_object *config, ufprog_bool thread_safe,
						       struct ufprog_controller_device **outdev);
ufprog_status UFPROG_API ufprog_controller_open_device_by_name(const char *name, uint32_t if_type,
							       ufprog_bool thread_safe,
							       struct ufprog_controller_device **outdev);
ufprog_status UFPROG_API ufprog_controller_close_device(struct ufprog_controller_device *dev);

const char *UFPROG_API ufprog_controller_device_name(struct ufprog_controller_device *dev);
uint32_t UFPROG_API ufprog_controller_device_if_type(struct ufprog_controller_device *dev);
struct ufprog_controller_driver *UFPROG_API ufprog_controller_device_get_driver(struct ufprog_controller_device *dev);
struct ufprog_interface *UFPROG_API ufprog_controller_device_get_interface(struct ufprog_controller_device *dev);

ufprog_status UFPROG_API ufprog_controller_device_lock(struct ufprog_controller_device *dev);
ufprog_status UFPROG_API ufprog_controller_device_unlock(struct ufprog_controller_device *dev);

ufprog_status UFPROG_API ufprog_controller_reset_device(struct ufprog_controller_device *dev);
ufprog_status UFPROG_API ufprog_controller_cancel_device_transfer(struct ufprog_controller_device *dev);
ufprog_status UFPROG_API ufprog_controller_set_device_disconnect_cb(struct ufprog_controller_device *dev,
								    ufprog_device_disconnect_cb cb, void *priv);

EXTERN_C_END

#endif /* _UFPROG_DEVICE_H_ */
