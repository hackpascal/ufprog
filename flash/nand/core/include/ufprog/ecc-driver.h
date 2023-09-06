/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ECC driver management
 */
#pragma once

#ifndef _UFPROG_ECC_DRIVER_H_
#define _UFPROG_ECC_DRIVER_H_

#include <ufprog/osdef.h>
#include <ufprog/config.h>

EXTERN_C_BEGIN

#define ECC_DRIVER_DIR_NAME			"ecc"

struct ufprog_ecc_driver;

ufprog_status UFPROG_API ufprog_load_ecc_config(const char *name, struct json_object **outconfig);

ufprog_status UFPROG_API ufprog_load_ecc_driver(const char *name, struct ufprog_ecc_driver **outdrv);
ufprog_status UFPROG_API ufprog_unload_ecc_driver(struct ufprog_ecc_driver *drv);
uint32_t UFPROG_API ufprog_ecc_driver_instance_count(struct ufprog_ecc_driver *drv);

const char *UFPROG_API ufprog_ecc_driver_name(struct ufprog_ecc_driver *drv);
uint32_t UFPROG_API ufprog_ecc_driver_version(struct ufprog_ecc_driver *drv);
uint32_t UFPROG_API ufprog_ecc_driver_api_version(struct ufprog_ecc_driver *drv);
const char *UFPROG_API ufprog_ecc_driver_desc(struct ufprog_ecc_driver *drv);

EXTERN_C_END

#endif /* _UFPROG_ECC_DRIVER_H_ */
