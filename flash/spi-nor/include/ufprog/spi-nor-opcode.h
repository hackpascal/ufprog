/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash opcode definitions
 */
#pragma once

#ifndef _UFPROG_SPI_NOR_OPCODE_H_
#define _UFPROG_SPI_NOR_OPCODE_H_

/* Read ID */
#define SNOR_CMD_READ_ID			0x9f
#define SNOR_CMD_READ_ID_MULTI			0xaf
#define SNOR_CMD_READ_SFDP			0x5a

/* Read registers */
#define SNOR_CMD_READ_SR			0x05
#define SNOR_CMD_READ_CR			0x35
#define SNOR_CMD_READ_SR3			0x15
#define SNOR_CMD_READ_BANK			0x16
#define SNOR_CMD_READ_EAR			0xc8
#define SNOR_CMD_READ_FLAGR			0x70
#define SNOR_CMD_READ_NVCR			0xb5
#define SNOR_CMD_READ_VCR			0x85
#define SNOR_CMD_READ_EVCR			0x65

/* Write registers */
#define SNOR_CMD_WRITE_SR			0x01
#define SNOR_CMD_WRITE_CR			0x31
#define SNOR_CMD_WRITE_SR3			0x11
#define SNOR_CMD_WRITE_BANK			0x17
#define SNOR_CMD_WRITE_EAR			0xc5
#define SNOR_CMD_WRITE_NVCR			0xb1
#define SNOR_CMD_WRITE_VCR			0x81
#define SNOR_CMD_WRITE_EVCR			0x61

/* Read */
#define SNOR_CMD_READ				0x03
#define SNOR_CMD_FAST_READ			0x0b
#define SNOR_CMD_FAST_READ_DUAL_OUT		0x3b
#define SNOR_CMD_FAST_READ_DUAL_IO		0xbb
#define SNOR_CMD_FAST_READ_QUAD_OUT		0x6b
#define SNOR_CMD_FAST_READ_QUAD_IO		0xeb

#define SNOR_CMD_4B_FAST_READ			0x0c
#define SNOR_CMD_4B_FAST_READ_DUAL_OUT		0x3c
#define SNOR_CMD_4B_FAST_READ_DUAL_IO		0xbc
#define SNOR_CMD_4B_FAST_READ_QUAD_OUT		0x6c
#define SNOR_CMD_4B_FAST_READ_QUAD_IO		0xec

/* Write */
#define SNOR_CMD_WRITE_EN			0x06
#define SNOR_CMD_WRITE_DIS			0x04
#define SNOR_CMD_VOLATILE_WRITE_EN		0x50

#define SNOR_CMD_PAGE_PROG			0x02
#define SNOR_CMD_PAGE_PROG_DUAL_IN		0xa2
#define SNOR_CMD_PAGE_PROG_QUAD_IN		0x32

#define SNOR_CMD_4B_PAGE_PROG			0x12
#define SNOR_CMD_4B_PAGE_PROG_QUAD_IN		0x34

/* Erase */
#define SNOR_CMD_SECTOR_ERASE			0x20
#define SNOR_CMD_SECTOR_ERASE_32K		0x52
#define SNOR_CMD_BLOCK_ERASE			0xd8
#define SNOR_CMD_CHIP_ERASE			0xc7

#define SNOR_CMD_4B_SECTOR_ERASE		0x21
#define SNOR_CMD_4B_BLOCK_ERASE			0xdc

/* Enter/Exit 4B mode */
#define SNOR_CMD_EN4B				0xb7
#define SNOR_CMD_EX4B				0xe9

/* Enter/Exit QPI mode */
#define SNOR_CMD_EN_QPI_38H			0x38
#define SNOR_CMD_EX_QPI_FFH			0xff

#define SNOR_CMD_EN_QPI_35H			0x35
#define SNOR_CMD_EX_QPI_F5H			0xf5

/* Reset */
#define SNOR_CMD_RESET_ENABLE			0x66
#define SNOR_CMD_RESET				0x99
#define SNOR_CMD_RESET_F0H			0xf0

/* Per-block protection */
#define SNOR_CMD_GLOBAL_BLOCK_UNLOCK		0x98

/* Multi-die */
#define SNOR_CMD_SELECT_DIE			0xc2

/* Winbond specific */
#define SNOR_CMD_SET_READ_PARAMETERS		0xc0
#define SNOR_CMD_READ_UNIQUE_ID			0x4b
#define SNOR_CMD_PROG_OTP			0x42
#define SNOR_CMD_ERASE_OTP			0x44
#define SNOR_CMD_READ_OTP			0x48

/* Microchip/SST specific */
#define SNOR_CMD_AAI_WP				0xad
#define SNOR_CMD_READ_SID			0x88
#define SNOR_CMD_PROG_SID			0xa5
#define SNOR_CMD_LOCK_SID			0x85
#define SNOR_CMD_WRITE_BPR			0x42

/* GigaDevice specific */
#define SNOR_CMD_GD_4B_SECTOR_ERASE_32K		0x5c
#define SNOR_CMD_GD_HPM				0xa3

/* ISSI/PMC specific */
#define SNOR_CMD_READ_FR			0x48
#define SNOR_CMD_WRITE_FR			0x42
#define SNOR_CMD_READ_READ_PARAMETERS		0x61
#define SNOR_CMD_SET_READ_PARAMETERS_V		0x63
#define SNOR_CMD_SET_READ_PARAMETERS_NV		0x65
#define SNOR_CMD_READ_EXT_READ_PARAMETERS	0x81
#define SNOR_CMD_SET_EXT_READ_PARAMETERS_V	0x83
#define SNOR_CMD_SET_EXT_READ_PARAMETERS_NV	0x85
#define SNOR_CMD_READ_AUTOBOOT_REG		0x14
#define SNOR_CMD_WRITE_AUTOBOOT_REG		0x15
#define SNOR_CMD_WRITE_BANK_NV			0x18
#define SNOR_CMD_READ_DLP_REG			0x41
#define SNOR_CMD_WRITE_DLP_REG_V		0x4a
#define SNOR_CMD_WRITE_DLP_REG_NV		0x43
#define SNOR_CMD_PROG_IRL			0x62
#define SNOR_CMD_ERASE_IRL			0x64
#define SNOR_CMD_READ_IRL			0x68
#define SNOR_CMD_IRP				0xb1
#define SNOR_CMD_IRRD				0x4b
#define SNOR_CMD_PMC_SECTOR_ERASE		0xd7

/* ESMT specific */
#define SNOR_CMD_ENSO				0xb1
#define SNOR_CMD_EXSO				0xc1
#define SNOR_CMD_READ_SCUR			0x2b
#define SNOR_CMD_WRITE_SCUR			0x2f
#define SNOR_CMD_RES				0xab
#define SNOR_CMD_PAGE_PROG_QUAD_IO		0x38

/* EON specific */
#define SNOR_CMD_EON_ENTER_OTP_MODE		0x3a
#define SNOR_CMD_EON_READ_SR3			0x95
#define SNOR_CMD_EON_WRITE_SR3			0xc0
#define SNOR_CMD_EON_READ_SR4			0x85
#define SNOR_CMD_EON_WRITE_SR4			0xc1
#define SNOR_CMD_EON_EN_HIGH_BANK_MODE		0x67
#define SNOR_CMD_EON_EX_HIGH_BANK_MODE		0x98

/* Macronix specific */
#define SNOR_CMD_MXIC_KEY1			0xc3
#define SNOR_CMD_MXIC_KEY2			0xa5
#define SNOR_CMD_MXIC_CHIP_UNPROTECT		0xf3
#define SNOR_CMD_MXIC_READ_CR2			0x71
#define SNOR_CMD_MXIC_WRITE_CR2			0x72
#define SNOR_CMD_4B_PAGE_PROG_QUAD_IO		0x3e
#define SNOR_CMD_4B_SECTOR_ERASE_32K		0x5c

/* Micron specific */
#define SNOR_CMD_MICRON_PAGE_ERASE		0xdb
#define SNOR_CMD_MICRON_READ_OTP		0x4b
#define SNOR_CMD_MICRON_PROG_OTP		0x42
#define SNOR_CMD_PAGE_PROG_DUAL_IO		0xd2
#define SNOR_CMD_CLEAR_FLAGR			0x50

/* Spansion specific */
#define SNOR_CMD_SPANDION_READ_SR2		0x07
#define SNOR_CMD_SPANDION_READ_CR3		0x33
#define SNOR_CMD_READ_AR			0x65
#define SNOR_CMD_WRITE_AR			0x71
#define SNOR_CMD_SECTOR_ERASE_8K		0x40

#endif /* _UFPROG_SPI_NOR_OPCODE_H_ */
