/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * CH347 library abstraction layer
 * Part of the CH347 definitions is from https://github.com/981213/spi-nand-prog/blob/master/spi-mem/ch347/ch347.h
 */
#pragma once

#ifndef _UFPROG_CH347_H_
#define _UFPROG_CH347_H_

#include <stdint.h>
#include <stdbool.h>
#include <ufprog/osdef.h>
#include <ufprog/config.h>

struct ch34x_handle;

#define CH347_MAX_XFER_LEN			0x1000

#define CH347_SPI_CMD_LEN			3
#define CH347_SPI_MAX_CS			2
#define CH347_SPI_MAX_FREQ			60000000
#define CH347_SPI_MAX_PRESCALER			7

#define CH347_SPI_DFL_PRESCALER			5

/* We use 510B packet size which is tested to be OK */
#define CH347_PACKET_LEN			510

#if CH347_PACKET_LEN > CH347_MAX_XFER_LEN - CH347_SPI_CMD_LEN
#error CH347_PACKET_LEN too large
#endif

/*
 * The time used for write is significantly longer than that for read.
 * While using 512B packet size, writing a 4093B packet using lowest speed uses 1.22s
 * While using 4096B packet size, writing a 4093B packet using lowest speed uses 1.83s
 * As a conclusion, at least 2s is needed for write timeout.
 * We use 10s for both read and write here.
 */
#define CH347_SPI_RW_TIMEOUT			10000

/* SPI_Direction */
#define SPI_Direction_2Lines_FullDuplex		0x0000
#define SPI_Direction_2Lines_RxOnly		0x0400
#define SPI_Direction_1Line_Rx			0x8000
#define SPI_Direction_1Line_Tx			0xC000

/* SPI_Mode */
#define SPI_Mode_Master				0x0104
#define SPI_Mode_Slave				0x0000

/* SPI_DataSize */
#define SPI_DataSize_16b			0x0800
#define SPI_DataSize_8b				0x0000

/* SPI_Clock_Polarity */
#define SPI_CPOL_Low				0x0000
#define SPI_CPOL_High				0x0002

/* SPI_Clock_Phase */
#define SPI_CPHA_1Edge				0x0000
#define SPI_CPHA_2Edge				0x0001

/* SPI_Slave_Select_management */
#define SPI_NSS_Software			0x0200
#define SPI_NSS_Hardware			0x0000

/* SPI_MSB_LSB_transmission */
#define SPI_FirstBit_MSB			0x0000
#define SPI_FirstBit_LSB			0x0080

/* CH347 commands */
#define CH347_CMD_SPI_INIT			0xC0
#define CH347_CMD_SPI_CONTROL			0xC1
#define CH347_CMD_SPI_RD_WR			0xC2
#define CH347_CMD_SPI_BLCK_RD			0xC3
#define CH347_CMD_SPI_BLCK_WR			0xC4
#define CH347_CMD_INFO_RD			0xCA

struct ch347_spi_hw_config {
	uint16_t SPI_Direction;
	uint16_t SPI_Mode;
	uint16_t SPI_DataSize;
	uint16_t SPI_CPOL;
	uint16_t SPI_CPHA;
	uint16_t SPI_NSS;
	uint16_t SPI_BaudRatePrescaler;
	uint16_t SPI_FirstBit;
	uint16_t SPI_CRCPolynomial;
	uint16_t SPI_WriteReadInterval;
	uint8_t SPI_OutDefaultData;
	/*
	 * Miscellaneous settings:
	 * Bit 7: CS0 polarity
	 * Bit 6: CS1 polarity
	 * Bit 5: Enable I2C clock stretching
	 * Bit 4: NACK on last I2C reading
	 * Bit 3-0: reserved
	 */
	uint8_t OtherCfg;

	uint8_t Reserved[4];
};

struct ufprog_interface {
	struct ch34x_handle *handle;

	struct ch347_spi_hw_config spicfg;
	uint32_t spi_cs;

	uint32_t max_payload_len;
	uint8_t iobuf[CH347_MAX_XFER_LEN];

	mutex_handle lock;
};

ufprog_status ch347_init(struct ufprog_interface *wchdev, bool thread_safe);

ufprog_status ch347_write(struct ch34x_handle *handle, const void *buf, size_t len, size_t *retlen);
ufprog_status ch347_read(struct ch34x_handle *handle, void *buf, size_t len, size_t *retlen);

ufprog_status ch347_spi_init(struct ufprog_interface *wchdev, struct json_object *config);

#endif /* _UFPROG_CH347_H_ */
