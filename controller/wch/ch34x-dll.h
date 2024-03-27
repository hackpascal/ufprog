/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * CH34x vendor library routine
 */
#pragma once

#ifndef _CH34X_VENDOR_LIB_H_
#define _CH34X_VENDOR_LIB_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

/* Max xfer size supported by vendor dll */
#define CH34X_MAX_PACKET_SIZE		0x1000

/* Driver interface */
#define CH347_USB_VENDOR		0
#define CH347_USB_HID			2
#define CH347_USB_VCP			3

/* Chip Function Interface Number */
#define CH347_FUNC_UART			0
#define CH347_FUNC_SPI_I2C		1
#define CH347_FUNC_JTAG_I2C		2
#define CH347_FUNC_ALL_IN_ONE		3

#pragma pack(push)
#pragma pack(1)
/* Device Information */
typedef struct _DEV_INFOR {
	UCHAR iIndex;			/* Current index opened */
	UCHAR DevicePath[MAX_PATH];	/* Device path, can be used for CreateFile */
	UCHAR UsbClass;			/* 0:CH347_USB_CH341, 2:CH347_USB_HID, 3:CH347_USB_VCP */
	UCHAR FuncType;			/* 0:CH347_FUNC_UART, 1:CH347_FUNC_SPI_I2C, 2:CH347_FUNC_JTAG_I2C */
	CHAR DeviceID[64];		/* USB\VID_xxxx&PID_xxxx */
	UCHAR ChipMode;			/* Chip Mode, 0:Mode0(UART0/1); 1:Mode1(UART1+SPI+I2C);
					              2:Mode2(HID UART1+SPI+I2C) 3:Mode3(UART1+JTAG+I2C) */
	HANDLE DevHandle;		/* Device handle */
	USHORT BulkOutEndpMaxSize;
	USHORT BulkInEndpMaxSize;
	UCHAR UsbSpeedType;		/* 0:FS, 1:HS, 2:SS */
	UCHAR CH347IfNum;		/* USB interface number: 0:UART, 1:SPI/I2C/JTAG/GPIO */
	UCHAR DataUpEndp;
	UCHAR DataDnEndp;
	CHAR ProductString[64];
	CHAR ManufacturerString[64];
	ULONG WriteTimeout;
	ULONG ReadTimeout;
	CHAR FuncDescStr[64];
	UCHAR FirewareVer;
} mDeviceInforS, *mPDeviceInforS;
#pragma pack(pop)

typedef HANDLE (WINAPI *fn_CH34xOpenDevice)(ULONG iIndex);
typedef BOOL (WINAPI *fn_CH347CloseDevice)(ULONG iIndex);
typedef VOID (WINAPI *fn_CH341CloseDevice)(ULONG iIndex);
typedef BOOL (WINAPI *fn_CH347GetDeviceInfor)(ULONG iIndex, mDeviceInforS *DevInformation);
typedef BOOL (WINAPI *fn_CH34xReadData)(ULONG iIndex, PVOID oBuffer, PULONG ioLength);
typedef BOOL (WINAPI *fn_CH34xWriteData)(ULONG iIndex, PVOID iBuffer, PULONG ioLength);
/* Unit: ms. ULONG_MAX means no timeout */
typedef BOOL (WINAPI *fn_CH34xSetTimeout)(ULONG iIndex, ULONG iWriteTimeout, ULONG iReadTimeout);

extern fn_CH34xOpenDevice CH34xOpenDevice;
extern fn_CH347CloseDevice CH347CloseDevice;
extern fn_CH341CloseDevice CH341CloseDevice;
extern fn_CH347GetDeviceInfor CH347GetDeviceInfor;
extern fn_CH34xReadData CH34xReadData;
extern fn_CH34xWriteData CH34xWriteData;
extern fn_CH34xSetTimeout CH34xSetTimeout;

int ch341_dll_init(void);
int ch347_dll_init(void);
void ch34x_dll_deinit(void);

struct ch34x_handle {
	ULONG iIndex;
};

#endif /* _CH34X_VENDOR_LIB_H_ */
