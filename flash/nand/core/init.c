/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * NAND core module initialization
 */

#include <stdlib.h>
#include "init.h"

static int ufprog_nand_core_init(void)
{
	if (ecc_driver_mgmt_init())
		goto cleanup;

	if (bbt_driver_mgmt_init())
		goto cleanup;

	if (ftl_driver_mgmt_init())
		goto cleanup;

	return 0;

cleanup:
	ecc_driver_mgmt_deinit();
	bbt_driver_mgmt_deinit();
	ftl_driver_mgmt_deinit();

	return -1;
}

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		if (ufprog_nand_core_init())
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
	if (ufprog_nand_core_init())
		exit(1);
}
#endif
