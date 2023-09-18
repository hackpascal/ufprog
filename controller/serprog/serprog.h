/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Serprog definitions
 */
#pragma once

#ifndef _UFPROG_SERPROG_H_
#define _UFPROG_SERPROG_H_

#include <stdint.h>
#include <stdbool.h>
#include <ufprog/osdef.h>
#include <ufprog/config.h>
#include <ufprog/serial.h>

#define SERPROG_DEFAULT_BAUDRATE			115200
#define SERPROG_DEFAULT_DATA_BITS			8
#define SERPROG_DEFAULT_TIMEOUT_MS			5000

#define SERPROG_MAX_BUFFER_SIZE				0xffffff

#define S_ACK						0x06
#define S_NAK						0x15

#define S_CMD_NOP					0x00	/* No operation					*/
#define S_CMD_Q_IFACE					0x01	/* Query interface version			*/
#define S_CMD_Q_CMDMAP					0x02	/* Query supported commands bitmap		*/
#define S_CMD_Q_PGMNAME					0x03	/* Query programmer name			*/
#define S_CMD_Q_BUSTYPE					0x05	/* Query supported bustypes			*/
#define S_CMD_SYNCNOP					0x10	/* Special no-operation that returns NAK+ACK	*/
#define S_CMD_S_BUSTYPE					0x12	/* Set used bustype(s).				*/
#define S_CMD_O_SPIOP					0x13	/* Perform SPI operation.			*/
#define S_CMD_S_SPI_FREQ				0x14	/* Set SPI clock frequency			*/
#define S_CMD_S_PIN_STATE				0x15	/* Enable/disable output drivers		*/

#define BUS_SPI						BIT(3)

struct ufprog_interface {
	const char *path;
	serial_port port;

	uint32_t buffer_size;
	uint32_t timeout_ms;
	uint32_t max_spi_freq;
	uint32_t min_spi_freq;
	uint32_t curr_spi_freq;

	mutex_handle lock;
};

ufprog_status serprog_spi_init(struct ufprog_interface *dev);

#endif /* _UFPROG_SERPROG_H_ */
