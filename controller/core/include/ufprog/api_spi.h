/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Controller SPI interface definitions
 */
#pragma once

#ifndef _UFPROG_API_SPI_H_
#define _UFPROG_API_SPI_H_

#include <ufprog/api_controller.h>

EXTERN_C_BEGIN

/* SPI interface version */
#define UFPROG_SPI_IF_MAJOR			1
#define UFPROG_SPI_IF_MINOR			0

/* Generic SPI interface capabilities */
#define UFP_SPI_GEN_DUAL			BIT(0)
#define UFP_SPI_GEN_QUAD			BIT(1)
#define UFP_SPI_GEN_OCTAL			BIT(2)
#define UFP_SPI_GEN_DTR				BIT(3)
#define UFP_SPI_NO_QPI_BULK_READ		BIT(4)

/* SPI modes */
#define SPI_MODE_CPHA				0x01
#define SPI_MODE_CPOL				0x02
#define SPI_MODE_0				0
#define SPI_MODE_1				SPI_MODE_CPHA
#define SPI_MODE_2				SPI_MODE_CPOL
#define SPI_MODE_3				(SPI_MODE_CPOL | SPI_MODE_CPHA)

/* SPI-MEM I/O types */
enum spi_mem_io_type {
	SPI_MEM_IO_1_1_1,
	SPI_MEM_IO_1S_1D_1D,
	SPI_MEM_IO_1D_1D_1D,

	SPI_MEM_IO_1_1_2,
	SPI_MEM_IO_1_2_2,
	SPI_MEM_IO_2_2_2,
	SPI_MEM_IO_1S_2D_2D,
	SPI_MEM_IO_2D_2D_2D,

	SPI_MEM_IO_1_1_4,
	SPI_MEM_IO_1_4_4,
	SPI_MEM_IO_4_4_4,
	SPI_MEM_IO_1S_4D_4D,
	SPI_MEM_IO_4D_4D_4D,

	SPI_MEM_IO_1_1_8,
	SPI_MEM_IO_1_8_8,
	SPI_MEM_IO_8_8_8,
	SPI_MEM_IO_1S_8D_8D,
	SPI_MEM_IO_8D_8D_8D,

	__SPI_MEM_IO_MAX
};

#define BIT_SPI_MEM_IO_1_1_1			BIT(SPI_MEM_IO_1_1_1)
#define BIT_SPI_MEM_IO_1S_1D_1D			BIT(SPI_MEM_IO_1S_1D_1D)
#define BIT_SPI_MEM_IO_1D_1D_1D			BIT(SPI_MEM_IO_1D_1D_1D)
#define BIT_SPI_MEM_IO_1_1_2			BIT(SPI_MEM_IO_1_1_2)
#define BIT_SPI_MEM_IO_1_2_2			BIT(SPI_MEM_IO_1_2_2)
#define BIT_SPI_MEM_IO_2_2_2			BIT(SPI_MEM_IO_2_2_2)
#define BIT_SPI_MEM_IO_1S_2D_2D			BIT(SPI_MEM_IO_1S_2D_2D)
#define BIT_SPI_MEM_IO_2D_2D_2D			BIT(SPI_MEM_IO_2D_2D_2D)
#define BIT_SPI_MEM_IO_1_1_4			BIT(SPI_MEM_IO_1_1_4)
#define BIT_SPI_MEM_IO_1_4_4			BIT(SPI_MEM_IO_1_4_4)
#define BIT_SPI_MEM_IO_4_4_4			BIT(SPI_MEM_IO_4_4_4)
#define BIT_SPI_MEM_IO_1S_4D_4D			BIT(SPI_MEM_IO_1S_4D_4D)
#define BIT_SPI_MEM_IO_4D_4D_4D			BIT(SPI_MEM_IO_4D_4D_4D)
#define BIT_SPI_MEM_IO_1_1_8			BIT(SPI_MEM_IO_1_1_8)
#define BIT_SPI_MEM_IO_1_8_8			BIT(SPI_MEM_IO_1_8_8)
#define BIT_SPI_MEM_IO_8_8_8			BIT(SPI_MEM_IO_8_8_8)
#define BIT_SPI_MEM_IO_1S_8D_8D			BIT(SPI_MEM_IO_1S_8D_8D)
#define BIT_SPI_MEM_IO_8D_8D_8D			BIT(SPI_MEM_IO_8D_8D_8D)

#define BIT_SPI_MEM_IO_X2			(BIT_SPI_MEM_IO_1_1_2 | BIT_SPI_MEM_IO_1_2_2)
#define BIT_SPI_MEM_IO_X4			(BIT_SPI_MEM_IO_1_1_4 | BIT_SPI_MEM_IO_1_4_4)
#define BIT_SPI_MEM_IO_X8			(BIT_SPI_MEM_IO_1_1_8 | BIT_SPI_MEM_IO_1_8_8)

#define BIT_SPI_MEM_IO_DPI			(BIT_SPI_MEM_IO_X2 | BIT_SPI_MEM_IO_2_2_2)
#define BIT_SPI_MEM_IO_QPI			(BIT_SPI_MEM_IO_X4 | BIT_SPI_MEM_IO_4_4_4)
#define BIT_SPI_MEM_IO_OPI			(BIT_SPI_MEM_IO_X8 | BIT_SPI_MEM_IO_8_8_8)

/* SPI data directions (Full-duplex is not needed) */
enum ufprog_spi_data_dir {
	SPI_DATA_IN,
	SPI_DATA_OUT,

	__MAX_SPI_DATA_DIR
};

union ufprog_io_buf {
	void *rx;
	const void *tx;
};

struct ufprog_spi_transfer {
	uint8_t /* enum ufprog_spi_data_dir */ dir;
	uint8_t buswidth;
	ufprog_bool dtr;
	ufprog_bool end;
	uint32_t speed;
	size_t len;
	union ufprog_io_buf buf;
};

struct ufprog_spi_mem_op {
	struct {
		uint8_t len;
		uint8_t buswidth;
		uint8_t dtr;
		uint16_t opcode;
	} cmd;

	struct {
		uint8_t len;
		uint8_t buswidth;
		uint8_t dtr;
		uint64_t val;
	} addr;

	struct {
		uint8_t len;
		uint8_t buswidth;
		uint8_t dtr;
	} dummy;

	struct {
		uint8_t buswidth;
		uint8_t dtr;
		uint8_t /* enum ufprog_spi_data_dir */ dir;
		size_t len;
		union ufprog_io_buf buf;
	} data;
};

#define SPI_MEM_OP(_cmd, _addr, _dummy, _data)			\
	{							\
		.cmd = _cmd,					\
		.addr = _addr,					\
		.dummy = _dummy,				\
		.data = _data,					\
	}

#define SPI_MEM_OP_CMD(_opcode, _buswidth)			\
	{							\
		.buswidth = (_buswidth),			\
		.opcode = (_opcode),				\
		.len = 1,					\
	}

#define SPI_MEM_OP_NO_CMD	{ 0 }

#define SPI_MEM_OP_ADDR(_len, _val, _buswidth)			\
	{							\
		.len = (_len),					\
		.val = (_val),					\
		.buswidth = (_buswidth),			\
	}

#define SPI_MEM_OP_NO_ADDR	{ 0 }

#define SPI_MEM_OP_DUMMY(_len, _buswidth)			\
	{							\
		.len = (_len),					\
		.buswidth = (_buswidth),			\
	}

#define SPI_MEM_OP_NO_DUMMY	{ 0 }

#define SPI_MEM_OP_DATA_IN(_len, _buf, _buswidth)		\
	{							\
		.dir = SPI_DATA_IN,				\
		.len = (_len),					\
		.buf.rx = (_buf),				\
		.buswidth = (_buswidth),			\
	}

#define SPI_MEM_OP_DATA_OUT(_len, _buf, _buswidth)		\
	{							\
		.dir = SPI_DATA_OUT,				\
		.len = (_len),					\
		.buf.tx = (_buf),				\
		.buswidth = (_buswidth),			\
	}

#define SPI_MEM_OP_NO_DATA	{ 0 }

#define API_NAME_SPI_IF_VERSION			"ufprog_spi_if_version"
typedef uint32_t (UFPROG_API *api_spi_if_version)(void);

#define API_NAME_SPI_IF_CAPS			"ufprog_spi_if_caps"
typedef uint32_t (UFPROG_API *api_spi_if_caps)(void);

#define API_NAME_SPI_MAX_READ_GRANULARITY	"ufprog_spi_max_read_granularity"
typedef size_t (UFPROG_API *api_spi_max_read_granularity)(void);

#define API_NAME_SPI_SET_CS_POL			"ufprog_spi_set_cs_pol"
typedef ufprog_status (UFPROG_API *api_spi_set_cs_pol)(struct ufprog_if_dev *ifdev, ufprog_bool positive);

#define API_NAME_SPI_SET_MODE			"ufprog_spi_set_mode"
typedef ufprog_status (UFPROG_API *api_spi_set_mode)(struct ufprog_if_dev *ifdev, uint32_t mode);

#define API_NAME_SPI_SET_SPEED			"ufprog_spi_set_speed"
typedef ufprog_status (UFPROG_API *api_spi_set_speed)(struct ufprog_if_dev *ifdev, uint32_t hz, uint32_t *rethz);

#define API_NAME_SPI_GET_SPEED			"ufprog_spi_get_speed"
typedef uint32_t (UFPROG_API *api_spi_get_speed)(struct ufprog_if_dev *ifdev);

#define API_NAME_SPI_GET_SPEED_RANGE		"ufprog_spi_get_speed_range"
typedef ufprog_status (UFPROG_API *api_spi_get_speed_range)(struct ufprog_if_dev *ifdev, uint32_t *retlowhz,
							    uint32_t *rethighhz);

#define API_NAME_SPI_GET_SPEED_LIST		"ufprog_spi_get_speed_list"
typedef uint32_t (UFPROG_API *api_spi_get_speed_list)(struct ufprog_if_dev *ifdev, uint32_t *retlist, int32_t count);

#define API_NAME_SPI_SET_WP			"ufprog_spi_set_wp"
typedef ufprog_status (UFPROG_API *api_spi_set_wp)(struct ufprog_if_dev *ifdev, ufprog_bool high);

#define API_NAME_SPI_SET_HOLD			"ufprog_spi_set_hold"
typedef ufprog_status (UFPROG_API *api_spi_set_hold)(struct ufprog_if_dev *ifdev, ufprog_bool high);

#define API_NAME_SPI_SET_BUSY_IND		"ufprog_spi_set_busy_ind"
typedef ufprog_status (UFPROG_API *api_spi_set_busy_ind)(struct ufprog_if_dev *ifdev, ufprog_bool active);

#define API_NAME_SPI_POWER_CONTROL		"ufprog_spi_power_control"
typedef ufprog_status (UFPROG_API *api_spi_power_control)(struct ufprog_if_dev *ifdev, ufprog_bool on);

#define API_NAME_SPI_GENERIC_XFER		"ufprog_spi_generic_xfer"
typedef ufprog_status (UFPROG_API *api_spi_generic_xfer)(struct ufprog_if_dev *ifdev,
							 const struct ufprog_spi_transfer *xfers, uint32_t count);

#define API_NAME_SPI_GENERIC_XFER_MAX_SIZE	"ufprog_spi_generic_xfer_max_size"
typedef size_t (UFPROG_API *api_spi_generic_xfer_max_size)(void);

#define API_NAME_SPI_MEM_ADJUST_OP_SIZE		"ufprog_spi_mem_adjust_op_size"
typedef ufprog_status (UFPROG_API *api_spi_mem_adjust_op_size)(struct ufprog_if_dev *ifdev,
							       struct ufprog_spi_mem_op *op);

#define API_NAME_SPI_MEM_SUPPORTS_OP		"ufprog_spi_mem_supports_op"
typedef ufprog_bool (UFPROG_API *api_spi_mem_supports_op)(struct ufprog_if_dev *ifdev,
							  const struct ufprog_spi_mem_op *op);

#define API_NAME_SPI_MEM_EXEC_OP		"ufprog_spi_mem_exec_op"
typedef ufprog_status (UFPROG_API *api_spi_mem_exec_op)(struct ufprog_if_dev *ifdev,
							const struct ufprog_spi_mem_op *op);

#define API_NAME_SPI_MEM_POLL_STATUS		"ufprog_spi_mem_poll_status"
typedef ufprog_status (UFPROG_API *api_spi_mem_poll_status)(struct ufprog_if_dev *ifdev,
							    const struct ufprog_spi_mem_op *op, uint16_t mask,
							    uint16_t match, uint32_t initial_delay_us,
							    uint32_t polling_rate_us, uint32_t timeout_ms);

#define API_NAME_SPI_DRIVE_4IO_ONES		"ufprog_spi_drive_4io_ones"
typedef ufprog_status (UFPROG_API *api_spi_drive_4io_ones)(struct ufprog_if_dev *ifdev, uint32_t clocks);

EXTERN_C_END

#endif /* _UFPROG_API_SPI_H_ */
