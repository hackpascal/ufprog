/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Common module initialization
 */

#include <stdlib.h>
#include "crc32.h"

static int ufprog_common_init(void)
{
	make_crc_table();

	return 0;
}

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		if (ufprog_common_init())
			return FALSE;

		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}
#else
__attribute__((constructor))
static void device_module_init(void)
{
	if (ufprog_common_init())
		exit(1);
}
#endif
