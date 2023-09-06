/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash opcode definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NAND_OPCODE_H_
#define _UFPROG_SPI_NAND_OPCODE_H_

/* Reset */
#define SNAND_CMD_RESET				0xff

/* Read ID */
#define SNAND_CMD_READID			0x9f

/* Get/set feature */
#define SNAND_CMD_GET_FEATURE			0x0f
#define SNAND_CMD_SET_FEATURE			0x1f

/* Read */
#define SNAND_CMD_READ_TO_CACHE			0x13
#define SNAND_CMD_READ_FROM_CACHE		0x03
#define SNAND_CMD_FAST_READ_FROM_CACHE		0x0b
#define SNAND_CMD_READ_FROM_CACHE_DUAL_OUT	0x3b
#define SNAND_CMD_READ_FROM_CACHE_QUAD_OUT	0x6b
#define SNAND_CMD_READ_FROM_CACHE_DUAL_IO	0xbb
#define SNAND_CMD_READ_FROM_CACHE_QUAD_IO	0xeb
#define SNAND_CMD_READ_FROM_CACHE_RANDOM	0x30
#define SNAND_CMD_READ_FROM_CACHE_SEQ		0x31
#define SNAND_CMD_READ_FROM_CACHE_END		0x3f

/* Write */
#define SNAND_CMD_WRITE_DISABLE			0x04
#define SNAND_CMD_WRITE_ENABLE			0x06
#define SNAND_CMD_PROGRAM_LOAD			0x02
#define SNAND_CMD_PROGRAM_LOAD_QUAD_IN		0x32
#define SNAND_CMD_PROGRAM_EXECUTE		0x10

/* Erase */
#define SNAND_CMD_BLOCK_ERASE			0xd8

/* Multi-die */
#define SNAND_CMD_SELECT_DIE			0xc2

#endif /* _UFPROG_SPI_NAND_OPCODE_H_ */
