/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Controller driver management
 */
#pragma once

#ifndef _UFPROG_DRIVER_H_
#define _UFPROG_DRIVER_H_

#include <ufprog/osdef.h>

EXTERN_C_BEGIN

#define CONTROLLER_DRIVER_DIR_NAME		"controller"

struct ufprog_driver;

ufprog_status UFPROG_API ufprog_load_driver(const char *name, struct ufprog_driver **outdrv);
ufprog_status UFPROG_API ufprog_unload_driver(struct ufprog_driver *drv);
uint32_t UFPROG_API ufprog_driver_device_count(struct ufprog_driver *drv);

const char *UFPROG_API ufprog_driver_name(struct ufprog_driver *drv);
module_handle UFPROG_API ufprog_driver_module(struct ufprog_driver *drv);
uint32_t UFPROG_API ufprog_driver_version(struct ufprog_driver *drv);
const char *UFPROG_API ufprog_driver_desc(struct ufprog_driver *drv);
uint32_t UFPROG_API ufprog_driver_supported_if(struct ufprog_driver *drv);

void *UFPROG_API ufprog_driver_find_symbol(struct ufprog_driver *drv, const char *name);
ufprog_bool UFPROG_API ufprog_driver_find_module_symbols(struct ufprog_driver *drv, struct symbol_find_entry *list,
							 size_t count, ufprog_bool full);

EXTERN_C_END

#endif /* _UFPROG_CONTROLLER_H_ */
