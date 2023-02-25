/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * CH34x vendor library routine
 */

#include <stdbool.h>
#include <ufprog/log.h>
#include "ch34x-dll.h"

#ifdef _WIN64
#define CH341DLL_NAME			"ch341dlla64.dll"
#define CH347DLL_NAME			"ch347dlla64.dll"
#else
#define CH341DLL_NAME			"ch341dll.dll"
#define CH347DLL_NAME			"ch347dll.dll"
#endif

fn_CH34xOpenDevice CH34xOpenDevice;
fn_CH347CloseDevice CH347CloseDevice;
fn_CH341CloseDevice CH341CloseDevice;
fn_CH347GetDeviceInfor CH347GetDeviceInfor;
fn_CH34xReadData CH34xReadData;
fn_CH34xWriteData CH34xWriteData;
fn_CH34xSetTimeout CH34xSetTimeout;

static module_handle hCh34xDll;

static struct symbol_find_entry ch341_dll_symbols[] = {
	FIND_MODULE("CH341OpenDevice", CH34xOpenDevice),
	FIND_MODULE("CH341CloseDevice", CH341CloseDevice),
	FIND_MODULE("CH341ReadData", CH34xReadData),
	FIND_MODULE("CH341WriteData", CH34xWriteData),
	FIND_MODULE("CH341SetTimeout", CH34xSetTimeout),
};

static struct symbol_find_entry ch347_dll_symbols[] = {
	FIND_MODULE("CH347OpenDevice", CH34xOpenDevice),
	FIND_MODULE("CH347CloseDevice", CH347CloseDevice),
	FIND_MODULE("CH347GetDeviceInfor", CH347GetDeviceInfor),
	FIND_MODULE("CH347ReadData", CH34xReadData),
	FIND_MODULE("CH347WriteData", CH34xWriteData),
	FIND_MODULE("CH347SetTimeout", CH34xSetTimeout),
};

int ch341_dll_init(void)
{
	bool ch341dll_exists = false;
	ufprog_status ret;

	ret = os_load_module(CH341DLL_NAME, &hCh34xDll);
	if (ret) {
		if (ret == UFP_FILE_NOT_EXIST) {
			logm_dbg("%s does not exist\n", CH341DLL_NAME);
		} else {
			logm_err("Failed to loaded %s\n", CH341DLL_NAME);
			ch341dll_exists = true;
		}

		ret = os_load_module(CH347DLL_NAME, &hCh34xDll);
		if (ret) {
			if (ret == UFP_FILE_NOT_EXIST) {
				if (ch341dll_exists)
					logm_err("Failed to loaded %s\n", CH347DLL_NAME);
				else
					logm_err("Neither %s nor %s exist\n", CH341DLL_NAME, CH347DLL_NAME);
			} else {
				logm_err("Failed to loaded %s\n", CH347DLL_NAME);
			}

			return -1;
		} else {
			logm_dbg("Loaded %s\n", CH347DLL_NAME);
		}
	} else {
		logm_dbg("Loaded %s\n", CH341DLL_NAME);
	}

	ret = os_find_module_symbols(hCh34xDll, ch341_dll_symbols, ARRAY_SIZE(ch341_dll_symbols), true);
	if (ret) {
		logm_err("Failed to load all symbols from driver DLL\n");
		os_unload_module(hCh34xDll);
		hCh34xDll = NULL;
		return -1;
	}

	return 0;
}

int ch347_dll_init(void)
{
	ufprog_status ret;

	ret = os_load_module(CH347DLL_NAME, &hCh34xDll);
	if (ret) {
		if (ret == UFP_FILE_NOT_EXIST)
			logm_err("%s does not exist\n", CH347DLL_NAME);
		else
			logm_err("Unable to load %s\n", CH347DLL_NAME);

		return -1;
	}

	logm_dbg("Loaded %s\n", CH347DLL_NAME);

	ret = os_find_module_symbols(hCh34xDll, ch347_dll_symbols, ARRAY_SIZE(ch347_dll_symbols), true);
	if (ret) {
		logm_err("Failed to load all symbols from driver DLL\n");
		os_unload_module(hCh34xDll);
		hCh34xDll = NULL;
		return -1;
	}

	return 0;
}

void ch34x_dll_deinit(void)
{
	if (hCh34xDll) {
		os_unload_module(hCh34xDll);
		hCh34xDll = NULL;
	}
}

static ufprog_status ch34x_write(struct ch34x_handle *handle, const void *buf, size_t len, size_t *retlen)
{
	ULONG iolen = (ULONG)len;

	if (len > CH34X_MAX_PACKET_SIZE)
		return UFP_INVALID_PARAMETER;

	if (!CH34xWriteData(handle->iIndex, (PVOID)buf, &iolen)) {
		logm_err("CH34xWriteData() failed\n");
		return UFP_DEVICE_IO_ERROR;
	}

	if (retlen)
		*retlen = iolen;

	return UFP_OK;
}

static ufprog_status ch34x_read(struct ch34x_handle *handle, void *buf, size_t len, size_t *retlen)
{
	ULONG iolen = (ULONG)len;

	if (len > CH34X_MAX_PACKET_SIZE)
		return UFP_INVALID_PARAMETER;

	if (!CH34xReadData(handle->iIndex, buf, &iolen)) {
		logm_err("CH34xReadData() failed\n");
		return UFP_DEVICE_IO_ERROR;
	}

	if (retlen)
		*retlen = iolen;

	return UFP_OK;
}

ufprog_status ch341_write(struct ch34x_handle *handle, const void *buf, size_t len, size_t *retlen)
{
	return ch34x_write(handle, buf, len, retlen);
}

ufprog_status ch347_write(struct ch34x_handle *handle, const void *buf, size_t len, size_t *retlen)
{
	return ch34x_write(handle, buf, len, retlen);
}

ufprog_status ch341_read(struct ch34x_handle *handle, void *buf, size_t len, size_t *retlen)
{
	return ch34x_read(handle, buf, len, retlen);
}

ufprog_status ch347_read(struct ch34x_handle *handle, void *buf, size_t len, size_t *retlen)
{
	return ch34x_read(handle, buf, len, retlen);
}
