// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Bit-wise manipulations
 */

#include <ufprog/bits.h>

uint32_t UFPROG_API generic_ffs(size_t word)
{
	uint32_t num = 1;

	if (!word)
		return 0;

	if (SIZE_MAX == UINT64_MAX) {
		if ((word & 0xffffffff) == 0) {
			num += 32;
			word >>= 32;
		}
	}

	if ((word & 0xffff) == 0) {
		num += 16;
		word >>= 16;
	}

	if ((word & 0xff) == 0) {
		num += 8;
		word >>= 8;
	}

	if ((word & 0xf) == 0) {
		num += 4;
		word >>= 4;
	}

	if ((word & 0x3) == 0) {
		num += 2;
		word >>= 2;
	}

	if ((word & 0x1) == 0)
		num += 1;

	return num;
}

uint32_t UFPROG_API generic_fls(size_t word)
{
	uint32_t num = 32;

	if (!word)
		return 0;

	if (SIZE_MAX == UINT64_MAX) {
		num = 64;

		if (!(word & 0xffffffff00000000ULL)) {
			num -= 32;
			word <<= 32;
		}
	}

	if (!(word & 0xffff0000U)) {
		num -= 16;
		word <<= 16;
	}

	if (!(word & 0xff000000U)) {
		num -= 8;
		word <<= 8;
	}

	if (!(word & 0xf0000000U)) {
		num -= 4;
		word <<= 4;
	}

	if (!(word & 0xc0000000U)) {
		num -= 2;
		word <<= 2;
	}

	if (!(word & 0x80000000U)) {
		num -= 1;
		word <<= 1;
	}

	return num;
}

uint32_t UFPROG_API generic_hweight32(uint32_t w)
{
	uint32_t res = (w & 0x55555555) + ((w >> 1) & 0x55555555);
	res = (res & 0x33333333) + ((res >> 2) & 0x33333333);
	res = (res & 0x0F0F0F0F) + ((res >> 4) & 0x0F0F0F0F);
	res = (res & 0x00FF00FF) + ((res >> 8) & 0x00FF00FF);
	return (res & 0x0000FFFF) + ((res >> 16) & 0x0000FFFF);
}

uint32_t UFPROG_API generic_hweight16(uint16_t w)
{
	uint32_t res = (w & 0x5555) + ((w >> 1) & 0x5555);
	res = (res & 0x3333) + ((res >> 2) & 0x3333);
	res = (res & 0x0F0F) + ((res >> 4) & 0x0F0F);
	return (res & 0x00FF) + ((res >> 8) & 0x00FF);
}

uint32_t UFPROG_API generic_hweight8(uint8_t w)
{
	uint32_t res = (w & 0x55) + ((w >> 1) & 0x55);
	res = (res & 0x33) + ((res >> 2) & 0x33);
	return (res & 0x0F) + ((res >> 4) & 0x0F);
}
