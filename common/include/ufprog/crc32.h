/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * CRC32 checksum helpers
 */
#pragma once

#ifndef _UFPROG_CRC32_H_
#define _UFPROG_CRC32_H_

#include <stddef.h>
#include <stdint.h>
#include <ufprog/common.h>

EXTERN_C_BEGIN

#define CRC32_TABLE_NUM_ENTRIES		256

#define CRC32_REFLECTED_POLYNOMIAL	0xedb88320
#define CRC32_NORMAL_POLYNOMIAL		0x04c11db7

uint32_t crc32_reflected_cal(uint32_t crc, const void *data, size_t length, const uint32_t *crc32_table);
void crc32_reflected_init(uint32_t *crc32_table, uint32_t poly);

uint32_t crc32_normal_cal(uint32_t crc, const void *data, size_t length, const uint32_t *crc32_table);
void crc32_normal_init(uint32_t *crc32_table, uint32_t poly);

uint32_t crc32_no_comp(uint32_t crc, const void *data, size_t length);
uint32_t crc32_be_no_comp(uint32_t crc, const void *data, size_t length);

static inline uint32_t crc32(uint32_t crc, const void *data, size_t length)
{
	/* reflected */
	return crc32_no_comp(crc ^ 0xffffffff, data, length) ^ 0xffffffff;
}

static inline uint32_t crc32_be(uint32_t crc, const void *data, size_t length)
{
	/* normal */
	return crc32_be_no_comp(crc ^ 0xffffffff, data, length) ^ 0xffffffff;
}

EXTERN_C_END

#endif /* _UFPROG_CRC32_H_ */
