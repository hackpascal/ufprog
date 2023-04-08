/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * FTDI D2XX library routine
 */

#include <stdlib.h>
#include <stdbool.h>
#include <ufprog/config.h>
#include <ufprog/osdef.h>
#include <ufprog/log.h>
#include "d2xx.h"
#include "ftdi.h"

fn_FT_OpenEx FT_OpenEx;
fn_FT_Close FT_Close;
fn_FT_GetDeviceInfo FT_GetDeviceInfo;
fn_FT_VendorCmdGet FT_VendorCmdGet;
fn_FT_VendorCmdSet FT_VendorCmdSet;
fn_FT_VendorCmdGetEx FT_VendorCmdGetEx;
fn_FT_VendorCmdSetEx FT_VendorCmdSetEx;
fn_FT_GetStatus FT_GetStatus;
fn_FT_Read FT_Read;
fn_FT_Write FT_Write;
fn_FT_Purge FT_Purge;
fn_FT_ResetDevice FT_ResetDevice;
fn_FT_SetLatencyTimer FT_SetLatencyTimer;
fn_FT_GetLatencyTimer FT_GetLatencyTimer;
fn_FT_SetBitMode FT_SetBitMode;
fn_FT_GetBitMode FT_GetBitMode;

static module_handle hD2xx;

static struct symbol_find_entry d2xx_symbols[] = {
	FIND_MODULE("FT_OpenEx", FT_OpenEx),
	FIND_MODULE("FT_Close", FT_Close),
	FIND_MODULE("FT_GetDeviceInfo", FT_GetDeviceInfo),
	FIND_MODULE("FT_VendorCmdGet", FT_VendorCmdGet),
	FIND_MODULE("FT_VendorCmdSet", FT_VendorCmdSet),
	FIND_MODULE("FT_VendorCmdGetEx", FT_VendorCmdGetEx),
	FIND_MODULE("FT_VendorCmdSetEx", FT_VendorCmdSetEx),
	FIND_MODULE("FT_GetStatus", FT_GetStatus),
	FIND_MODULE("FT_Read", FT_Read),
	FIND_MODULE("FT_Write", FT_Write),
	FIND_MODULE("FT_Purge", FT_Purge),
	FIND_MODULE("FT_ResetDevice", FT_ResetDevice),
	FIND_MODULE("FT_SetLatencyTimer", FT_SetLatencyTimer),
	FIND_MODULE("FT_GetLatencyTimer", FT_GetLatencyTimer),
	FIND_MODULE("FT_SetBitMode", FT_SetBitMode),
	FIND_MODULE("FT_GetBitMode", FT_GetBitMode),
};

int d2xx_init(void)
{
	ufprog_status ret;

	ret = os_load_module("ftd2xx.dll", &hD2xx);
	if (ret) {
		if (ret == UFP_FILE_NOT_EXIST)
			logm_err("%s does not exist\n", "ftd2xx.dll");
		else
			logm_err("Failed to load %s\n", "ftd2xx.dll");

		return -1;
	}

	ret = os_find_module_symbols(hD2xx, d2xx_symbols, ARRAY_SIZE(d2xx_symbols), true);
	if (ret) {
		logm_err("Failed to load symbols from %s\n", "ftd2xx.dll");
		os_unload_module(hD2xx);
		hD2xx = NULL;
		return -1;
	}

	return 0;
}

void d2xx_deinit(void)
{
	if (hD2xx) {
		os_unload_module(hD2xx);
		hD2xx = NULL;
	}
}

static ufprog_status d2xx_status_translate(FT_STATUS ftStatus)
{
	switch (ftStatus) {
	case FT_OK:
		return UFP_OK;

	case FT_DEVICE_NOT_FOUND:
		return UFP_DEVICE_DISCONNECTED;

	case FT_IO_ERROR:
	case FT_FAILED_TO_WRITE_DEVICE:
		return UFP_DEVICE_IO_ERROR;

	default:
		return UFP_FAIL;
	}
}

ufprog_status ftdi_reset(struct ft_handle *handle)
{
	FT_STATUS ftStatus;

	ftStatus = FT_ResetDevice(handle->ftHandle);

	return d2xx_status_translate(ftStatus);
}

ufprog_status ftdi_purge_all(struct ft_handle *handle)
{
	FT_STATUS ftStatus;

	ftStatus = FT_Purge(handle->ftHandle, FT_PURGE_RX | FT_PURGE_TX);

	return d2xx_status_translate(ftStatus);
}

ufprog_status ftdi_read(struct ft_handle *handle, void *buf, size_t len)
{
	DWORD dwBytesToRead, dwBytesRead, dwRxBytes = 0, dwTxBytes = 0, dwEventDWord = 0;
	FT_STATUS ftStatus;
	UCHAR *p;

	p = (UCHAR *)buf;

	while (len) {
		ftStatus = FT_GetStatus(handle->ftHandle, &dwRxBytes, &dwTxBytes, &dwEventDWord);
		if (ftStatus) {
			logm_err("FT_GetStatus() failed with %u\n", ftStatus);
			return d2xx_status_translate(ftStatus);
		}

		/* FIXME: implement transfer cancellation */
		if (!dwRxBytes)
			continue;

		dwBytesToRead = dwRxBytes;
		if (dwBytesToRead > len)
			dwBytesToRead = (DWORD)len;

		ftStatus = FT_Read(handle->ftHandle, p, dwBytesToRead, &dwBytesRead);
		if (ftStatus) {
			logm_err("FT_Read() failed with %lu\n", ftStatus);
			return UFP_DEVICE_IO_ERROR;
		}

		if (dwBytesToRead != dwBytesRead) {
			logm_err("FT_Read() insufficient read %uB of %uB\n", dwBytesRead, dwBytesToRead);
			return UFP_DEVICE_IO_ERROR;
		}

		len -= dwBytesRead;
		p += dwBytesRead;
	}

	return UFP_OK;
}

ufprog_status ftdi_write(struct ft_handle *handle, const void *buf, size_t len)
{
	DWORD dwBytesToWrite, dwBytesWritten = 0;
	FT_STATUS ftStatus;
	UCHAR *p;

	p = (UCHAR *)buf;

	do {
		if (len > MAXDWORD)
			dwBytesToWrite = MAXDWORD;
		else
			dwBytesToWrite = (DWORD)len;

		ftStatus = FT_Write(handle->ftHandle, p, dwBytesToWrite, &dwBytesWritten);
		if (ftStatus) {
			logm_err("FT_Write() failed with %lu\n", ftStatus);
			return d2xx_status_translate(ftStatus);
		}

		if (dwBytesWritten != dwBytesToWrite) {
			logm_err("FT_Write() insufficient write %uB of %uB\n", dwBytesWritten, dwBytesToWrite);
			return UFP_DEVICE_IO_ERROR;
		}

		len -= dwBytesWritten;
		p += dwBytesWritten;
	} while (len);

	return UFP_OK;
}

ufprog_status ftdi_vendor_cmd_get(struct ft_handle *handle, uint8_t request, void *buf, uint16_t len)
{
	FT_STATUS ftStatus;

	ftStatus = FT_VendorCmdGet(handle->ftHandle, request, buf, len);

	return d2xx_status_translate(ftStatus);
}

ufprog_status ftdi_vendor_cmd_set(struct ft_handle *handle, uint8_t request, const void *buf, uint16_t len)
{
	FT_STATUS ftStatus;

	ftStatus = FT_VendorCmdSet(handle->ftHandle, request, (UCHAR *)buf, len);

	return d2xx_status_translate(ftStatus);
}

ufprog_status ftdi_set_latency_timer(struct ft_handle *handle, uint8_t latency_ms)
{
	FT_STATUS ftStatus;

	ftStatus = FT_SetLatencyTimer(handle->ftHandle, latency_ms);

	return d2xx_status_translate(ftStatus);
}

ufprog_status ftdi_get_latency_timer(struct ft_handle *handle, uint8_t *platency_ms)
{
	FT_STATUS ftStatus;

	ftStatus = FT_GetLatencyTimer(handle->ftHandle, platency_ms);

	return d2xx_status_translate(ftStatus);
}

ufprog_status ftdi_set_bit_mode(struct ft_handle *handle, uint8_t mask, uint8_t mode)
{
	FT_STATUS ftStatus;

	ftStatus = FT_SetBitMode(handle->ftHandle, mask, mode);

	return d2xx_status_translate(ftStatus);
}

ufprog_status ftdi_get_bit_mode(struct ft_handle *handle, uint8_t *pmode)
{
	FT_STATUS ftStatus;

	ftStatus = FT_GetBitMode(handle->ftHandle, pmode);

	return d2xx_status_translate(ftStatus);
}

ufprog_status ftdi_get_mpsse_chip(struct ft_handle *handle, enum ftdi_mpsse_chip *chip)
{
	char SerialNumber[16], Description[64];
	FT_DEVICE ftDevice;
	FT_STATUS ftStatus;
	DWORD dwID;

	ftStatus = FT_GetDeviceInfo(handle->ftHandle, &ftDevice, &dwID, SerialNumber, Description, NULL);
	if (ftStatus)
		return d2xx_status_translate(ftStatus);

	switch (ftDevice) {
	case FT_DEVICE_2232C:
		*chip = FT2232C;
		break;
	case FT_DEVICE_2232H:
		*chip = FT2232H;
		break;
	case FT_DEVICE_4232H:
		*chip = FT4232H;
		break;
	case FT_DEVICE_232H:
		*chip = FT232H;
		break;
	default:
		return UFP_UNSUPPORTED;
	}

	return UFP_OK;
}

int UFPROG_API ftdi_d2xx_try_match_open(void *priv, struct json_object *match, int index)
{
	FT_HANDLE ftHandle = NULL, *phandle = priv;
	const char *open_type, *open_arg;
	FT_STATUS ftStatus;
	ufprog_status ret;
	char *end;

	ret = json_read_str(match, "type", &open_type, NULL);
	if (ret) {
		if (index >= 0)
			logm_warn("Invalid device open type in match#%u\n", index);
		else
			logm_warn("Invalid device open type in matching data\n");
		return 0;
	}

	ret = json_read_str(match, "value", &open_arg, NULL);
	if (ret) {
		if (index >= 0)
			logm_warn("Invalid device open argument value in match#%u\n", index);
		else
			logm_warn("Invalid device open argument value in matching data\n");
		return 0;
	}

	if (!strcmp(open_type, "serial")) {
		ftStatus = FT_OpenEx((PVOID)open_arg, FT_OPEN_BY_SERIAL_NUMBER, &ftHandle);
	} else if (!strcmp(open_type, "description")) {
		ftStatus = FT_OpenEx((PVOID)open_arg, FT_OPEN_BY_DESCRIPTION, &ftHandle);
	} else if (!strcmp(open_type, "location")) {
		ULONG ulLoc;

		ulLoc = strtoul(open_arg, &end, 0);
		if (ulLoc == ULONG_MAX || *end) {
			if (index >= 0)
				logm_warn("Invalid location id in match#%u\n",index);
			else
				logm_warn("Invalid location id in matching data\n");
			return 0;
		}

		ftStatus = FT_OpenEx((PVOID)(uintptr_t)ulLoc, FT_OPEN_BY_LOCATION, &ftHandle);
	} else {
		if (index >= 0)
			logm_warn("Invalid device open type in match#%u\n", index);
		else
			logm_warn("Invalid device open type in matching data\n");
		return 0;
	}

	if (ftStatus != FT_OK) {
		logm_dbg("Failed to open device with '%s': '%s'\n", open_type, open_arg);
		return 0;
	}

	logm_info("Opened device with '%s': '%s'\n", open_type, open_arg);

	*phandle = ftHandle;

	return 1;
}
