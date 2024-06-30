// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * vasprintf/asprintf for Windows
 */

#include <malloc.h>
#include <stdio.h>
#include <stdarg.h>

int vasprintf(char **strp, const char *fmt, va_list arg)
{
	va_list args;
	int len;

	va_copy(args, arg);

	len = vsnprintf(NULL, 0, fmt, arg);
	if (len < 0) {
		va_end(args);
		return len;
	}

	*strp = malloc(len + 1);
	if (!*strp) {
		va_end(args);
		return -1;
	}

	len = vsnprintf(*strp, len + 1, fmt, args);
	va_end(args);

	return len;
}

int asprintf(char **strp, const char *fmt, ...)
{
	va_list args;
	int ret;

	if (!fmt)
		return -1;

	va_start(args, fmt);
	ret = vasprintf(strp, fmt, args);
	va_end(args);

	return ret;
}
