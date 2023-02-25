/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Device module initialization
 */

#include <stdlib.h>
#include "init.h"

static int ufprog_controller_core_init(void)
{
	if (driver_lookup_table_init())
		return -1;

	if (libusb_global_init())
		return -1;

	return 0;
}

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		if (ufprog_controller_core_init())
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
	if (ufprog_controller_core_init())
		exit(1);
}
#endif
