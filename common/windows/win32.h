/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Windows-specific declarations
 */
#pragma once

#ifndef _UFPROG_WIN32_H_
#define _UFPROG_WIN32_H_

#define _WIN32_LEAN_AND_MEAN
#include <tchar.h>
#include <Windows.h>
#include <strsafe.h>
#include <versionhelpers.h>
#include <shlobj.h>

PWSTR utf8_to_wcs(PCSTR pInputText);
char *wcs_to_utf8(PCWSTR pInputText);

PWSTR get_system_error_va(DWORD dwErrorCode, UINT nNumArgs, va_list args);
PWSTR get_system_error(DWORD dwErrorCode, UINT nNumArgs, ...);
char *get_system_error_utf8(DWORD dwErrorCode, UINT nNumArgs, ...);
void log_sys_error_utf8(DWORD dwErrorCode, const char *fmt, ...);

#endif /* _UFPROG_WIN32_H_ */
