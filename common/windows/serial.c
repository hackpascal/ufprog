/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Windows serial port device operations
 */

#include <ufprog/log.h>
#include <ufprog/osdef.h>
#include <ufprog/serial.h>
#include "win32.h"

#define SERIAL_FIFO_SIZE				1024

struct os_serial_port {
	HANDLE hPort;
	OVERLAPPED olSerialIn, olSerialOut;
	DCB dcbOriginal;
	uint32_t timeout_ms;
};

ufprog_status UFPROG_API serial_port_open(const char *path, serial_port *outdev)
{
	uint32_t pos = 0, port_num;
	WCHAR szPortPath[64];
	serial_port dev;
	HANDLE hPort;
	char *end;

	if (!path || !outdev)
		return UFP_INVALID_PARAMETER;

	if (strlen(path) < 4)
		goto out_invalid_port_name;

	if (!strncmp(path, "\\\\.\\", 4)) {
		if (strlen(path) < 8)
			goto out_invalid_port_name;

		pos = 4;
	}

	if (!strncasecmp(&path[pos], "com", 3) && isdigit(path[pos + 3])) {
		port_num = strtoul(&path[pos + 3], &end, 10);
		if (port_num > 255 || end == &path[pos + 3] || *end)
			goto out_invalid_port_name;
	} else {
		goto out_invalid_port_name;
	}

	StringCchPrintfW(szPortPath, ARRAYSIZE(szPortPath), L"\\\\.\\COM%u", port_num);

	hPort = CreateFileW(szPortPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
			    NULL);
	if (hPort == INVALID_HANDLE_VALUE) {
		log_sys_error_utf8(GetLastError(), "Failed to open serial port %s", szPortPath);
		return UFP_FAIL;
	}

	dev = calloc(1, sizeof(*dev));
	if (!dev) {
		log_err("No memory for serial port device\n");
		CloseHandle(hPort);
		return UFP_NOMEM;
	}

	dev->hPort = hPort;

	dev->dcbOriginal.DCBlength = sizeof(dev->dcbOriginal);
	if (!GetCommState(dev->hPort, &dev->dcbOriginal)) {
		log_sys_error_utf8(GetLastError(), "Failed to get serial port config");
		goto cleanup;
	}

	dev->olSerialIn.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!dev->olSerialIn.hEvent) {
		log_sys_error_utf8(GetLastError(), "Failed to create I/O Overlapped event for serial input");
		goto cleanup;
	}

	dev->olSerialOut.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!dev->olSerialOut.hEvent) {
		log_sys_error_utf8(GetLastError(), "Failed to create I/O Overlapped event for serial output");
		goto cleanup;
	}

	if (!SetupComm(dev->hPort, SERIAL_FIFO_SIZE, SERIAL_FIFO_SIZE)) {
		log_sys_error_utf8(GetLastError(), "Failed to set serial port FIFO size");
		goto cleanup;
	}

	*outdev = dev;

	return UFP_OK;

cleanup:
	CloseHandle(dev->hPort);

	if (dev->olSerialIn.hEvent)
		CloseHandle(dev->olSerialIn.hEvent);

	if (dev->olSerialOut.hEvent)
		CloseHandle(dev->olSerialOut.hEvent);

	free(dev);

	return UFP_FAIL;

out_invalid_port_name:
	log_err("%s is not a valid serial port path\n", path);

	return UFP_INVALID_PARAMETER;
}

ufprog_status UFPROG_API serial_port_close(serial_port dev)
{
	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (dev->hPort != INVALID_HANDLE_VALUE) {
		if (!SetCommState(dev->hPort, &dev->dcbOriginal))
			log_sys_error_utf8(GetLastError(), "Failed to restore serial port config");
	}

	CloseHandle(dev->hPort);

	if (dev->olSerialIn.hEvent)
		CloseHandle(dev->olSerialIn.hEvent);

	if (dev->olSerialOut.hEvent)
		CloseHandle(dev->olSerialOut.hEvent);

	free(dev);

	return UFP_OK;
}

ufprog_status UFPROG_API serial_port_set_config(serial_port dev, const struct serial_port_config *config)
{
	DCB dcb;

	if (!dev || !config)
		return UFP_INVALID_PARAMETER;

	if (config->stop_bits >= __MAX_SERIAL_STOP_BITS || config->parity >= __MAX_SERIAL_PARITY ||
	    (config->data_bits < 5 || config->data_bits > 8))
		return UFP_INVALID_PARAMETER;

	memset(&dcb, 0, sizeof(dcb));

	dcb.DCBlength = sizeof(dcb);
	if (!GetCommState(dev->hPort, &dcb)) {
		log_sys_error_utf8(GetLastError(), "Failed to get serial port config");
		return UFP_FAIL;
	}

	dcb.BaudRate = config->baudrate;
	dcb.ByteSize = config->data_bits;

	switch (config->stop_bits) {
	case SERIAL_STOP_BITS_1P5:
		dcb.StopBits = ONE5STOPBITS;
		break;

	case SERIAL_STOP_BITS_2:
		dcb.StopBits = TWOSTOPBITS;
		break;

	default:
		dcb.StopBits = ONESTOPBIT;
	}

	switch (config->parity) {
	case SERIAL_PARITY_ODD:
		dcb.Parity = ODDPARITY;
		break;

	case SERIAL_PARITY_EVEN:
		dcb.Parity = EVENPARITY;
		break;

	case SERIAL_PARITY_MARK:
		dcb.Parity = MARKPARITY;
		break;

	case SERIAL_PARITY_SPACE:
		dcb.Parity = SPACEPARITY;
		break;

	default:
		dcb.Parity = NOPARITY;
		break;
	}

	if (config->parity != SERIAL_PARITY_NONE)
		dcb.fParity = TRUE;
	else
		dcb.fParity = FALSE;

	if (config->flags & SERIAL_F_DTR_DSR) {
		dcb.fDtrControl = DTR_CONTROL_ENABLE;
		dcb.fOutxDsrFlow = TRUE;
	} else {
		dcb.fDtrControl = DTR_CONTROL_DISABLE;
		dcb.fOutxDsrFlow = FALSE;
	}

	dcb.fDsrSensitivity = FALSE;

	if (config->flags & SERIAL_F_RTS_CTS) {
		dcb.fRtsControl = RTS_CONTROL_ENABLE;
		dcb.fOutxCtsFlow = TRUE;
	} else {
		dcb.fRtsControl = RTS_CONTROL_DISABLE;
		dcb.fOutxCtsFlow = FALSE;
	}

	dcb.fAbortOnError = FALSE;
	dcb.fOutX = FALSE;
	dcb.fInX = FALSE;

	if (!SetCommState(dev->hPort, &dcb)) {
		log_sys_error_utf8(GetLastError(), "Failed to set serial port config");
		return UFP_FAIL;
	}

	dev->timeout_ms = config->timeout_ms;

	return UFP_OK;
}

ufprog_status UFPROG_API serial_port_get_config(serial_port dev, struct serial_port_config *retconfig)
{
	DCB dcb;

	if (!dev || !retconfig)
		return UFP_INVALID_PARAMETER;

	memset(&dcb, 0, sizeof(dcb));

	dcb.DCBlength = sizeof(dcb);
	if (!GetCommState(dev->hPort, &dcb)) {
		log_sys_error_utf8(GetLastError(), "Failed to get serial port config");
		return UFP_FAIL;
	}

	memset(retconfig, 0, sizeof(*retconfig));

	retconfig->baudrate = dcb.BaudRate;
	retconfig->data_bits = dcb.ByteSize;

	switch (dcb.StopBits) {
	case ONE5STOPBITS:
		retconfig->stop_bits = SERIAL_STOP_BITS_1P5;
		break;

	case TWOSTOPBITS:
		retconfig->stop_bits = SERIAL_STOP_BITS_2;
		break;

	default:
		retconfig->stop_bits = SERIAL_STOP_BITS_1;
	}

	if (!dcb.fParity) {
		retconfig->parity = SERIAL_PARITY_NONE;
	} else {
		switch (dcb.Parity) {
		case ODDPARITY:
			retconfig->parity = SERIAL_PARITY_ODD;
			break;

		case EVENPARITY:
			retconfig->parity = SERIAL_PARITY_EVEN;
			break;

		case MARKPARITY:
			retconfig->parity = SERIAL_PARITY_MARK;
			break;

		case SPACEPARITY:
			retconfig->parity = SERIAL_PARITY_SPACE;
			break;

		default:
			retconfig->parity = SERIAL_PARITY_NONE;
		}
	}

	if (dcb.fDtrControl != DTR_CONTROL_DISABLE || dcb.fOutxDsrFlow)
		retconfig->flags |= SERIAL_F_DTR_DSR;

	if (dcb.fRtsControl != RTS_CONTROL_DISABLE || dcb.fOutxCtsFlow)
		retconfig->flags |= SERIAL_F_RTS_CTS;

	retconfig->timeout_ms = dev->timeout_ms;

	return UFP_OK;
}

ufprog_status UFPROG_API serial_port_flush(serial_port dev)
{
	DWORD dwErrors;

	if (!dev)
		return UFP_INVALID_PARAMETER;

	if (!ClearCommError(dev->hPort, &dwErrors, NULL)) {
		log_sys_error_utf8(GetLastError(), "Failed to clear serial port error");
		return UFP_FAIL;
	}

	if (!PurgeComm(dev->hPort, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR)) {
		log_sys_error_utf8(GetLastError(), "Failed to flush serial port");
		return UFP_FAIL;
	}

	return UFP_OK;
}

static ufprog_status serial_port_read_once(serial_port dev, void *data, uint32_t len, uint32_t *retlen)
{
	DWORD dwBytesRead, dwResult = 0, dwErrors;
	COMMTIMEOUTS ctOld, ctNew;
	BOOL bResult;

	if (retlen)
		*retlen = 0;

	if (dev->timeout_ms) {
		if (!GetCommTimeouts(dev->hPort, &ctOld)) {
			log_sys_error_utf8(GetLastError(), "Failed to get current serial port timeout");
			return UFP_FAIL;
		}

		memset(&ctNew, 0, sizeof(ctNew));
		ctNew.ReadTotalTimeoutMultiplier = dev->timeout_ms / len;
		ctNew.ReadTotalTimeoutConstant = dev->timeout_ms % len;

		if (!SetCommTimeouts(dev->hPort, &ctNew)) {
			log_sys_error_utf8(GetLastError(), "Failed to set new serial port timeout");
			return UFP_FAIL;
		}
	}

	bResult = ReadFile(dev->hPort, data, len, NULL, &dev->olSerialIn);
	if (!bResult) {
		if (GetLastError() != ERROR_IO_PENDING) {
			log_sys_error_utf8(GetLastError(), "Failed to issue read for serial port");
			return UFP_FAIL;
		}

		dwResult = WaitForSingleObject(dev->olSerialIn.hEvent, INFINITE);
		if (dwResult == WAIT_FAILED) {
			log_sys_error_utf8(GetLastError(), "Failed to wait for serial port read compete");
			return UFP_FAIL;
		}

		if (dwResult == WAIT_TIMEOUT)
			return UFP_TIMEOUT;
	}

	bResult = GetOverlappedResult(dev->hPort, &dev->olSerialIn, &dwBytesRead, FALSE);
	if (!bResult) {
		log_sys_error_utf8(GetLastError(), "Failed to get Overlapped result for reading serial port");
		return UFP_FAIL;
	}

	if (retlen)
		*retlen = dwBytesRead;

	if (!ResetEvent(dev->olSerialIn.hEvent)) {
		log_sys_error_utf8(GetLastError(), "Failed to reset Overlapped event for reading serial port");
		return UFP_FAIL;
	}

	if (dev->timeout_ms) {
		if (!SetCommTimeouts(dev->hPort, &ctOld)) {
			log_sys_error_utf8(GetLastError(), "Failed to restore serial port timeout");
			return UFP_FAIL;
		}
	}

	if (!ClearCommError(dev->hPort, &dwErrors, NULL)) {
		log_sys_error_utf8(GetLastError(), "Failed to clear serial port error");
		return UFP_FAIL;
	}

	if (dwErrors & CE_RXOVER)
		log_err("Serial port RX buffer overrun!\n");

	return UFP_OK;
}

ufprog_status UFPROG_API serial_port_read(serial_port dev, void *data, size_t len, size_t *retlen)
{
	ufprog_status ret = UFP_OK;
	uint32_t read_len, chklen;
	size_t total_len = 0;
	uint8_t *p = data;

	if (!dev || !data || !len)
		return UFP_INVALID_PARAMETER;

	while (len) {
		if (len > MAXDWORD)
			chklen = MAXDWORD;
		else
			chklen = (uint32_t)len;

		ret = serial_port_read_once(dev, p, chklen, &read_len);

		total_len += read_len;
		len -= read_len;
		p += read_len;

		if (ret)
			break;
	}

	if (retlen)
		*retlen = total_len;

	return ret;
}

static ufprog_status serial_port_write_once(serial_port dev, const void *data, uint32_t len, uint32_t *retlen)
{
	DWORD dwBytesWritten, dwResult = 0;
	COMMTIMEOUTS ctOld, ctNew;
	BOOL bResult;

	if (retlen)
		*retlen = 0;

	if (dev->timeout_ms) {
		if (!GetCommTimeouts(dev->hPort, &ctOld)) {
			log_sys_error_utf8(GetLastError(), "Failed to get current serial port timeout");
			return UFP_FAIL;
		}

		memset(&ctNew, 0, sizeof(ctNew));
		ctNew.WriteTotalTimeoutMultiplier = dev->timeout_ms / len;
		ctNew.WriteTotalTimeoutConstant = dev->timeout_ms % len;

		if (!SetCommTimeouts(dev->hPort, &ctNew)) {
			log_sys_error_utf8(GetLastError(), "Failed to set new serial port timeout");
			return UFP_FAIL;
		}
	}

	bResult = WriteFile(dev->hPort, data, len, NULL, &dev->olSerialOut);
	if (!bResult) {
		if (GetLastError() != ERROR_IO_PENDING) {
			log_sys_error_utf8(GetLastError(), "Failed to issue write for serial port");
			return UFP_FAIL;
		}

		dwResult = WaitForSingleObject(dev->olSerialOut.hEvent, INFINITE);
		if (dwResult == WAIT_FAILED) {
			log_sys_error_utf8(GetLastError(), "Failed to wait for serial port write compete");
			return UFP_FAIL;
		}

		if (dwResult == WAIT_TIMEOUT)
			return UFP_TIMEOUT;
	}

	bResult = GetOverlappedResult(dev->hPort, &dev->olSerialOut, &dwBytesWritten, FALSE);
	if (!bResult) {
		log_sys_error_utf8(GetLastError(), "Failed to get Overlapped result for writing serial port");
		return UFP_FAIL;
	}

	if (retlen)
		*retlen = dwBytesWritten;

	if (!ResetEvent(dev->olSerialOut.hEvent)) {
		log_sys_error_utf8(GetLastError(), "Failed to reset Overlapped event for writing serial port");
		return UFP_FAIL;
	}

	if (dev->timeout_ms) {
		if (!SetCommTimeouts(dev->hPort, &ctOld)) {
			log_sys_error_utf8(GetLastError(), "Failed to restore serial port timeout");
			return UFP_FAIL;
		}
	}

	return UFP_OK;
}

ufprog_status UFPROG_API serial_port_write(serial_port dev, const void *data, size_t len, size_t *retlen)
{
	ufprog_status ret = UFP_OK;
	uint32_t write_len, chklen;
	const uint8_t *p = data;
	size_t total_len = 0;

	if (!dev || !data || !len)
		return UFP_INVALID_PARAMETER;

	while (len) {
		if (len > MAXDWORD)
			chklen = MAXDWORD;
		else
			chklen = (uint32_t)len;

		ret = serial_port_write_once(dev, p, chklen, &write_len);

		total_len += write_len;
		len -= write_len;
		p += write_len;

		if (ret)
			break;
	}

	if (retlen)
		*retlen = total_len;

	return ret;
}
