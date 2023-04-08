/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Common plugin interface definitions
 */
#pragma once

#ifndef _UFPROG_API_PLUGIN_H_
#define _UFPROG_API_PLUGIN_H_

#include <stdint.h>
#include <ufprog/common.h>

EXTERN_C_BEGIN

#define API_NAME_PLUGIN_INIT			"ufprog_plugin_init"
typedef ufprog_status (UFPROG_API *api_plugin_init)(void);

#define API_NAME_PLUGIN_CLEANUP			"ufprog_plugin_cleanup"
typedef ufprog_status (UFPROG_API *api_plugin_cleanup)(void);

#define API_NAME_PLUGIN_VERSION			"ufprog_plugin_version"
typedef uint32_t (UFPROG_API *api_plugin_version)(void);

#define API_NAME_PLUGIN_API_VERSION		"ufprog_plugin_api_version"
typedef uint32_t (UFPROG_API *api_plugin_api_version)(void);

#define API_NAME_PLUGIN_DESC			"ufprog_plugin_desc"
typedef const char *(UFPROG_API *api_plugin_desc)(void);

EXTERN_C_END

#endif /* _UFPROG_API_PLUGIN_H_ */
