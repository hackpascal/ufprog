/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc. All Rights Reserved.
 *
 * OS-dependent definitions for NAND Mapped-block Management (NMBM)
 *
 * Author: Weijie Gao <weijie.gao@mediatek.com>
 */

#ifndef _NMBM_OS_H_
#define _NMBM_OS_H_

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ufprog/crc32.h>
#include <ufprog/bits.h>

#define nmbm_crc32			crc32_no_comp

static inline uint64_t nmbm_lldiv(uint64_t dividend, uint32_t divisor)
{
	return dividend / divisor;
}

#ifdef CONFIG_NMBM_LOG_LEVEL_DEBUG
#define NMBM_DEFAULT_LOG_LEVEL		0
#elif defined(NMBM_LOG_LEVEL_INFO)
#define NMBM_DEFAULT_LOG_LEVEL		1
#elif defined(NMBM_LOG_LEVEL_WARN)
#define NMBM_DEFAULT_LOG_LEVEL		2
#elif defined(NMBM_LOG_LEVEL_ERR)
#define NMBM_DEFAULT_LOG_LEVEL		3
#elif defined(NMBM_LOG_LEVEL_EMERG)
#define NMBM_DEFAULT_LOG_LEVEL		4
#elif defined(NMBM_LOG_LEVEL_NONE)
#define NMBM_DEFAULT_LOG_LEVEL		5
#else
#define NMBM_DEFAULT_LOG_LEVEL		1
#endif

#define WATCHDOG_RESET()

#endif /* _NMBM_OS_H_ */
