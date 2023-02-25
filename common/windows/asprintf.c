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
	int len;

	len = vsnprintf(NULL, 0, fmt, arg);
	if (len < 0)
		return len;

	*strp = malloc(len + 1);
	if (!*strp)
		return -1;

	return vsnprintf(*strp, len + 1, fmt, arg);
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
