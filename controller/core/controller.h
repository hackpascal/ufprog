/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Controller internal definitions
 */
#pragma once

#ifndef _UFPROG_CONTROLLER_H_
#define _UFPROG_CONTROLLER_H_

#include <ufprog/api_controller.h>
#include <ufprog/osdef.h>
#include <plugin-common.h>

struct ufprog_controller_driver {
	struct plugin plugin;

	uint32_t supported_if;

	api_device_open open_device;
	api_device_free free_device;

	api_device_lock lock_device;
	api_device_unlock unlock_device;

	api_device_reset reset_device;
	api_device_cancel_transfer cancel_transfer;
	api_set_device_disconnect_cb set_disconnect_cb;

	struct ufprog_lookup_table *devices;
};

struct ufprog_controller_device {
	uint32_t if_type;
	char *name;
	struct ufprog_controller_driver *driver;
	struct ufprog_interface *ifdev;
};

ufprog_status ufprog_controller_add_device(struct ufprog_controller_driver *drv, const struct ufprog_interface *ifdev);
ufprog_status ufprog_controller_remove_device(struct ufprog_controller_driver *drv,
					      const struct ufprog_interface *ifdev);

#endif /* _UFPROG_CONTROLLER_H_ */
