/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * FTDI D2XX library routine
 */
#pragma once

#ifndef _UFPROG_D2XX_LIB_H_
#define _UFPROG_D2XX_LIB_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

typedef PVOID	FT_HANDLE;
typedef ULONG	FT_STATUS;
typedef ULONG	FT_DEVICE;

enum {
	FT_OK,
	FT_INVALID_HANDLE,
	FT_DEVICE_NOT_FOUND,
	FT_DEVICE_NOT_OPENED,
	FT_IO_ERROR,
	FT_INSUFFICIENT_RESOURCES,
	FT_INVALID_PARAMETER,
	FT_INVALID_BAUD_RATE,

	FT_DEVICE_NOT_OPENED_FOR_ERASE,
	FT_DEVICE_NOT_OPENED_FOR_WRITE,
	FT_FAILED_TO_WRITE_DEVICE,
	FT_EEPROM_READ_FAILED,
	FT_EEPROM_WRITE_FAILED,
	FT_EEPROM_ERASE_FAILED,
	FT_EEPROM_NOT_PRESENT,
	FT_EEPROM_NOT_PROGRAMMED,
	FT_INVALID_ARGS,
	FT_NOT_SUPPORTED,
	FT_OTHER_ERROR,
	FT_DEVICE_LIST_NOT_READY,
};

enum {
	FT_DEVICE_BM,
	FT_DEVICE_AM,
	FT_DEVICE_100AX,
	FT_DEVICE_UNKNOWN,
	FT_DEVICE_2232C,
	FT_DEVICE_232R,
	FT_DEVICE_2232H,
	FT_DEVICE_4232H,
	FT_DEVICE_232H,
	FT_DEVICE_X_SERIES,
	FT_DEVICE_4222H_0,
	FT_DEVICE_4222H_1_2,
	FT_DEVICE_4222H_3,
	FT_DEVICE_4222_PROG,
	FT_DEVICE_900,
	FT_DEVICE_UMFTPD3A,
};

#define FT_OPEN_BY_SERIAL_NUMBER	1
#define FT_OPEN_BY_DESCRIPTION		2
#define FT_OPEN_BY_LOCATION		4

#define FT_PURGE_RX			1
#define FT_PURGE_TX			2

typedef FT_STATUS (WINAPI *fn_FT_OpenEx)(PVOID pArg1, DWORD Flags, FT_HANDLE *pHandle);

typedef FT_STATUS (WINAPI *fn_FT_Close)(FT_HANDLE ftHandle);

typedef FT_STATUS (WINAPI *fn_FT_GetDeviceInfo)( FT_HANDLE ftHandle, FT_DEVICE *lpftDevice, LPDWORD lpdwID,
						PCHAR SerialNumber, PCHAR Description, LPVOID Dummy);

typedef FT_STATUS (WINAPI *fn_FT_VendorCmdGet)(FT_HANDLE ftHandle, UCHAR Request, UCHAR *Buf,
					       USHORT Len);

typedef FT_STATUS (WINAPI *fn_FT_VendorCmdSet)(FT_HANDLE ftHandle, UCHAR Request, UCHAR *Buf,
					       USHORT Len);

typedef FT_STATUS (WINAPI *fn_FT_VendorCmdGetEx)(FT_HANDLE ftHandle, USHORT wValue, UCHAR *Buf,
						 USHORT Len);

typedef FT_STATUS (WINAPI *fn_FT_VendorCmdSetEx)(FT_HANDLE ftHandle, USHORT wValue, UCHAR *Buf,
						 USHORT Len);

typedef FT_STATUS (WINAPI *fn_FT_GetStatus)(FT_HANDLE ftHandle, DWORD *dwRxBytes,
					    DWORD *dwTxBytes,DWORD *dwEventDWord);

typedef FT_STATUS (WINAPI *fn_FT_Read)(FT_HANDLE ftHandle, LPVOID lpBuffer, DWORD dwBytesToRead,
				       LPDWORD lpBytesReturned);

typedef FT_STATUS (WINAPI *fn_FT_Write)(FT_HANDLE ftHandle, LPVOID lpBuffer,
					DWORD dwBytesToWrite,LPDWORD lpBytesWritten);

typedef FT_STATUS (WINAPI *fn_FT_Purge)(FT_HANDLE ftHandle, ULONG Mask);

typedef FT_STATUS (WINAPI *fn_FT_ResetDevice)(FT_HANDLE ftHandle);

typedef FT_STATUS (WINAPI *fn_FT_SetLatencyTimer)(FT_HANDLE ftHandle, UCHAR ucLatency);

typedef FT_STATUS (WINAPI *fn_FT_GetLatencyTimer)(FT_HANDLE ftHandle, PUCHAR pucLatency);

typedef FT_STATUS (WINAPI *fn_FT_SetBitMode)(FT_HANDLE ftHandle, UCHAR ucMask, UCHAR ucEnable);

typedef FT_STATUS (WINAPI *fn_FT_GetBitMode)( FT_HANDLE ftHandle, PUCHAR pucMode);

extern fn_FT_OpenEx FT_OpenEx;
extern fn_FT_Close FT_Close;
extern fn_FT_GetDeviceInfo FT_GetDeviceInfo;
extern fn_FT_VendorCmdGet FT_VendorCmdGet;
extern fn_FT_VendorCmdSet FT_VendorCmdSet;
extern fn_FT_VendorCmdGetEx FT_VendorCmdGetEx;
extern fn_FT_VendorCmdSetEx FT_VendorCmdSetEx;
extern fn_FT_GetStatus FT_GetStatus;
extern fn_FT_Read FT_Read;
extern fn_FT_Write FT_Write;
extern fn_FT_Purge FT_Purge;
extern fn_FT_ResetDevice FT_ResetDevice;
extern fn_FT_SetLatencyTimer FT_SetLatencyTimer;
extern fn_FT_GetLatencyTimer FT_GetLatencyTimer;
extern fn_FT_SetBitMode FT_SetBitMode;
extern fn_FT_GetBitMode FT_GetBitMode;

int d2xx_init(void);
void d2xx_deinit(void);

int UFPROG_API ftdi_d2xx_try_match_open(void *priv, struct json_object *match, int index);

struct ft_handle {
	FT_HANDLE ftHandle;
};

#endif /* _UFPROG_D2XX_LIB_H_ */
