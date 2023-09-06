/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Simple Flash Translation Layer (FTL) driver management
 */
#pragma once

#ifndef _UFPROG_FTL_DRIVER_H_
#define _UFPROG_FTL_DRIVER_H_

#include <ufprog/osdef.h>
#include <ufprog/config.h>

EXTERN_C_BEGIN

#define FTL_DRIVER_DIR_NAME			"ftl"

struct ufprog_ftl_driver;

ufprog_status UFPROG_API ufprog_load_ftl_config(const char *name, struct json_object **outconfig);

ufprog_status UFPROG_API ufprog_load_ftl_driver(const char *name, struct ufprog_ftl_driver **outdrv);
ufprog_status UFPROG_API ufprog_unload_ftl_driver(struct ufprog_ftl_driver *drv);
uint32_t UFPROG_API ufprog_ftl_driver_instance_count(struct ufprog_ftl_driver *drv);

const char *UFPROG_API ufprog_ftl_driver_name(struct ufprog_ftl_driver *drv);
uint32_t UFPROG_API ufprog_ftl_driver_version(struct ufprog_ftl_driver *drv);
uint32_t UFPROG_API ufprog_ftl_driver_api_version(struct ufprog_ftl_driver *drv);
const char *UFPROG_API ufprog_ftl_driver_desc(struct ufprog_ftl_driver *drv);

EXTERN_C_END

#endif /* _UFPROG_FTL_DRIVER_H_ */
