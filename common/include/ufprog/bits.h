/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Bit operations
 */
#pragma once

#ifndef _UFPROG_BITS_H_
#define _UFPROG_BITS_H_

#include <stdlib.h>
#include <stdint.h>
#include <ufprog/common.h>

EXTERN_C_BEGIN

#define BIT(n)					(1UL << (n))

#define BITS(h, l)				(((~0UL) - (1UL << (l)) + 1) & \
						 (~0UL >> (sizeof(long) * 8 - 1 - (h))))

#define FIELD_GET(_field_name, _val)		(((_val) & (_field_name ## _M)) >> (_field_name ## _S))
#define FIELD_SET(_field_name, _field_val)	(((_field_val) << (_field_name ## _S)) & (_field_name ## _M))
#define FIELD_MAX(_field_name)			((_field_name ## _M) >> (_field_name ## _S))

uint32_t UFPROG_API generic_ffs(size_t word);
uint32_t UFPROG_API generic_fls(size_t word);

static inline uint32_t generic_ffs64(uint64_t word)
{
	uint32_t l = word & 0xffffffff, h = word >> 32;

	if (SIZE_MAX == UINT64_MAX)
		return generic_ffs(word);

	if (l)
		return generic_ffs(l);

	if (h)
		return generic_ffs(h) + 32;

	return 0;
}

static inline uint32_t generic_fls64(uint64_t word)
{
	uint32_t h = word >> 32;

	if (SIZE_MAX == UINT64_MAX)
		return generic_fls(word);

	if (h)
		return generic_fls(h) + 32;

	return generic_fls(word);

}

uint32_t UFPROG_API generic_hweight32(uint32_t w);
uint32_t UFPROG_API generic_hweight16(uint16_t w);
uint32_t UFPROG_API generic_hweight8(uint8_t w);

#ifdef _MSC_VER
#define ffs(_word)		generic_ffs(_word)
#define ffs64(_word)		generic_ffs64(_word)

#define fls(_word)		generic_fls(_word)
#define fls64(_word)		generic_fls64(_word)

#define hweight32(_word)	generic_hweight32(_word)
#define hweight16(_word)	generic_hweight16(_word)
#define hweight8(_word)		generic_hweight8(_word)
#else
#define ffs(_word)		(__builtin_constant_p(_word) ?  __builtin_ffsll(_word) : generic_ffs(_word))
#define ffs64(_word)		(__builtin_constant_p(_word) ?  __builtin_ffsll(_word) : generic_ffs64(_word))

#define __builtin_flsll(_word)	((_word) ? sizeof((_word)) * 8 - __builtin_clzll(_word) : 0)
#define fls(_word)		(__builtin_constant_p(_word) ?  __builtin_flsll(_word) : generic_fls(_word))
#define fls64(_word)		(__builtin_constant_p(_word) ?  __builtin_flsll(_word) : generic_fls64(_word))

#define hweight32(_word)	(__builtin_constant_p(_word) ?  __builtin_popcountll(_word) : generic_hweight32(_word))
#define hweight16(_word)	(__builtin_constant_p(_word) ?  __builtin_popcountll(_word) : generic_hweight16(_word))
#define hweight8(_word)		(__builtin_constant_p(_word) ?  __builtin_popcountll(_word) : generic_hweight8(_word))
#endif

static inline ufprog_bool is_power_of_2(uint64_t n)
{
	return (n != 0 && ((n & (n - 1)) == 0));
}

EXTERN_C_END

#endif /* _UFPROG_BITS_H_ */
