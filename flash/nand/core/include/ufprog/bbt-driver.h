/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * BBT driver management
 */
#pragma once

#ifndef _UFPROG_BBT_DRIVER_H_
#define _UFPROG_BBT_DRIVER_H_

#include <ufprog/osdef.h>
#include <ufprog/config.h>

EXTERN_C_BEGIN

#define BBT_DRIVER_DIR_NAME			"bbt"

struct ufprog_bbt_driver;

ufprog_status UFPROG_API ufprog_load_bbt_config(const char *name, struct json_object **outconfig);

ufprog_status UFPROG_API ufprog_load_bbt_driver(const char *name, struct ufprog_bbt_driver **outdrv);
ufprog_status UFPROG_API ufprog_unload_bbt_driver(struct ufprog_bbt_driver *drv);
uint32_t UFPROG_API ufprog_bbt_driver_instance_count(struct ufprog_bbt_driver *drv);

const char *UFPROG_API ufprog_bbt_driver_name(struct ufprog_bbt_driver *drv);
uint32_t UFPROG_API ufprog_bbt_driver_version(struct ufprog_bbt_driver *drv);
uint32_t UFPROG_API ufprog_bbt_driver_api_version(struct ufprog_bbt_driver *drv);
const char *UFPROG_API ufprog_bbt_driver_desc(struct ufprog_bbt_driver *drv);

EXTERN_C_END

#endif /* _UFPROG_BBT_DRIVER_H_ */
