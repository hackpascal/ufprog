/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Serial port device operations
 */
#pragma once

#ifndef _UFPROG_SERIAL_H_
#define _UFPROG_SERIAL_H_

#include <ufprog/common.h>
#include <ufprog/bits.h>

EXTERN_C_BEGIN

typedef struct os_serial_port *serial_port;

enum serial_stop_bits {
	SERIAL_STOP_BITS_1,
	SERIAL_STOP_BITS_1P5,
	SERIAL_STOP_BITS_2,

	__MAX_SERIAL_STOP_BITS
};

enum serial_parity_config {
	SERIAL_PARITY_NONE,
	SERIAL_PARITY_ODD,
	SERIAL_PARITY_EVEN,
	SERIAL_PARITY_MARK,
	SERIAL_PARITY_SPACE,

	__MAX_SERIAL_PARITY
};

enum serial_fc_config {
	SERIAL_FC_DTR_DSR,
	SERIAL_FC_RTS_CTS,
	SERIAL_FC_XON_XOFF,

	__MAX_SERIAL_FCY
};

struct serial_port_config {
	uint32_t flags;
	uint32_t baudrate;
	uint32_t timeout_ms;
	uint8_t data_bits;
	uint8_t /* enum serial_stop_bits */ stop_bits;
	uint8_t /* enum serial_parity_config */ parity;
	uint8_t /* enum serial_fc_config */ fc;
	uint8_t xon;
	uint8_t xoff;
};

ufprog_status UFPROG_API serial_port_open(const char *path, serial_port *outdev);
ufprog_status UFPROG_API serial_port_close(serial_port dev);
ufprog_status UFPROG_API serial_port_set_config(serial_port dev, const struct serial_port_config *config);
ufprog_status UFPROG_API serial_port_get_config(serial_port dev, struct serial_port_config *retconfig);
ufprog_status UFPROG_API serial_port_flush(serial_port dev);
ufprog_status UFPROG_API serial_port_read(serial_port dev, void *data, size_t len, size_t *retlen);
ufprog_status UFPROG_API serial_port_write(serial_port dev, const void *data, size_t len, size_t *retlen);

EXTERN_C_END

#endif /* _UFPROG_SERIAL_H_ */
