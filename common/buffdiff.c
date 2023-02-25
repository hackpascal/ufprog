// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Generic memory buffer comparision implementation
 */

#include <stdint.h>
#include <stdbool.h>
#include <ufprog/buffdiff.h>

static ufprog_bool buf_diff_byte(const void *a, const void *b, size_t len, size_t *retpos)
{
	const uint8_t *sa = a, *sb = b;
	size_t n = 0;

	while (n < len) {
		if (*sa++ != *sb++) {
			*retpos = n;
			return false;
		}

		n++;
	}

	return true;
}

static ufprog_bool buf_diff_sizeptr(const void *a, const void *b, size_t len, size_t *retpos)
{
	const size_t *sa = a, *sb = b;
	size_t n = 0, cmppos = 0;

	while (n < len) {
		if (*sa != *sb) {
			buf_diff_byte(sa, sb, sizeof(size_t), &cmppos);
			*retpos = n + cmppos;
			return false;
		}

		sa++;
		sb++;
		n += sizeof(size_t);
	}

	return true;
}

ufprog_bool UFPROG_API bufdiff(const void *a, const void *b, size_t len, size_t *retpos)
{
	size_t ua, ub, left, cmppos = 0;
	ufprog_bool ret;

	ua = ((size_t)a) & (sizeof(size_t) - 1);
	ub = ((size_t)b) & (sizeof(size_t) - 1);

	if (ua != ub)
		return buf_diff_byte(a, b, len, retpos);

	if (ua) {
		ret = buf_diff_byte(a, b, ua, retpos);
		if (!ret)
			return ret;

		a = (void *)((uintptr_t)a + ua);
		b = (void *)((uintptr_t)b + ub);
		len -= ua;
	}

	left = len & (sizeof(size_t) - 1);
	len -= left;

	if (len) {
		ret = buf_diff_sizeptr(a, b, len, &cmppos);
		if (!ret) {
			if (retpos)
				*retpos = ua + cmppos;
			return ret;
		}

		a = (void *)((uintptr_t)a + len);
		b = (void *)((uintptr_t)b + len);
	}

	if (!left)
		return true;

	ret = buf_diff_byte(a, b, left, &cmppos);
	if (ret)
		return ret;

	if (retpos)
		*retpos = ua + len + cmppos;

	return false;
}
