/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * String related
 */
#pragma once

#ifndef _UFPROG_STRING_H_
#define _UFPROG_STRING_H_

#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ufprog/common.h>

EXTERN_C_BEGIN

#if defined(WIN32)
#define os_strdup			_strdup
#define strcasecmp			_stricmp
#define strncasecmp			_strnicmp

char *strcasestr(const char *s, const char *find);
size_t strlcpy(char *dst, const char *src, size_t dsize);
size_t strlcat(char *dst, const char *src, size_t dsize);
char *strndup(const char *str, size_t n);
ssize_t wgetdelim(wchar_t **bufptr, size_t *n, int delim, FILE *fp);
int vasprintf(char **strp, const char *fmt, va_list ap);
int asprintf(char **strp, const char *fmt, ...);
#else
#include <strings.h>
#define os_strdup			strdup
#endif

EXTERN_C_END

#endif /* _UFPROG_STRING_H_ */
