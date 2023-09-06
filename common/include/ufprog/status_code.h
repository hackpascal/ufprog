/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Status code definitions
 */
#pragma once

#ifndef _UFPROG_STATUS_CODE_H_
#define _UFPROG_STATUS_CODE_H_

#include <stdint.h>

enum ufprog_status {
	UFP_OK = 0,
	UFP_FAIL = 1,
	UFP_INVALID_PARAMETER,
	UFP_UNSUPPORTED,
	UFP_NOMEM,
	UFP_ALREADY_EXIST,
	UFP_NOT_EXIST,
	UFP_TIMEOUT,

	UFP_LOCK_FAIL = 100,

	UFP_FILE_NOT_EXIST = 200,
	UFP_FILE_NAME_INVALID,
	UFP_FILE_READ_FAILURE,
	UFP_FILE_WRITE_FAILURE,

	UFP_JSON_DATA_INVALID = 300,
	UFP_JSON_TYPE_INVALID,
	UFP_JSON_FORMAT_FAILED,

	UFP_MODULE_INIT_FAIL = 400,
	UFP_MODULE_IN_USE,
	UFP_MODULE_MISSING_SYMBOL,

	UFP_DEVICE_MISSING_CONFIG = 500,
	UFP_DEVICE_INVALID_CONFIG,
	UFP_DEVICE_NOT_FOUND,
	UFP_DEVICE_DISCONNECTED,
	UFP_DEVICE_IO_ERROR,
	UFP_DEVICE_IO_CANCELLED,

	UFP_FLASH_NOT_PROBED = 600,
	UFP_FLASH_PART_MISMATCH,
	UFP_FLASH_PART_NOT_RECOGNISED,
	UFP_FLASH_PART_NOT_SPECIFIED,
	UFP_FLASH_ADDRESS_OUT_OF_RANGE,
	UFP_FLASH_PROGRAM_FAILED,
	UFP_FLASH_ERASE_FAILED,

	UFP_CMDARG_INVALID_TYPE = 700,
	UFP_CMDARG_MISSING_VALUE,
	UFP_CMDARG_INVALID_VALUE,

	UFP_DATA_VERIFICATION_FAIL = 800,

	UFP_ECC_CORRECTED = 900,
	UFP_ECC_UNCORRECTABLE,
};

typedef uint32_t ufprog_status;

#define STATUS_CHECK_RET(_exp)				\
	do {						\
		ufprog_status _ret = (_exp);		\
		if (_ret)				\
			return _ret;			\
	} while (0)

#define STATUS_CHECK_GOTO(_exp, _label)			\
	do {						\
		ufprog_status _ret = (_exp);		\
		if (_ret)				\
			goto _label;			\
	} while (0)

#define STATUS_CHECK_GOTO_RET(_exp, _ret, _label)	\
	do {						\
		_ret = (_exp);				\
		if (_ret)				\
			goto _label;			\
	} while (0)

#endif /* _UFPROG_STATUS_CODE_H_ */
