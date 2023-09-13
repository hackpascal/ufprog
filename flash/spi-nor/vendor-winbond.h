/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Winbond SPI-NOR flash definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NOR_VENDOR_WINBOND_H_
#define _UFPROG_SPI_NOR_VENDOR_WINBOND_H_

/* SR3 bits */
#define SR3_WPS					BIT(2)

/* QPI Read Parameters */
#define QPI_READ_DUMMY_CLOCKS_2			0x00
#define QPI_READ_DUMMY_CLOCKS_4			0x10
#define QPI_READ_DUMMY_CLOCKS_6			0x20
#define QPI_READ_DUMMY_CLOCKS_8			0x30

#define QPI_READ_WRAP_LENGTH_8			0x00
#define QPI_READ_WRAP_LENGTH_16			0x01
#define QPI_READ_WRAP_LENGTH_32			0x02
#define QPI_READ_WRAP_LENGTH_64			0x03

#endif /* _UFPROG_SPI_NOR_VENDOR_WINBOND_H_ */
