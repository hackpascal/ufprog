// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash OTP operations
 */

#include <stdbool.h>
#include <ufprog/spi-nor-opcode.h>
#include "core.h"
#include "regs.h"
#include "otp.h"

#define SECR_CR_OTP_LB_SHIFT				2
#define SECR_DFL_PAGE_SIZE				0x100

uint32_t default_secr_otp_addr(struct spi_nor *snor, uint32_t index, uint32_t addr)
{
	return (index << 12) | addr;
}

ufprog_status default_secr_otp_lock_bit(struct spi_nor *snor, uint32_t index, uint32_t *retbit,
					const struct spi_nor_reg_access **retacc)
{
	*retbit = index + SECR_CR_OTP_LB_SHIFT + 8;
	*(retacc) = &srcr_acc;

	return UFP_OK;
}

static inline uint32_t secr_otp_addr(struct spi_nor *snor, uint32_t index, uint32_t addr)
{
	const struct spi_nor_flash_secr_otp_ops *ops = snor->ext_param.ops.otp->secr;

	if (ops && ops->otp_addr)
		return ops->otp_addr(snor, index, addr);

	return default_secr_otp_addr(snor, index, addr);
}

static inline ufprog_status secr_otp_lock_bit(struct spi_nor *snor, uint32_t index, uint32_t *retbit,
					      const struct spi_nor_reg_access **retacc)
{
	const struct spi_nor_flash_secr_otp_ops *ops = snor->ext_param.ops.otp->secr;

	if (ops && ops->otp_lock_bit)
		return ops->otp_lock_bit(snor, index, retbit, retacc);

	return default_secr_otp_lock_bit(snor, index, retbit, retacc);
}

ufprog_status secr_otp_read_naddr(struct spi_nor *snor, uint8_t opcode, uint32_t index, uint32_t addr, uint8_t naddr,
				  uint32_t len, void *data)
{
	uint32_t otp_addr = secr_otp_addr(snor, index, addr);
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(opcode, 1),
		SPI_MEM_OP_ADDR(naddr, otp_addr, 1),
		SPI_MEM_OP_DUMMY(1, 1),
		SPI_MEM_OP_DATA_IN(len, data, 1)
	);

	if (!ufprog_spi_mem_supports_op(snor->spi, &op))
		return UFP_UNSUPPORTED;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	while (len) {
		STATUS_CHECK_RET(ufprog_spi_mem_adjust_op_size(snor->spi, &op));
		STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));

		op.data.buf.rx = (void *)((uintptr_t)op.data.buf.rx + op.data.len);

		otp_addr += (uint32_t)op.data.len;
		op.addr.val = otp_addr;

		len -= (uint32_t)op.data.len;
		op.data.len = len;
	}

	return UFP_OK;
}

ufprog_status secr_otp_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	return secr_otp_read_naddr(snor, SNOR_CMD_READ_OTP, index, addr, snor->state.a4b_mode ? 4 : 3, len, data);
}

ufprog_status secr_otp_read_paged_naddr(struct spi_nor *snor, uint8_t opcode, uint32_t index, uint32_t addr,
					uint8_t naddr, uint32_t len, void *data)
{
	uint8_t *p = data;
	uint32_t chksz;

	while (len) {
		chksz = SECR_DFL_PAGE_SIZE - (addr & (SECR_DFL_PAGE_SIZE - 1));
		if (chksz > len)
			chksz = len;

		STATUS_CHECK_RET(secr_otp_read_naddr(snor, opcode, index, addr, naddr, chksz, p));

		p += chksz;
		addr += chksz;
		len -= chksz;
	}

	return UFP_OK;
}

ufprog_status secr_otp_read_paged(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, void *data)
{
	return secr_otp_read_paged_naddr(snor, SNOR_CMD_READ_OTP, index, addr, snor->state.a4b_mode ? 4 : 3, len, data);
}

ufprog_status secr_otp_write_naddr(struct spi_nor *snor, uint8_t opcode, uint32_t index, uint32_t addr, uint8_t naddr,
				   uint32_t len, const void *data)
{
	uint32_t otp_addr = secr_otp_addr(snor, index, addr);
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(opcode, 1),
		SPI_MEM_OP_ADDR(naddr, otp_addr, 1),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_DATA_OUT(len, data, 1)
	);

	if (!ufprog_spi_mem_supports_op(snor->spi, &op))
		return UFP_UNSUPPORTED;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	while (len) {
		STATUS_CHECK_RET(spi_nor_write_enable(snor));

		STATUS_CHECK_RET(ufprog_spi_mem_adjust_op_size(snor->spi, &op));
		STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));

		STATUS_CHECK_RET(spi_nor_wait_busy(snor, SNOR_PP_TIMEOUT_MS));

		op.data.buf.tx = (const void *)((uintptr_t)op.data.buf.tx + op.data.len);

		otp_addr += (uint32_t)op.data.len;
		op.addr.val = otp_addr;

		len -= (uint32_t)op.data.len;
		op.data.len = len;
	}

	return UFP_OK;
}

ufprog_status secr_otp_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, const void *data)
{
	return secr_otp_write_naddr(snor, SNOR_CMD_PROG_OTP, index, addr, snor->state.a4b_mode ? 4 : 3, len, data);
}

ufprog_status secr_otp_write_paged_naddr(struct spi_nor *snor, uint8_t opcode, uint32_t index, uint32_t addr,
					 uint8_t naddr, uint32_t len, const void *data)
{
	const uint8_t *p = data;
	uint32_t chksz;

	while (len) {
		chksz = SECR_DFL_PAGE_SIZE - (addr & (SECR_DFL_PAGE_SIZE - 1));
		if (chksz > len)
			chksz = len;

		STATUS_CHECK_RET(secr_otp_write_naddr(snor, opcode, index, addr, naddr, chksz, p));

		p += chksz;
		addr += chksz;
		len -= chksz;
	}

	return UFP_OK;
}

ufprog_status secr_otp_write_paged(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len, const void *data)
{
	return secr_otp_write_paged_naddr(snor, SNOR_CMD_PROG_OTP, index, addr, snor->state.a4b_mode ? 4 : 3, len,
					  data);
}

ufprog_status secr_otp_erase_naddr(struct spi_nor *snor, uint8_t opcode, uint32_t index, uint8_t naddr)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(opcode, 1),
		SPI_MEM_OP_ADDR(naddr, secr_otp_addr(snor, index, 0), 1),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_NO_DATA
	);

	if (!ufprog_spi_mem_supports_op(snor->spi, &op))
		return UFP_UNSUPPORTED;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, 1));

	STATUS_CHECK_RET(spi_nor_write_enable(snor));
	STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));
	STATUS_CHECK_RET(spi_nor_wait_busy(snor, SNOR_ERASE_TIMEOUT_MS));

	return UFP_OK;
}

ufprog_status secr_otp_erase(struct spi_nor *snor, uint32_t index)
{
	return secr_otp_erase_naddr(snor, SNOR_CMD_ERASE_OTP, index, snor->state.a4b_mode ? 4 : 3);
}

ufprog_status secr_otp_lock(struct spi_nor *snor, uint32_t index)
{
	const struct spi_nor_reg_access *acc;
	uint32_t val, bit;

	STATUS_CHECK_RET(secr_otp_lock_bit(snor, index, &bit, &acc));
	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, acc, 0, BIT(bit), false));

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, acc, &val));
	if (val & BIT(bit))
		return UFP_OK;

	return UFP_FAIL;
}

ufprog_status secr_otp_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{
	const struct spi_nor_reg_access *acc;
	uint32_t val, bit;

	STATUS_CHECK_RET(secr_otp_lock_bit(snor, index, &bit, &acc));
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, acc, &val));

	if (val & BIT(bit))
		*retlocked = true;
	else
		*retlocked = false;

	return UFP_OK;
}

const struct spi_nor_flash_part_otp_ops secr_otp_ops = {
	.read = secr_otp_read,
	.write = secr_otp_write,
	.erase = secr_otp_erase,
	.lock = secr_otp_lock,
	.locked = secr_otp_locked,
};

ufprog_status UFPROG_API ufprog_spi_nor_otp_read(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len,
						 void *data)
{
	ufprog_status ret;

	if (!snor || !data)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (!snor->ext_param.otp || !snor->ext_param.ops.otp)
		return UFP_UNSUPPORTED;

	if (index < snor->ext_param.otp->start_index ||
	    index >= snor->ext_param.otp->start_index + snor->ext_param.otp->count)
		return UFP_INVALID_PARAMETER;

	if (addr >= snor->ext_param.otp->size || addr + len > snor->ext_param.otp->size)
		return UFP_INVALID_PARAMETER;

	if (!len)
		return UFP_OK;

	ufprog_spi_nor_bus_lock(snor);
	ret = snor->ext_param.ops.otp->read(snor, index, addr, len, data);
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nor_otp_write(struct spi_nor *snor, uint32_t index, uint32_t addr, uint32_t len,
						  const void *data)
{
	ufprog_status ret;

	if (!snor || !data)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (!snor->ext_param.otp || !snor->ext_param.ops.otp)
		return UFP_UNSUPPORTED;

	if (index < snor->ext_param.otp->start_index ||
	    index >= snor->ext_param.otp->start_index + snor->ext_param.otp->count)
		return UFP_INVALID_PARAMETER;

	if (addr >= snor->ext_param.otp->size || addr + len > snor->ext_param.otp->size)
		return UFP_INVALID_PARAMETER;

	if (!len)
		return UFP_OK;

	ufprog_spi_nor_bus_lock(snor);
	ret = snor->ext_param.ops.otp->write(snor, index, addr, len, data);
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nor_otp_erase(struct spi_nor *snor, uint32_t index)
{
	ufprog_status ret;

	if (!snor)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (!snor->ext_param.otp || !snor->ext_param.ops.otp)
		return UFP_UNSUPPORTED;

	if (index < snor->ext_param.otp->start_index ||
	    index >= snor->ext_param.otp->start_index + snor->ext_param.otp->count)
		return UFP_INVALID_PARAMETER;

	ufprog_spi_nor_bus_lock(snor);
	ret = snor->ext_param.ops.otp->erase(snor, index);
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nor_otp_lock(struct spi_nor *snor, uint32_t index)
{
	ufprog_status ret;

	if (!snor)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (!snor->ext_param.otp || !snor->ext_param.ops.otp)
		return UFP_UNSUPPORTED;

	if (index < snor->ext_param.otp->start_index ||
	    index >= snor->ext_param.otp->start_index + snor->ext_param.otp->count)
		return UFP_INVALID_PARAMETER;

	ufprog_spi_nor_bus_lock(snor);
	ret = snor->ext_param.ops.otp->lock(snor, index);
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nor_otp_locked(struct spi_nor *snor, uint32_t index, ufprog_bool *retlocked)
{
	ufprog_status ret;

	if (!snor || !retlocked)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (!snor->ext_param.otp || !snor->ext_param.ops.otp)
		return UFP_UNSUPPORTED;

	if (index < snor->ext_param.otp->start_index ||
	    index >= snor->ext_param.otp->start_index + snor->ext_param.otp->count)
		return UFP_INVALID_PARAMETER;

	ufprog_spi_nor_bus_lock(snor);
	ret = snor->ext_param.ops.otp->locked(snor, index, retlocked);
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}
