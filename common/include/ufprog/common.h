/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Common definitions
 */
#pragma once

#ifndef _UFPROG_COMMON_H_
#define _UFPROG_COMMON_H_

#include <ufprog/status_code.h>

#ifdef __cplusplus
#define EXTERN_C_BEGIN		\
	extern "C" {

#define EXTERN_C_END		\
	}
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)				(sizeof(a) / sizeof(a[0]))
#endif

#if defined(WIN32)
#define UFPROG_API				__stdcall
#else
#define UFPROG_API
#endif

/* Generic directory names */
#define UFPROG_APPDATA_NAME			"ufprog"
#define UFPROG_DEVICE_DIR_NAME			"device"
#define UFPROG_INTERFACE_DIR_NAME		"controller"
#define UFPROG_CONFIG_SUFFIX			".json"

/* Compiler-independent types */
typedef int ufprog_bool;

#endif /* _UFPROG_COMMON_H_ */
