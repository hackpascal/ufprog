/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Generic hexdump
 */
#pragma once

#ifndef _UFPROG_HEXDUMP_H_
#define _UFPROG_HEXDUMP_H_

#include <stdint.h>
#include <ufprog/common.h>

EXTERN_C_BEGIN

void UFPROG_API hexdump(const void *data, size_t size, uint64_t addr, ufprog_bool head_align);

EXTERN_C_END

#endif /* _UFPROG_HEXDUMP_H_ */
