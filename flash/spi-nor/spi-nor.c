// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash core
 */

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ufprog/log.h>
#include <ufprog/misc.h>
#include <ufprog/sizes.h>
#include <ufprog/crc32.h>
#include <ufprog/spi-nor-opcode.h>
#include "core.h"
#include "ext_id.h"

struct spi_nor_opcodes {
	const struct spi_nor_io_opcode *read;
	const struct spi_nor_io_opcode *pp;
	struct spi_nor_erase_info ei;
};

static ufprog_status spi_nor_chip_setup(struct spi_nor *snor);
static ufprog_status spi_nor_page_program(struct spi_nor *snor, uint64_t addr, size_t len, const void *data,
					  size_t *retlen);
static ufprog_status spi_nor_aai_write(struct spi_nor *snor, uint64_t addr, size_t len, const void *data,
				       size_t *retlen);

ufprog_status UFPROG_API ufprog_spi_nor_load_ext_id_file(void)
{
	return spi_nor_load_ext_id_list();
}

struct spi_nor *UFPROG_API ufprog_spi_nor_create(void)
{
	struct spi_nor *snor;

	snor = calloc(1, sizeof(*snor));
	if (!snor) {
		logm_err("No memory for SPI-NOR object\n");
		return NULL;
	}

	snor->max_speed = SNOR_SPEED_HIGH;
	snor->allowed_io_caps = (1 << __SPI_MEM_IO_MAX) - 1;

	return snor;
}

ufprog_status UFPROG_API ufprog_spi_nor_destroy(struct spi_nor *snor)
{
	if (!snor)
		return UFP_INVALID_PARAMETER;

	if (snor->ext_param.erase_regions && snor->ext_param.erase_regions != &snor->uniform_erase_region)
		free(snor->ext_param.erase_regions);

	if (snor->sfdp.data)
		free(snor->sfdp.data);

	if (snor->sfdp.data_copy)
		free(snor->sfdp.data_copy);

	if (snor->wp_regions)
		free(snor->wp_regions);

	free(snor);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nor_attach(struct spi_nor *snor, struct ufprog_spi *spi)
{
	if (!snor || !spi)
		return UFP_INVALID_PARAMETER;

	if (snor->spi) {
		logm_err("The SPI-NOR object has already attached to a SPI interface device\n");
		return UFP_FAIL;
	}

	snor->spi = spi;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nor_detach(struct spi_nor *snor, ufprog_bool close_if)
{
	if (!snor)
		return UFP_INVALID_PARAMETER;

	if (!snor->spi)
		return UFP_OK;

	if (close_if)
		ufprog_spi_close_device(snor->spi);

	snor->spi = NULL;

	return UFP_OK;
}

struct ufprog_spi *UFPROG_API ufprog_spi_nor_get_interface_device(struct spi_nor *snor)
{
	if (!snor)
		return NULL;

	return snor->spi;
}

ufprog_status UFPROG_API ufprog_spi_nor_bus_lock(struct spi_nor *snor)
{
	return ufprog_spi_bus_lock(snor->spi);
}

ufprog_status UFPROG_API ufprog_spi_nor_bus_unlock(struct spi_nor *snor)
{
	return ufprog_spi_bus_unlock(snor->spi);
}

uint32_t UFPROG_API ufprog_spi_nor_get_allowed_io_caps(struct spi_nor *snor)
{
	if (!snor)
		return 0;

	return snor->allowed_io_caps;
}

void UFPROG_API ufprog_spi_nor_set_allowed_io_caps(struct spi_nor *snor, uint32_t io_caps)
{
	if (!snor)
		return;

	snor->allowed_io_caps = io_caps;
}

uint32_t UFPROG_API ufprog_spi_nor_get_speed_limit(struct spi_nor *snor)
{
	if (!snor)
		return 0;

	return snor->max_speed;
}

void UFPROG_API ufprog_spi_nor_set_speed_limit(struct spi_nor *snor, uint32_t hz)
{
	if (!snor)
		return;

	snor->max_speed = hz;
}

uint32_t UFPROG_API ufprog_spi_nor_get_speed_low(struct spi_nor *snor)
{
	if (!snor)
		return 0;

	return snor->state.speed_low;
}

uint32_t UFPROG_API ufprog_spi_nor_get_speed_high(struct spi_nor *snor)
{
	if (!snor)
		return 0;

	return snor->state.speed_high;
}

static ufprog_status spi_nor_set_speed(struct spi_nor *snor, uint32_t speed)
{
	ufprog_status ret;

	ret = ufprog_spi_set_speed(snor->spi, speed, NULL);
	if (!ret || ret == UFP_UNSUPPORTED)
		return UFP_OK;

	return ret;
}

ufprog_status spi_nor_set_low_speed(struct spi_nor *snor)
{
	return spi_nor_set_speed(snor, snor->state.speed_low);
}

ufprog_status spi_nor_set_high_speed(struct spi_nor *snor)
{
	return spi_nor_set_speed(snor, snor->state.speed_high);
}

ufprog_status spi_nor_read_reg(struct spi_nor *snor, uint8_t regopcode, uint8_t *retval)
{
	struct ufprog_spi_mem_op op = SNOR_READ_NO_ADDR_DUMMY_OP(regopcode, snor->state.cmd_buswidth_curr, 1, retval);

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

ufprog_status spi_nor_write_reg(struct spi_nor *snor, uint8_t regopcode, uint8_t val)
{
	struct ufprog_spi_mem_op op = SNOR_WRITE_NO_ADDR_DUMMY_OP(regopcode, snor->state.cmd_buswidth_curr, 1, &val);

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

ufprog_status spi_nor_issue_single_opcode(struct spi_nor *snor, uint8_t opcode)
{
	struct ufprog_spi_mem_op op = SNOR_WRITE_NO_ADDR_DUMMY_OP(opcode, snor->state.cmd_buswidth_curr, 0, NULL);

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_status spi_nor_read_id_custom(struct spi_nor *snor, uint8_t opcode, uint8_t *id, uint32_t len,
					    uint8_t ndummy, uint8_t bw)
{
	struct ufprog_spi_mem_op op = SNOR_READ_ID_OP(opcode, bw, len, ndummy, id);

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_bool spi_nor_supports_read_id_custom(struct spi_nor *snor, uint8_t opcode, uint32_t len, uint8_t ndummy,
						   uint8_t bw)
{
	struct ufprog_spi_mem_op op = SNOR_READ_ID_OP(opcode, bw, len, ndummy, NULL);

	return ufprog_spi_mem_supports_op(snor->spi, &op);
}

static ufprog_status spi_nor_read_id(struct spi_nor *snor, uint8_t opcode, uint8_t *id, uint32_t len, uint8_t ndummy)
{
	return spi_nor_read_id_custom(snor, opcode, id, len, ndummy, snor->state.cmd_buswidth_curr);
}

ufprog_status spi_nor_volatile_write_enable(struct spi_nor *snor)
{
	if (snor->param.flags & SNOR_F_NO_WREN)
		return UFP_OK;

	return spi_nor_issue_single_opcode(snor, SNOR_CMD_VOLATILE_WRITE_EN);
}

ufprog_status spi_nor_sr_write_enable(struct spi_nor *snor, bool volatile_write, bool *retpoll)
{
	*retpoll = false;

	if (snor->param.flags & SNOR_F_NO_WREN)
		return UFP_OK;

	if (volatile_write && (snor->param.flags & SNOR_F_SR_VOLATILE_WREN_50H))
		return spi_nor_volatile_write_enable(snor);

	if ((volatile_write && (snor->param.flags & SNOR_F_SR_VOLATILE)) ||
	    (!volatile_write && (snor->param.flags & SNOR_F_SR_NON_VOLATILE))) {
		*retpoll = true;
		return spi_nor_write_enable(snor);
	}

	return UFP_OK;
}

ufprog_status spi_nor_read_sr(struct spi_nor *snor, uint8_t *retval)
{
	ufprog_status ret;
	uint32_t val;

	ret = spi_nor_read_reg_acc(snor, snor->state.reg.sr_r, &val);
	if (ret)
		logm_err("Failed to read status register\n");
	else
		*retval = val & 0xff;

	return ret;
}

ufprog_status spi_nor_write_sr(struct spi_nor *snor, uint8_t val, bool volatile_write)
{
	ufprog_status ret;

	if (snor->state.reg.cr == snor->state.reg.sr_w)
		ret = spi_nor_update_reg_acc(snor, snor->state.reg.sr_w, 0xff, val, volatile_write);
	else
		ret = spi_nor_write_reg_acc(snor, snor->state.reg.sr_w, val, volatile_write);

	if (ret)
		logm_err("Failed to write status register\n");

	return ret;
}

ufprog_status spi_nor_write_enable(struct spi_nor *snor)
{
	ufprog_status ret;
	uint8_t val;

	if (snor->param.flags & SNOR_F_NO_WREN)
		return UFP_OK;

	ret = spi_nor_issue_single_opcode(snor, SNOR_CMD_WRITE_EN);
	if (ret) {
		logm_err("Failed to issue write enable instruction\n");
		return ret;
	}

	STATUS_CHECK_RET(spi_nor_read_sr(snor, &val));

	if (!(val & SR_WEL)) {
		logm_err("Write enable instruction failed\n");
		return UFP_FAIL;
	}

	return UFP_OK;
}

ufprog_status spi_nor_data_write_enable(struct spi_nor *snor)
{
	if (snor->param.flags & SNOR_F_NO_WREN)
		return UFP_OK;

	return snor->ext_param.data_write_enable(snor);
}

ufprog_status spi_nor_write_disable(struct spi_nor *snor)
{
	ufprog_status ret;
	uint8_t val;

	if (snor->param.flags & SNOR_F_NO_WREN)
		return UFP_OK;

	ret = spi_nor_issue_single_opcode(snor, SNOR_CMD_WRITE_DIS);
	if (ret) {
		logm_err("Failed to issue write disable instruction\n");
		return ret;
	}

	STATUS_CHECK_RET(spi_nor_read_sr(snor, &val));

	if (val & SR_WEL) {
		logm_err("Write disable instruction failed\n");
		return UFP_FAIL;
	}

	return UFP_OK;
}

ufprog_status spi_nor_quad_enable_any(struct spi_nor *snor, const struct spi_nor_reg_access *regacc, uint32_t bit)
{
	uint32_t val, bitmask;

	bitmask = BIT(bit);

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, regacc, &val));

	if (val & bitmask) {
		logm_dbg("Quad-Enable bit has already been set\n");
		return UFP_OK;
	}

	val |= bitmask;

	STATUS_CHECK_RET(spi_nor_write_reg_acc(snor, regacc, val, true));

	/* Do verify the bit */
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, regacc, &val));

	if (!(val & bitmask)) {
		logm_dbg("Failed to set Quad-Enable bit\n");
		return UFP_FAIL;
	}

	return UFP_OK;
}

static ufprog_status spi_nor_quad_enable_sr2_bit1_write_sr1(struct spi_nor *snor)
{
	return spi_nor_quad_enable_any(snor, &srcr_acc, 9);
}

static ufprog_status spi_nor_quad_enable_sr1_bit6(struct spi_nor *snor)
{
	return spi_nor_quad_enable_any(snor, &sr_acc, 6);
}

static ufprog_status spi_nor_quad_enable_sr2_bit1(struct spi_nor *snor)
{
	return spi_nor_quad_enable_any(snor, &cr_acc, 1);
}

static ufprog_status spi_nor_quad_enable_sr2_bit7(struct spi_nor *snor)
{
	return spi_nor_quad_enable_any(snor, &cr_acc, 7);
}

static ufprog_status spi_nor_quad_enable_nvcr_bit4(struct spi_nor *snor)
{
	/* Use extended volatile configuration register to avoid modifying the non-volatile one */
	return spi_nor_quad_enable_any(snor, &evcr_acc, 4);
}

static ufprog_status spi_nor_chip_soft_reset_drive_4io_fh(struct spi_nor *snor, uint32_t clocks)
{
	ufprog_status ret;

	ret = ufprog_spi_drive_4io_ones(snor->spi, clocks);
	if (ret)
		logm_err("Failed to drive Fh on all for I/O lines for %u clocks for chip soft reset\n", clocks);

	return ret;
}

static ufprog_status spi_nor_chip_soft_reset_drive_4io_fh_8clks(struct spi_nor *snor)
{
	return spi_nor_chip_soft_reset_drive_4io_fh(snor, 8);
}

static ufprog_status spi_nor_chip_soft_reset_drive_4io_fh_8_10clks(struct spi_nor *snor)
{
	return spi_nor_chip_soft_reset_drive_4io_fh(snor, snor->state.a4b_mode ? 10 : 8);
}

static ufprog_status spi_nor_chip_soft_reset_drive_4io_fh_16clks(struct spi_nor *snor)
{
	return spi_nor_chip_soft_reset_drive_4io_fh(snor, 16);
}

static ufprog_status spi_nor_chip_soft_reset_f0h(struct spi_nor *snor)
{
	return spi_nor_issue_single_opcode(snor, SNOR_CMD_RESET_F0H);
}

static ufprog_status spi_nor_chip_soft_reset_66h_99h(struct spi_nor *snor)
{
	ufprog_status ret;

	ret = spi_nor_issue_single_opcode(snor, SNOR_CMD_RESET_ENABLE);
	if (ret) {
		logm_err("Failed to issue 66h for enabling chip soft reset\n");
		return ret;
	}

	ret = spi_nor_issue_single_opcode(snor, SNOR_CMD_RESET);
	if (ret)
		logm_err("Failed to issue 99h for chip soft reset\n");

	return ret;
}

static ufprog_status spi_nor_enable_qpi_38h(struct spi_nor *snor)
{
	ufprog_status ret;

	ret = spi_nor_issue_single_opcode(snor, SNOR_CMD_EN_QPI_38H);
	if (ret)
		logm_err("Failed to issue instruction 38h for entering QPI mode\n");

	return ret;
}

static ufprog_status spi_nor_enable_qpi_35h(struct spi_nor *snor)
{
	ufprog_status ret;

	ret = spi_nor_issue_single_opcode(snor, SNOR_CMD_EN_QPI_35H);
	if (ret)
		logm_err("Failed to issue instruction 35h for entering QPI mode\n");

	return ret;
}

static ufprog_status spi_nor_enable_qpi_800003h(struct spi_nor *snor)
{
	uint8_t orig_cmd_bw = snor->state.cmd_buswidth_curr;
	ufprog_status ret;
	uint32_t val;

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &cr2v_800003h_acc, 0, BIT(6), false));

	snor->state.cmd_buswidth_curr = 4;
	STATUS_CHECK_GOTO_RET(spi_nor_wait_busy(snor, snor->state.max_nvcr_pp_time_ms), ret, out);

	STATUS_CHECK_GOTO_RET(spi_nor_read_reg_acc(snor, &cr2v_800003h_acc, &val), ret, out);

	if (!(val & BIT(6))) {
		logm_err("Failed to set bit 6 of register 800003h for entering QPI mode\n");
		ret = UFP_FAIL;
		goto out;
	}

	return UFP_OK;

out:
	snor->state.cmd_buswidth_curr = orig_cmd_bw;
	spi_nor_write_disable(snor);

	return ret;
}

static ufprog_status spi_nor_enable_qpi_38h_qer(struct spi_nor *snor)
{
	STATUS_CHECK_RET(spi_nor_quad_enable(snor));

	return spi_nor_enable_qpi_38h(snor);
}

static ufprog_status spi_nor_enable_qpi_vecr_clr_bit7(struct spi_nor *snor)
{
	return spi_nor_update_reg_acc(snor, &evcr_acc, BIT(7), 0, false);
}

ufprog_status spi_nor_disable_qpi_ffh(struct spi_nor *snor)
{
	ufprog_status ret;

	ret = spi_nor_issue_single_opcode(snor, SNOR_CMD_EX_QPI_FFH);
	if (ret)
		logm_err("Failed to issue instruction FFh for exiting QPI mode\n");

	return ret;
}

ufprog_status spi_nor_disable_qpi_f5h(struct spi_nor *snor)
{
	ufprog_status ret;

	ret = spi_nor_issue_single_opcode(snor, SNOR_CMD_EX_QPI_F5H);
	if (ret)
		logm_err("Failed to issue instruction F5h for exiting QPI mode\n");

	return ret;
}

ufprog_status spi_nor_disable_qpi_800003h(struct spi_nor *snor)
{
	uint8_t orig_cmd_bw = snor->state.cmd_buswidth_curr;
	ufprog_status ret;
	uint32_t val;

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &cr2v_800003h_acc, BIT(6), 0, false));

	snor->state.cmd_buswidth_curr = 1;
	STATUS_CHECK_GOTO_RET(spi_nor_wait_busy(snor, snor->state.max_nvcr_pp_time_ms), ret, out);

	STATUS_CHECK_GOTO_RET(spi_nor_read_reg_acc(snor, &cr2v_800003h_acc, &val), ret, out);

	if (val & BIT(6)) {
		logm_err("Failed to clear bit 6 of register 800003h for exiting QPI mode\n");
		ret = UFP_FAIL;
		goto out;
	}

	return UFP_OK;

out:
	snor->state.cmd_buswidth_curr = orig_cmd_bw;
	spi_nor_write_disable(snor);

	return ret;
}

ufprog_status spi_nor_disable_qpi_66h_99h(struct spi_nor *snor)
{
	return spi_nor_chip_soft_reset_66h_99h(snor);
}

ufprog_status spi_nor_enable_4b_addressing_b7h(struct spi_nor *snor)
{
	ufprog_status ret;

	ret = spi_nor_issue_single_opcode(snor, SNOR_CMD_EN4B);
	if (ret)
		logm_err("Failed to issue instruction B7h for entering 4-byte addressing mode\n");

	return ret;
}

static ufprog_status spi_nor_enable_4b_addressing_b7h_wren(struct spi_nor *snor)
{
	ufprog_status ret;

	STATUS_CHECK_RET(spi_nor_write_enable(snor));
	STATUS_CHECK_RET(spi_nor_enable_4b_addressing_b7h(snor));
	ret = spi_nor_wait_busy(snor, snor->state.max_nvcr_pp_time_ms);
	spi_nor_write_disable(snor);

	return ret;
}

static ufprog_status spi_nor_enable_4b_addressing_bank(struct spi_nor *snor)
{
	uint32_t val;

	STATUS_CHECK_RET(spi_nor_write_reg_acc(snor, &br_acc, BANK_4B_ADDR, true));

	/* Do verify the bit */
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &br_acc, &val));

	if (val != BANK_4B_ADDR) {
		logm_err("Bank register value validation failed\n");
		return UFP_FAIL;
	}

	return UFP_OK;
}

static ufprog_status spi_nor_enable_4b_addressing_nvcr(struct spi_nor *snor)
{
	uint32_t val;

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &nvcr_acc, NVCR_3B_ADDR, 0, false));

	/* Do verify the bit */
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &nvcr_acc, &val));

	if (val & NVCR_3B_ADDR) {
		logm_err("NVCR value validation failed\n");
		return UFP_FAIL;
	}

	return UFP_OK;
}

ufprog_status spi_nor_disable_4b_addressing_e9h(struct spi_nor *snor)
{
	ufprog_status ret;

	ret = spi_nor_issue_single_opcode(snor, SNOR_CMD_EX4B);
	if (ret)
		logm_err("Failed to issue instruction E9h for exiting 4-byte addressing mode\n");

	return ret;
}

static ufprog_status spi_nor_disable_4b_addressing_e9h_wren(struct spi_nor *snor)
{
	ufprog_status ret;

	STATUS_CHECK_RET(spi_nor_write_enable(snor));
	STATUS_CHECK_RET(spi_nor_disable_4b_addressing_e9h(snor));
	ret = spi_nor_wait_busy(snor, snor->state.max_nvcr_pp_time_ms);
	spi_nor_write_disable(snor);

	return ret;
}

static ufprog_status spi_nor_disable_4b_addressing_bank(struct spi_nor *snor)
{
	uint32_t val;

	STATUS_CHECK_RET(spi_nor_write_reg_acc(snor, &br_acc, 0, true));

	/* Do verify the bit */
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &br_acc, &val));

	if (val) {
		logm_err("Bank register value validation failed\n");
		return UFP_FAIL;
	}

	return UFP_OK;
}

static ufprog_status spi_nor_disable_4b_addressing_nvcr(struct spi_nor *snor)
{
	uint32_t val;

	STATUS_CHECK_RET(spi_nor_update_reg_acc(snor, &nvcr_acc, 0, NVCR_3B_ADDR, false));

	/* Do verify the bit */
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &nvcr_acc, &val));

	if (!(val & NVCR_3B_ADDR)) {
		logm_err("NVCR value validation failed\n");
		return UFP_FAIL;
	}

	return UFP_OK;
}

static ufprog_status spi_nor_write_addr_high_byte_ear(struct spi_nor *snor, uint8_t addr_byte)
{
	ufprog_status ret;
	uint32_t val;

	STATUS_CHECK_RET(spi_nor_write_enable(snor));
	ret = spi_nor_write_reg_acc(snor, &ear_acc, addr_byte, true);
	spi_nor_write_disable(snor);

	if (ret)
		return ret;

	/* Do verify the bit */
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &ear_acc, &val));

	if (val != addr_byte) {
		logm_err("Extended address register value validation failed\n");
		return UFP_FAIL;
	}

	return UFP_OK;
}

static ufprog_status spi_nor_write_addr_high_byte_bank(struct spi_nor *snor, uint8_t addr_byte)
{
	uint32_t val;

	STATUS_CHECK_RET(spi_nor_write_reg_acc(snor, &br_acc, addr_byte, true));

	/* Do verify the bit */
	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, &br_acc, &val));

	if (val != addr_byte) {
		logm_err("Bank register value validation failed\n");
		return UFP_FAIL;
	}

	return UFP_OK;
}

static bool spi_nor_recheck_cmd_buswidth(struct spi_nor *snor, const struct spi_nor_id *idcmp)
{
	struct spi_nor_id id;
	ufprog_status ret;

	/* SPI cmd */
	ret = spi_nor_read_id_custom(snor, SNOR_CMD_READ_ID, id.id, idcmp->len, 0, 1);
	if (!ret) {
		if (!memcmp(id.id, idcmp->id, idcmp->len)) {
			snor->state.cmd_buswidth_curr = 1;
			return true;
		}
	}

	/* QPI cmd */
	ret = spi_nor_read_id_custom(snor, SNOR_CMD_READ_ID, id.id, idcmp->len, 0, 4);
	if (!ret) {
		if (!memcmp(id.id, idcmp->id, idcmp->len)) {
			snor->state.cmd_buswidth_curr = 4;
			return true;
		}
	}

	ret = spi_nor_read_id_custom(snor, SNOR_CMD_READ_ID_MULTI, id.id, idcmp->len, 1, 4);
	if (!ret) {
		if (!memcmp(id.id, idcmp->id, idcmp->len)) {
			snor->state.cmd_buswidth_curr = 4;
			return true;
		}
	}

	ret = spi_nor_read_id_custom(snor, SNOR_CMD_READ_ID_MULTI, id.id, idcmp->len, 0, 4);
	if (!ret) {
		if (!memcmp(id.id, idcmp->id, idcmp->len)) {
			snor->state.cmd_buswidth_curr = 4;
			return true;
		}
	}

	/* DPI cmd */
	ret = spi_nor_read_id_custom(snor, SNOR_CMD_READ_ID, id.id, idcmp->len, 0, 2);
	if (!ret) {
		if (!memcmp(id.id, idcmp->id, idcmp->len)) {
			snor->state.cmd_buswidth_curr = 2;
			return true;
		}
	}

	ret = spi_nor_read_id_custom(snor, SNOR_CMD_READ_ID_MULTI, id.id, idcmp->len, 0, 2);
	if (!ret) {
		if (!memcmp(id.id, idcmp->id, idcmp->len)) {
			snor->state.cmd_buswidth_curr = 2;
			return true;
		}
	}

	return false;
}

ufprog_status spi_nor_quad_enable(struct spi_nor *snor)
{
	if (!snor->ext_param.ops.quad_enable || snor->state.qe_set)
		return UFP_OK;

	STATUS_CHECK_RET(snor->ext_param.ops.quad_enable(snor));

	snor->state.qe_set = 1;

	return UFP_OK;
}

ufprog_status spi_nor_setup_addr(struct spi_nor *snor, uint64_t *addr)
{
	uint8_t high_byte = (*addr >> 24) & 0xff;

	if (!snor->ext_param.ops.write_addr_high_byte)
		return UFP_OK;

	if (high_byte != snor->state.curr_high_addr) {
		STATUS_CHECK_RET(snor->ext_param.ops.write_addr_high_byte(snor, high_byte));
		snor->state.curr_high_addr = high_byte;
	}

	*addr &= 0xffffff;

	return UFP_OK;
}

ufprog_status spi_nor_4b_addressing_control(struct spi_nor *snor, bool enable)
{
	ufprog_status ret = UFP_OK;

	if (enable && snor->ext_param.ops.a4b_en) {
		ret = snor->ext_param.ops.a4b_en(snor);
		if (!ret)
			snor->state.a4b_mode = true;
	} else if (!enable && snor->ext_param.ops.a4b_dis) {
		ret = snor->ext_param.ops.a4b_dis(snor);
		if (!ret)
			snor->state.a4b_mode = false;
	}

	return ret;
}

ufprog_status spi_nor_dpi_control(struct spi_nor *snor, bool enable)
{
	ufprog_status ret = UFP_OK;

	if (enable && snor->ext_param.ops.dpi_en) {
		ret = snor->ext_param.ops.dpi_en(snor);
		if (!ret)
			snor->state.cmd_buswidth_curr = 2;
	} else if (!enable && snor->ext_param.ops.dpi_dis) {
		ret = snor->ext_param.ops.dpi_dis(snor);
		if (!ret)
			snor->state.cmd_buswidth_curr = 1;
	}

	return ret;
}

ufprog_status spi_nor_qpi_control(struct spi_nor *snor, bool enable)
{
	ufprog_status ret = UFP_OK;

	if (enable && snor->ext_param.ops.qpi_en) {
		ret = snor->ext_param.ops.qpi_en(snor);
		if (!ret)
			snor->state.cmd_buswidth_curr = 4;
	} else if (!enable && snor->ext_param.ops.qpi_dis) {
		ret = snor->ext_param.ops.qpi_dis(snor);
		if (!ret)
			snor->state.cmd_buswidth_curr = 1;
	}

	return ret;
}

ufprog_status spi_nor_chip_soft_reset(struct spi_nor *snor)
{
	if (!snor->ext_param.ops.soft_reset)
		return UFP_UNSUPPORTED;

	STATUS_CHECK_RET(snor->ext_param.ops.soft_reset(snor));

	os_udelay(SNOR_RESET_WAIT_MS * 1000);

	if (spi_nor_recheck_cmd_buswidth(snor, &snor->param.id)) {
		snor->state.qe_set = false;
		return spi_nor_chip_setup(snor);
	}

	logm_err("Failed to check flash bus width after chip soft reset\n");

	return UFP_DEVICE_IO_ERROR;
}

ufprog_status spi_nor_select_die(struct spi_nor *snor, uint8_t id)
{
	if (id >= snor->param.ndies)
		return UFP_UNSUPPORTED;

	if (snor->param.ndies == 1)
		return UFP_OK;

	return spi_nor_write_reg(snor, SNOR_CMD_SELECT_DIE, id);
}

bool spi_nor_test_io_opcode(struct spi_nor *snor, const struct spi_nor_io_opcode *opcodes, enum spi_mem_io_type io_type,
			    uint8_t naddr, enum ufprog_spi_data_dir data_dir)
{
	struct ufprog_spi_mem_op op = { 0 };
	uint8_t dummy_cycles, dummy_bytes;

	op.cmd.len = 1;
	op.cmd.buswidth = spi_mem_io_cmd_bw(io_type);
	op.cmd.dtr = spi_mem_io_cmd_dtr(io_type);

	op.addr.len = naddr;
	op.addr.buswidth = spi_mem_io_addr_bw(io_type);
	op.addr.dtr = spi_mem_io_addr_dtr(io_type);

	dummy_cycles = opcodes[io_type].ndummy + opcodes[io_type].nmode;

	if ((dummy_cycles * op.addr.buswidth) % 8)
		return false;

	dummy_bytes = dummy_cycles * op.addr.buswidth / 8;

	op.dummy.len = dummy_bytes;
	op.dummy.buswidth = op.addr.buswidth;
	op.dummy.dtr = op.addr.dtr;

	op.data.len = 1;
	op.data.buswidth = spi_mem_io_data_bw(io_type);
	op.data.dtr = spi_mem_io_data_dtr(io_type);
	op.data.dir = data_dir;

	return ufprog_spi_mem_supports_op(snor->spi, &op);
}

static enum spi_mem_io_type spi_nor_choose_io_type(struct spi_nor *snor, const struct spi_nor_io_opcode *opcodes,
						   uint32_t io_caps, uint8_t naddr, enum ufprog_spi_data_dir data_dir)
{
	enum spi_mem_io_type io_type;

	for (io_type = __SPI_MEM_IO_MAX; io_type > 0; io_type--) {
		if (!(io_caps & (1 << (io_type - 1))))
			continue;

		if (!opcodes[io_type - 1].opcode)
			continue;

		if (spi_nor_test_io_opcode(snor, opcodes, io_type - 1, naddr, data_dir))
			return io_type - 1;
	}

	return __SPI_MEM_IO_MAX;
}

static bool spi_nor_test_read_pp_opcode(struct spi_nor *snor, const struct spi_nor_io_opcode *read_opcodes,
					uint32_t read_io_caps,  const struct spi_nor_io_opcode *pp_opcodes,
					uint32_t pp_io_caps, uint8_t naddr, bool same_cmd_bw,
					enum spi_mem_io_type *ret_read_io_type, enum spi_mem_io_type *ret_pp_io_type)
{
	enum spi_mem_io_type read_io_type = SPI_MEM_IO_1_1_1, pp_io_type = SPI_MEM_IO_1_1_1;
	uint32_t read_bw, pp_bw, dis_bw, mask = 0;

	if (!read_opcodes || !pp_opcodes || !read_io_caps || !pp_io_caps)
		return false;

	if (ufprog_spi_if_caps(snor->spi) & UFP_SPI_NO_QPI_BULK_READ)
		read_io_caps &= ~BIT_SPI_MEM_IO_4_4_4;

	read_io_caps &= snor->allowed_io_caps;
	pp_io_caps &= snor->allowed_io_caps;

	while (read_io_caps && pp_io_caps) {
		read_io_type = spi_nor_choose_io_type(snor, read_opcodes, read_io_caps, naddr, SPI_DATA_IN);
		if (read_io_type >= __SPI_MEM_IO_MAX)
			return false;

		pp_io_type = spi_nor_choose_io_type(snor, pp_opcodes, pp_io_caps, naddr, SPI_DATA_OUT);
		if (pp_io_type >= __SPI_MEM_IO_MAX)
			return false;

		if (!same_cmd_bw)
			break;

		read_bw = spi_mem_io_cmd_bw(read_io_type);
		pp_bw = spi_mem_io_cmd_bw(pp_io_type);

		if (read_bw == pp_bw)
			break;

		if (read_bw > pp_bw)
			dis_bw = read_bw;
		else
			dis_bw = pp_bw;

		switch (dis_bw) {
		case 2:
			mask |= BIT_SPI_MEM_IO_2_2_2 | BIT_SPI_MEM_IO_2D_2D_2D;
		case 4:
			mask |= BIT_SPI_MEM_IO_4_4_4 | BIT_SPI_MEM_IO_4D_4D_4D;
		case 8:
			mask |= BIT_SPI_MEM_IO_8_8_8 | BIT_SPI_MEM_IO_8D_8D_8D;
		}

		read_io_caps &= ~mask;
		pp_io_caps &= ~mask;
	}

	*ret_read_io_type = read_io_type;
	*ret_pp_io_type = pp_io_type;

	return true;
}

void spi_nor_gen_erase_info(const struct spi_nor_flash_part *part, const struct spi_nor_erase_info *src,
			    struct spi_nor_erase_info *retei)
{
	uint32_t i, num = 0;

	memset(retei, 0, sizeof(*retei));

	for (i = 0; i < SPI_NOR_MAX_ERASE_INFO; i++) {
		if (!src->info[i].size)
			continue;

		if ((part->flags & SNOR_F_SECT_4K && src->info[i].size == SZ_4K) ||
		    (part->flags & SNOR_F_SECT_32K && src->info[i].size == SZ_32K)) {
			memcpy(&retei->info[num], &src->info[i], sizeof(src->info[i]));
			num++;
		} else if (part->flags & (SNOR_F_SECT_64K | SNOR_F_SECT_256K) && src->info[i].size == SZ_64K) {
			memcpy(&retei->info[num], &src->info[i], sizeof(src->info[i]));
			if (part->flags & SNOR_F_SECT_256K)
				retei->info[num].size = SZ_256K;
			num++;
		}
	}
}

static void spi_nor_get_3b_opcodes(const struct spi_nor_flash_part *part, struct spi_nor_opcodes *opcodes)
{
	const struct spi_nor_erase_info *ei;

	if (part->read_opcodes_3b)
		opcodes->read = part->read_opcodes_3b;
	else
		opcodes->read = default_read_opcodes_3b;

	if (part->pp_opcodes_3b)
		opcodes->pp = part->pp_opcodes_3b;
	else
		opcodes->pp = default_pp_opcodes_3b;

	if (part->erase_info_3b)
		ei = part->erase_info_3b;
	else
		ei = &default_erase_opcodes_3b;

	if (part->flags & (SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SECT_256K))
		spi_nor_gen_erase_info(part, ei, &opcodes->ei);
	else
		memcpy(&opcodes->ei, ei, sizeof(opcodes->ei));
}

static void spi_nor_get_4b_3b_opcodes(const struct spi_nor_flash_part *part, struct spi_nor_opcodes *opcodes)
{
	const struct spi_nor_erase_info *ei;

	if (part->read_opcodes_4b)
		opcodes->read = part->read_opcodes_4b;
	else if (part->read_opcodes_3b)
		opcodes->read = part->read_opcodes_3b;
	else
		opcodes->read = default_read_opcodes_3b;

	if (part->pp_opcodes_4b)
		opcodes->pp = part->pp_opcodes_4b;
	else if (part->pp_opcodes_3b)
		opcodes->pp = part->pp_opcodes_3b;
	else
		opcodes->pp = default_pp_opcodes_3b;

	if (part->erase_info_4b)
		ei = part->erase_info_4b;
	if (part->erase_info_3b)
		ei = part->erase_info_3b;
	else
		ei = &default_erase_opcodes_3b;

	if (part->flags & (SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SECT_256K))
		spi_nor_gen_erase_info(part, ei, &opcodes->ei);
	else
		memcpy(&opcodes->ei, ei, sizeof(opcodes->ei));
}

static void spi_nor_get_4b_opcodes(const struct spi_nor_flash_part *part, struct spi_nor_opcodes *opcodes)
{
	const struct spi_nor_erase_info *ei;

	if (part->read_opcodes_4b)
		opcodes->read = part->read_opcodes_4b;
	else
		opcodes->read = default_read_opcodes_4b;

	if (part->pp_opcodes_4b)
		opcodes->pp = part->pp_opcodes_4b;
	else
		opcodes->pp = default_pp_opcodes_4b;

	if (part->erase_info_4b)
		ei = part->erase_info_4b;
	else
		ei = &default_erase_opcodes_4b;

	if (part->flags & (SNOR_F_SECT_4K | SNOR_F_SECT_32K | SNOR_F_SECT_64K | SNOR_F_SECT_256K))
		spi_nor_gen_erase_info(part, ei, &opcodes->ei);
	else
		memcpy(&opcodes->ei, ei, sizeof(opcodes->ei));
}

static bool spi_nor_is_valid_erase_info(const struct spi_nor_erase_info *ei)
{
	uint32_t i, num = 0;

	for (i = 0; i < SPI_NOR_MAX_ERASE_INFO; i++) {
		if (ei->info[i].size)
			num++;
	}

	return num;
}

static bool spi_nor_setup_opcode(struct spi_nor *snor, const struct spi_nor_flash_part *part)
{
	enum spi_mem_io_type read_io_type, pp_io_type;
	struct spi_nor_opcodes opcodes;
	uint8_t bw;
	uint32_t i;
	bool ret;

	snor->state.naddr = snor->param.naddr;

	if (snor->param.naddr <= 3) {
		spi_nor_get_3b_opcodes(part, &opcodes);

		ret = spi_nor_test_read_pp_opcode(snor, opcodes.read, part->read_io_caps, opcodes.pp, part->pp_io_caps,
						  snor->state.naddr, false, &read_io_type, &pp_io_type);
		if (ret && spi_nor_is_valid_erase_info(&opcodes.ei))
			goto setup_io_opcodes;

		goto fail;
	}

	if ((part->a4b_flags & SNOR_4B_F_ALWAYS) || (!part->a4b_flags && part->a4b_en_type == A4B_EN_ALWAYS)) {
		spi_nor_get_4b_3b_opcodes(part, &opcodes);

		ret = spi_nor_test_read_pp_opcode(snor, opcodes.read, part->read_io_caps, opcodes.pp, part->pp_io_caps,
						  snor->state.naddr, false, &read_io_type, &pp_io_type);
		if (ret && spi_nor_is_valid_erase_info(&opcodes.ei)) {
			logm_dbg("All opcodes are always working in 4-byte addressing mode\n");
			goto setup_io_opcodes;
		}
	}

	if ((part->a4b_flags & SNOR_4B_F_OPCODE) || (!part->a4b_flags && part->a4b_en_type == A4B_EN_4B_OPCODE)) {
		spi_nor_get_4b_opcodes(part, &opcodes);

		ret = spi_nor_test_read_pp_opcode(snor, opcodes.read, part->read_io_caps, opcodes.pp, part->pp_io_caps,
						  snor->state.naddr, false, &read_io_type, &pp_io_type);
		if (ret && spi_nor_is_valid_erase_info(&opcodes.ei)) {
			logm_dbg("Using 4-byte addressing opcodes\n");
			goto setup_io_opcodes;
		}
	}

	if ((part->a4b_flags & (SNOR_4B_F_B7H_E9H | SNOR_4B_F_WREN_B7H_E9H | SNOR_4B_F_BANK | SNOR_4B_F_NVCR)) ||
	    (!part->a4b_flags && (part->a4b_en_type == A4B_EN_B7H || part->a4b_en_type == A4B_EN_WREN_B7H ||
	     part->a4b_en_type == A4B_EN_BANK || part->a4b_en_type == A4B_EN_NVCR))) {
		if ((part->a4b_flags & SNOR_4B_F_B7H_E9H) ||
			(!part->a4b_flags && part->a4b_en_type == A4B_EN_B7H))
			snor->ext_param.ops.a4b_en = spi_nor_enable_4b_addressing_b7h;
		else if ((part->a4b_flags & SNOR_4B_F_WREN_B7H_E9H) ||
				(!part->a4b_flags && part->a4b_en_type == A4B_EN_WREN_B7H))
			snor->ext_param.ops.a4b_en = spi_nor_enable_4b_addressing_b7h_wren;
		else if ((part->a4b_flags & SNOR_4B_F_BANK) ||
				(!part->a4b_flags && part->a4b_en_type == A4B_EN_BANK))
			snor->ext_param.ops.a4b_en = spi_nor_enable_4b_addressing_bank;
		else if ((part->a4b_flags & SNOR_4B_F_NVCR) ||
			 (!part->a4b_flags && part->a4b_en_type == A4B_EN_NVCR))
			snor->ext_param.ops.a4b_en = spi_nor_enable_4b_addressing_nvcr;

		spi_nor_get_3b_opcodes(part, &opcodes);

		ret = spi_nor_test_read_pp_opcode(snor, opcodes.read, part->read_io_caps, opcodes.pp, part->pp_io_caps,
						  snor->state.naddr, false, &read_io_type, &pp_io_type);
		if (ret && spi_nor_is_valid_erase_info(&opcodes.ei)) {

			if ((part->a4b_flags & SNOR_4B_F_B7H_E9H) ||
			    (!part->a4b_flags && part->a4b_dis_type == A4B_DIS_E9H))
				snor->ext_param.ops.a4b_dis = spi_nor_disable_4b_addressing_e9h;
			else if ((part->a4b_flags & SNOR_4B_F_WREN_B7H_E9H) ||
				   (!part->a4b_flags && part->a4b_dis_type == A4B_DIS_WREN_E9H))
				snor->ext_param.ops.a4b_dis = spi_nor_disable_4b_addressing_e9h_wren;
			else if ((part->a4b_flags & SNOR_4B_F_BANK) ||
				 (!part->a4b_flags && part->a4b_dis_type == A4B_DIS_BANK))
				snor->ext_param.ops.a4b_dis = spi_nor_disable_4b_addressing_bank;
			else if ((part->a4b_flags & SNOR_4B_F_NVCR) ||
				 (!part->a4b_flags && part->a4b_dis_type == A4B_DIS_NVCR))
				snor->ext_param.ops.a4b_dis = spi_nor_disable_4b_addressing_nvcr;
			/*
			 * // Do not support this now
			 * else if (!part->a4b_flags && part->a4b_dis_type == A4B_DIS_66H_99H)
			 *	snor->param.a4b_dis = snor->param.soft_reset;
			*/

			logm_dbg("Using 3-byte addressing opcodes in 4-byte addressing mode\n");

			goto setup_io_opcodes;
		}
	}

	if ((part->a4b_flags & (SNOR_4B_F_EAR | SNOR_4B_F_BANK)) ||
	    (part->a4b_en_type == A4B_EN_EAR || part->a4b_en_type == A4B_EN_BANK)) {
		snor->state.naddr = 3;

		spi_nor_get_3b_opcodes(part, &opcodes);

		ret = spi_nor_test_read_pp_opcode(snor, opcodes.read, part->read_io_caps, opcodes.pp, part->pp_io_caps,
						  snor->state.naddr, false, &read_io_type, &pp_io_type);
		if (ret && spi_nor_is_valid_erase_info(&opcodes.ei)) {
			if (part->a4b_flags & SNOR_4B_F_EAR)
				snor->ext_param.ops.write_addr_high_byte = spi_nor_write_addr_high_byte_ear;
			else if (part->a4b_flags & SNOR_4B_F_BANK)
				snor->ext_param.ops.write_addr_high_byte = spi_nor_write_addr_high_byte_bank;

			logm_dbg("Using 3-byte addressing opcodes with extended address byte\n");
			goto setup_io_opcodes;
		}
	}

fail:
	logm_err("Unable to select a proper opcode for read/page program/erase\n");
	return false;

setup_io_opcodes:
	snor->state.read_opcode = opcodes.read[read_io_type].opcode;
	snor->state.read_ndummy = (opcodes.read[read_io_type].ndummy + opcodes.read[read_io_type].nmode) *
		spi_mem_io_addr_bw(read_io_type) / 8;
	snor->state.read_io_info = ufprog_spi_mem_io_bus_width_info(read_io_type);

	logm_dbg("Selected opcode %02Xh for read, I/O type %s, %u dummy byte(s)\n",
		 snor->state.read_opcode, ufprog_spi_mem_io_name(read_io_type), snor->state.read_ndummy);

	snor->state.pp_opcode = opcodes.pp[pp_io_type].opcode;
	snor->state.pp_io_info = ufprog_spi_mem_io_bus_width_info(pp_io_type);

	logm_dbg("Selected opcode %02Xh for page program, I/O type %s\n", snor->state.pp_opcode,
		 ufprog_spi_mem_io_name(pp_io_type));

	memcpy(&snor->param.erase_info, &opcodes.ei, sizeof(opcodes.ei));

	for (i = 0; i < SPI_NOR_MAX_ERASE_INFO; i++) {
		if (!snor->param.erase_info.info[i].size)
			continue;

		logm_dbg("Selected opcode %02Xh for %uKB erase\n",
			 snor->param.erase_info.info[i].opcode, snor->param.erase_info.info[i].size >> 10);

		if (!snor->param.erase_info.info[i].max_erase_time_ms)
			snor->param.erase_info.info[i].max_erase_time_ms = SNOR_ERASE_TIMEOUT_MS;
	}

	/* Whether to enable full scope DPI/QPI */
	snor->state.cmd_buswidth = 1;

	if (spi_mem_io_cmd_bw(read_io_type) == spi_mem_io_cmd_bw(pp_io_type)) {
		bw = spi_mem_io_cmd_bw(read_io_type);

		if (bw == 4 && (part->flags & SNOR_F_FULL_QPI_OPCODES)) {
			snor->state.cmd_buswidth = 4;
			logm_notice("The flash will be working in QPI mode\n");
		} else if (bw == 2 && (part->flags & SNOR_F_FULL_DPI_OPCODES)) {
			snor->state.cmd_buswidth = 2;
			logm_notice("The flash will be working in DPI mode\n");
		}
	}

	return true;
}

static int spi_nor_erase_info_cmp(void const *a, void const *b)
{
	const struct spi_nor_erase_sector_info *eia = a, *eib = b;

	if (eia->size < eib->size)
		return 1;
	else if (eia->size > eib->size)
		return -1;

	return 0;
}

void spi_nor_fill_erase_region_erasesizes(struct spi_nor *snor, struct spi_nor_erase_region *erg, uint64_t region_size)
{
	uint32_t i;

	erg->max_erasesize = erg->min_erasesize = 0;

	for (i = 0; i < SPI_NOR_MAX_ERASE_INFO; i++) {
		if (!(erg->erasesizes_mask & BIT(i)))
			continue;

		if (!erg->min_erasesize || erg->min_erasesize > snor->param.erase_info.info[i].size) {
			erg->min_erasesize = snor->param.erase_info.info[i].size;
			if (erg->min_erasesize > region_size)
				erg->min_erasesize = (uint32_t)region_size;
		}

		if (snor->param.erase_info.info[i].size > erg->max_erasesize) {
			erg->max_erasesize = snor->param.erase_info.info[i].size;
			if (erg->max_erasesize > region_size)
				erg->max_erasesize = (uint32_t)region_size;
		}
	}
}

static void spi_nor_generate_erase_regions(struct spi_nor *snor)
{
	uint32_t i;

	/* Sort by size, from large to small */
	qsort(snor->param.erase_info.info, SPI_NOR_MAX_ERASE_INFO, sizeof(snor->param.erase_info.info[0]),
	      spi_nor_erase_info_cmp);

	snor->ext_param.num_erase_regions = 1;
	snor->ext_param.erase_regions = &snor->uniform_erase_region;
	memset(&snor->uniform_erase_region, 0, sizeof(snor->uniform_erase_region));

	snor->ext_param.erase_regions[0].size = snor->param.size;

	for (i = 0; i < SPI_NOR_MAX_ERASE_INFO; i++) {
		if (!snor->param.erase_info.info[i].size)
			continue;

		snor->ext_param.erase_regions[0].erasesizes_mask |= BIT(i);
	}

	spi_nor_fill_erase_region_erasesizes(snor, &snor->ext_param.erase_regions[0], snor->param.size);
}

static void spi_nor_setup_soft_reset(struct spi_nor *snor, const struct spi_nor_flash_part *part)
{
	if (part->soft_reset_flags & SNOR_SOFT_RESET_OPCODE_66H_99H) {
		snor->ext_param.ops.soft_reset = spi_nor_chip_soft_reset_66h_99h;
	} else if (part->soft_reset_flags & SNOR_SOFT_RESET_OPCODE_F0H) {
		snor->ext_param.ops.soft_reset = spi_nor_chip_soft_reset_f0h;
	} else if (part->soft_reset_flags & (SNOR_SOFT_RESET_DRV_FH_4IO_8CLKS | SNOR_SOFT_RESET_DRV_FH_4IO_16CLKS)) {
		if (!ufprog_spi_supports_drive_4io_ones(snor->spi)) {
			logm_warn("Chip soft reset using driving Fh on all four I/O lines is not supported\n");
			return;
		}

		if (part->soft_reset_flags & SNOR_SOFT_RESET_DRV_FH_4IO_8CLKS) {
			if (part->soft_reset_flags & SNOR_SOFT_RESET_DRV_FH_4IO_10CLKS_4B)
				snor->ext_param.ops.soft_reset = spi_nor_chip_soft_reset_drive_4io_fh_8_10clks;
			else
				snor->ext_param.ops.soft_reset = spi_nor_chip_soft_reset_drive_4io_fh_8clks;
		} else if (part->soft_reset_flags & SNOR_SOFT_RESET_DRV_FH_4IO_16CLKS) {
			snor->ext_param.ops.soft_reset = spi_nor_chip_soft_reset_drive_4io_fh_16clks;
		}
	}
}

static bool spi_nor_setup_quad_enable(struct spi_nor *snor, const struct spi_nor_flash_part *part)
{
	switch (part->qe_type) {
	case QE_UNKNOWN:
	case QE_DONT_CARE:
		return true;

	case QE_SR1_BIT6:
		snor->ext_param.ops.quad_enable = spi_nor_quad_enable_sr1_bit6;
		return true;

	case QE_SR2_BIT1:
		snor->ext_param.ops.quad_enable = spi_nor_quad_enable_sr2_bit1;
		return true;

	case QE_SR2_BIT1_WR_SR1:
		snor->ext_param.ops.quad_enable = spi_nor_quad_enable_sr2_bit1_write_sr1;
		return true;

	case QE_SR2_BIT7:
		snor->ext_param.ops.quad_enable = spi_nor_quad_enable_sr2_bit7;
		return true;

	case QE_NVCR_BIT4:
		snor->ext_param.ops.quad_enable = spi_nor_quad_enable_nvcr_bit4;
		return true;

	default:
		logm_err("Invalid configuration for Quad-Enable\n");
		snor->ext_param.ops.quad_enable = NULL;
		return false;
	}
}

static bool spi_nor_setup_multi_io(struct spi_nor *snor, const struct spi_nor_flash_part *part)
{
	switch (part->qpi_en_type) {
	case QPI_EN_NONE:
		if (spi_mem_io_cmd_bw(snor->state.read_io_info) == 4 ||
		    spi_mem_io_cmd_bw(snor->state.pp_io_info) == 4) {
			logm_err("Missing method for enabling QPI mode\n");
			return false;
		}

		snor->ext_param.ops.qpi_en = NULL;
		break;

	case QPI_EN_VENDOR:
		break;

	case QPI_EN_QER_38H:
		snor->ext_param.ops.qpi_en = spi_nor_enable_qpi_38h_qer;
		break;

	case QPI_EN_38H:
		snor->ext_param.ops.qpi_en = spi_nor_enable_qpi_38h;
		break;

	case QPI_EN_35H:
		snor->ext_param.ops.qpi_en = spi_nor_enable_qpi_35h;
		break;

	case QPI_EN_800003H:
		snor->ext_param.ops.qpi_en = spi_nor_enable_qpi_800003h;
		break;

	case QPI_EN_VECR_BIT7_CLR:
		snor->ext_param.ops.qpi_en = spi_nor_enable_qpi_vecr_clr_bit7;
		break;

	default:
		logm_err("Invalid configuration for QPI-mode enable\n");
		snor->ext_param.ops.qpi_en = NULL;
		return false;
	}

	switch (part->qpi_dis_type) {
	case QPI_DIS_NONE:
		if (spi_mem_io_cmd_bw(snor->state.read_io_info) == 4 ||
		    spi_mem_io_cmd_bw(snor->state.pp_io_info) == 4) {
			logm_err("Missing method for disabling QPI mode\n");
			return false;
		}

		snor->ext_param.ops.qpi_dis = NULL;
		break;

	case QPI_DIS_VENDOR:
		break;

	case QPI_DIS_FFH:
		snor->ext_param.ops.qpi_dis = spi_nor_disable_qpi_ffh;
		break;

	case QPI_DIS_F5H:
		snor->ext_param.ops.qpi_dis = spi_nor_disable_qpi_f5h;
		break;

	case QPI_DIS_800003H:
		snor->ext_param.ops.qpi_dis = spi_nor_disable_qpi_800003h;
		break;

	case QPI_DIS_66H_99H:
		snor->ext_param.ops.qpi_dis = spi_nor_disable_qpi_66h_99h;
		break;

	default:
		logm_err("Invalid configuration for QPI-mode disable\n");
		snor->ext_param.ops.qpi_dis = NULL;
		return false;
	}

	if ((!!snor->ext_param.ops.qpi_en) ^ (!!snor->ext_param.ops.qpi_dis)) {
		logm_err("Invalid configuration for QPI-mode enable/disable\n");
		return false;
	}

	return true;
}

static bool spi_nor_read_and_match_jedec_id(struct spi_nor *snor, uint8_t opcode, uint8_t ndummy,
					    struct spi_nor_vendor_part *retvp)
{
	ufprog_status ret = UFP_UNSUPPORTED;
	bool probed;

	if (spi_nor_supports_read_id_custom(snor, opcode, SPI_NOR_MAX_ID_LEN, ndummy, snor->state.cmd_buswidth_curr))
		ret = spi_nor_read_id(snor, opcode, snor->param.id.id, SPI_NOR_MAX_ID_LEN, ndummy);

	if (ret) {
		if (ret == UFP_UNSUPPORTED) {
			if (spi_nor_supports_read_id_custom(snor, SNOR_CMD_READ_ID, SPI_NOR_DFL_ID_LEN, ndummy,
							    snor->state.cmd_buswidth_curr))
				ret = spi_nor_read_id(snor, opcode, snor->param.id.id, SPI_NOR_DFL_ID_LEN, ndummy);
		}

		if (ret)
			return false;
	}

	probed = spi_nor_find_vendor_part(snor->param.id.id, retvp);
	if (probed)
		snor->param.id.len = retvp->part->id.len;

	return probed;
}

static bool spi_nor_probe_jedec_id_retry(struct spi_nor *snor, struct spi_nor_vendor_part *retvp, uint32_t retries)
{
	bool ret;

	retvp->part = NULL;
	retvp->vendor = NULL;

retry:
	/* SPI cmd */
	snor->state.cmd_buswidth_curr = 1;

	logm_dbg("Trying reading JEDEC ID in SPI mode\n");
	ret = spi_nor_read_and_match_jedec_id(snor, SNOR_CMD_READ_ID, 0, retvp);
	if (ret)
		return ret;

	/* QPI cmd */
	logm_dbg("Trying reading JEDEC ID in QPI mode\n");

	snor->state.cmd_buswidth_curr = 4;

	ret = spi_nor_read_and_match_jedec_id(snor, SNOR_CMD_READ_ID_MULTI, 1, retvp);
	if (ret)
		return ret;

	ret = spi_nor_read_and_match_jedec_id(snor, SNOR_CMD_READ_ID_MULTI, 0, retvp);
	if (ret)
		return ret;

	ret = spi_nor_read_and_match_jedec_id(snor, SNOR_CMD_READ_ID, 0, retvp);
	if (ret)
		return ret;

	/* Dual I/O cmd */
	logm_dbg("Trying reading JEDEC ID in DPI mode\n");

	snor->state.cmd_buswidth_curr = 2;

	ret = spi_nor_read_and_match_jedec_id(snor, SNOR_CMD_READ_ID_MULTI, 0, retvp);
	if (ret)
		return ret;

	ret = spi_nor_read_and_match_jedec_id(snor, SNOR_CMD_READ_ID, 0, retvp);
	if (ret)
		return ret;

	if (retries) {
		retries--;
		goto retry;
	}

	snor->state.cmd_buswidth_curr = 1;

	logm_notice("Unable to identify SPI-NOR chip using JEDEC ID\n");

	return false;
}

static bool spi_nor_probe_jedec_id(struct spi_nor *snor, struct spi_nor_vendor_part *retvp)
{
	char idstr[20];

	if (spi_nor_set_low_speed(snor))
		logm_warn("Failed to set spi bus low speed\n");

	/* Read JEDEC ID. This may fail due to new ID not recorded. */
	if (!spi_nor_probe_jedec_id_retry(snor, retvp, SNOR_ID_READ_RETRIES))
		return false;

	if (snor->state.cmd_buswidth_curr == 4) {
		/* Disable QPI and retry */
		if (retvp->part && retvp->part->ops && retvp->part->ops->qpi_dis)
			STATUS_CHECK_RET(retvp->part->ops->qpi_dis(snor));
		else if (retvp->vendor && retvp->vendor->default_part_ops && retvp->vendor->default_part_ops->qpi_dis)
			STATUS_CHECK_RET(retvp->vendor->default_part_ops->qpi_dis(snor));

		if (!spi_nor_probe_jedec_id_retry(snor, retvp, SNOR_ID_READ_RETRIES))
			return false;
	} else if (snor->state.cmd_buswidth_curr == 2) {
		/* Disable DPI and retry */
		if (retvp->part && retvp->part->ops && retvp->part->ops->dpi_dis)
			STATUS_CHECK_RET(retvp->part->ops->dpi_dis(snor));
		else if (retvp->vendor && retvp->vendor->default_part_ops && retvp->vendor->default_part_ops->dpi_dis)
			STATUS_CHECK_RET(retvp->vendor->default_part_ops->dpi_dis(snor));

		if (!spi_nor_probe_jedec_id_retry(snor, retvp, SNOR_ID_READ_RETRIES))
			return false;
	}

	logm_dbg("Matched predefined model: %s\n", retvp->part->model);
	bin_to_hex_str(idstr, sizeof(idstr), retvp->part->id.id, retvp->part->id.len, true, true);
	logm_dbg("Matched JEDEC ID: %s\n", idstr);

	return true;
}

static ufprog_status spi_nor_setup_param(struct spi_nor *snor, const struct spi_nor_vendor *vendor,
					 struct spi_nor_flash_part *part)
{
	snor->param.size = part->size;
	snor->param.ndies = part->ndies;
	snor->param.page_size = part->page_size;
	snor->param.naddr = snor->param.size > SZ_16M ? 4 : 3;
	snor->param.max_pp_time_ms = (part->max_pp_time_us + 999) / 1000;

	if (!snor->param.ndies)
		snor->param.ndies = 1;

	spi_nor_setup_soft_reset(snor, part);

	if (!spi_nor_setup_quad_enable(snor, part))
		return UFP_FAIL;

	if (!spi_nor_setup_multi_io(snor, part))
		return UFP_FAIL;

	if (part->qe_type == QE_UNKNOWN) {
		part->read_io_caps &= ~(BIT_SPI_MEM_IO_X4);
		part->pp_io_caps &= ~(BIT_SPI_MEM_IO_X4);
	}

	if (!snor->ext_param.ops.qpi_en) {
		part->read_io_caps &= ~(BIT_SPI_MEM_IO_4_4_4);
		part->pp_io_caps &= ~(BIT_SPI_MEM_IO_4_4_4);
	}

	if (!snor->ext_param.ops.dpi_en) {
		part->read_io_caps &= ~(BIT_SPI_MEM_IO_2_2_2);
		part->pp_io_caps &= ~(BIT_SPI_MEM_IO_2_2_2);
	}

	if (part->flags & SNOR_F_PP_DUAL_INPUT)
		part->pp_io_caps |= BIT_SPI_MEM_IO_1_1_2;

	if (!spi_nor_setup_opcode(snor, part))
		return UFP_FAIL;

	return UFP_OK;
}

static ufprog_status spi_nor_setup_param_final(struct spi_nor *snor, const struct spi_nor_vendor *vendor,
					       struct spi_nor_flash_part *part)
{
	uint32_t final_data_bw, pp_data_bw, max_speed = 0;
	const char *jesd216_ver;

	if (!snor->ext_param.regs)
		snor->ext_param.regs = part->regs;

	if (!snor->ext_param.otp)
		snor->ext_param.otp = part->otp;

	if (!snor->ext_param.wp_ranges)
		snor->ext_param.wp_ranges = part->wp_ranges;

	if (!snor->ext_param.wp_regacc)
		snor->ext_param.wp_regacc = part->wp_regacc;

	if (!snor->ext_param.ops.select_die) {
		if (part->ops && part->ops->select_die)
			snor->ext_param.ops.select_die = part->ops->select_die;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->select_die)
			snor->ext_param.ops.select_die = vendor->default_part_ops->select_die;
	}

	if (!snor->ext_param.ops.setup_dpi) {
		if (part->ops && part->ops->setup_dpi)
			snor->ext_param.ops.setup_dpi = part->ops->setup_dpi;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->setup_dpi)
			snor->ext_param.ops.setup_dpi = vendor->default_part_ops->setup_dpi;
	}

	if (!snor->ext_param.ops.setup_qpi) {
		if (part->ops && part->ops->setup_qpi)
			snor->ext_param.ops.setup_qpi = part->ops->setup_qpi;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->setup_qpi)
			snor->ext_param.ops.setup_qpi = vendor->default_part_ops->setup_qpi;
	}

	if (!snor->ext_param.ops.otp) {
		if (part->ops && part->ops->otp)
			snor->ext_param.ops.otp = part->ops->otp;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->otp)
			snor->ext_param.ops.otp = vendor->default_part_ops->otp;
	}

	if (!snor->ext_param.ops.chip_setup) {
		if (part->ops && part->ops->chip_setup)
			snor->ext_param.ops.chip_setup = part->ops->chip_setup;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->chip_setup)
			snor->ext_param.ops.chip_setup = vendor->default_part_ops->chip_setup;
	}

	if (!snor->ext_param.ops.read_uid) {
		if (part->ops && part->ops->read_uid)
			snor->ext_param.ops.read_uid = part->ops->read_uid;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->read_uid)
			snor->ext_param.ops.read_uid = vendor->default_part_ops->read_uid;
	}

	if (!snor->ext_param.ops.dpi_en) {
		if (part->ops && part->ops->dpi_en)
			snor->ext_param.ops.dpi_en = part->ops->dpi_en;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->dpi_en)
			snor->ext_param.ops.dpi_en = vendor->default_part_ops->dpi_en;
	}

	if (!snor->ext_param.ops.dpi_dis) {
		if (part->ops && part->ops->dpi_dis)
			snor->ext_param.ops.dpi_dis = part->ops->dpi_dis;
		else if (vendor && vendor->default_part_ops && vendor->default_part_ops->dpi_dis)
			snor->ext_param.ops.dpi_dis = vendor->default_part_ops->dpi_dis;
	}

	if (!snor->param.max_pp_time_ms)
		snor->param.max_pp_time_ms = SNOR_PP_TIMEOUT_MS;

	if (!snor->param.page_size)
		snor->param.page_size = SNOR_DFL_PAGE_SIZE;

	final_data_bw = spi_mem_io_info_data_bw(snor->state.read_io_info);
	pp_data_bw = spi_mem_io_info_data_bw(snor->state.pp_io_info);
	if (final_data_bw < pp_data_bw)
		final_data_bw = pp_data_bw;

	switch (final_data_bw) {
	case 2:
		max_speed = part->max_speed_dual_mhz;
		break;

	case 4:
		max_speed = part->max_speed_quad_mhz;
		break;
	}

	if (!max_speed)
		max_speed = part->max_speed_spi_mhz;

	max_speed *= 1000000;
	if (!max_speed)
		max_speed = snor->max_speed;

	snor->param.max_speed = max_speed;

	snor->state.speed_high = snor->param.max_speed;
	if (snor->state.speed_high > snor->max_speed)
		snor->state.speed_high = snor->max_speed;

	/* Set and read back the real highest/lowest speed */
	STATUS_CHECK_RET(spi_nor_set_high_speed(snor));
	snor->state.speed_high = ufprog_spi_get_speed(snor->spi);

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	snor->state.speed_low = ufprog_spi_get_speed(snor->spi);

	snor->param.flags = part->flags;
	snor->param.vendor_flags = part->vendor_flags;

	if (!snor->ext_param.data_write_enable)
		snor->ext_param.data_write_enable = spi_nor_write_enable;

	if (!snor->ext_param.write_page) {
		if (snor->param.flags & SNOR_F_AAI_WRITE)
			snor->ext_param.write_page = spi_nor_aai_write;
		else
			snor->ext_param.write_page = spi_nor_page_program;
	}

	if (!spi_nor_parse_sfdp_smpt(snor))
		return UFP_FAIL;

	if (!snor->ext_param.erase_regions)
		spi_nor_generate_erase_regions(snor);

	if (!snor->state.reg.cr) {
		if (part->qe_type == QE_SR2_BIT1_WR_SR1) {
			snor->state.reg.cr = &srcr_acc;
			snor->state.reg.cr_shift = 8;
		} else {
			snor->state.reg.cr = &cr_acc;
			snor->state.reg.cr_shift = 0;
		}
	}
	memset(snor->param.model, 0, sizeof(snor->param.model));

	if (part->model) {
		snprintf(snor->param.model, sizeof(snor->param.model), "%s", part->model);
	} else {
		switch (snor->sfdp.bfpt_hdr->minor_ver) {
		case SFDP_REV_MINOR_A:
			jesd216_ver = "A";
			break;

		case SFDP_REV_MINOR_B:
			jesd216_ver = "B";
			break;

		case SFDP_REV_MINOR_C:
			jesd216_ver = "C";
			break;

		case SFDP_REV_MINOR_D:
			jesd216_ver = "D";
			break;

		case SFDP_REV_MINOR_E:
			jesd216_ver = "E";
			break;

		case SFDP_REV_MINOR_F:
			jesd216_ver = "F";
			break;

		default:
			jesd216_ver = "";
		}

		snprintf(snor->param.model, sizeof(snor->param.model), "JESD216%s SFDP compatible", jesd216_ver);
	}

	return UFP_OK;
}

static ufprog_status spi_nor_chip_die_setup(struct spi_nor *snor, uint32_t die)
{
	spi_nor_select_die(snor, (uint8_t )die);

	STATUS_CHECK_RET(spi_nor_write_sr(snor, 0, true));

	if (snor->ext_param.ops.chip_setup)
		STATUS_CHECK_RET(snor->ext_param.ops.chip_setup(snor));

	if ((snor->param.flags & SNOR_F_GLOBAL_UNLOCK) || (snor->state.flags & SNOR_F_GLOBAL_UNLOCK)) {
		STATUS_CHECK_RET(spi_nor_write_enable(snor));
		STATUS_CHECK_RET(spi_nor_issue_single_opcode(snor, SNOR_CMD_GLOBAL_BLOCK_UNLOCK));
		STATUS_CHECK_RET(spi_nor_write_disable(snor));
	}

	if (spi_mem_io_info_data_bw(snor->state.read_io_info) == 4 ||
	    spi_mem_io_info_data_bw(snor->state.pp_io_info) == 4) {
		snor->state.qe_set = false;
		STATUS_CHECK_RET(spi_nor_quad_enable(snor));
	}

	STATUS_CHECK_RET(ufprog_spi_nor_set_bus_width(snor, snor->state.cmd_buswidth));

	snor->state.curr_high_addr = 0;

	if (snor->ext_param.ops.write_addr_high_byte)
		STATUS_CHECK_RET(snor->ext_param.ops.write_addr_high_byte(snor, (uint8_t)snor->state.curr_high_addr));

	if (snor->param.naddr > 3) {
		if (snor->state.naddr > 3)
			STATUS_CHECK_RET(spi_nor_4b_addressing_control(snor, true));
		else
			STATUS_CHECK_RET(spi_nor_4b_addressing_control(snor, false));
	}

	return UFP_OK;
}

static ufprog_status spi_nor_chip_setup(struct spi_nor *snor)
{
	uint32_t i;

	for (i = snor->param.ndies; i > 0; i--)
		STATUS_CHECK_RET(spi_nor_chip_die_setup(snor, i - 1));

	return UFP_OK;
}

static void spi_nor_reset_param(struct spi_nor *snor)
{
	if (snor->ext_param.erase_regions && snor->ext_param.erase_regions != &snor->uniform_erase_region)
		free(snor->ext_param.erase_regions);

	if (snor->sfdp.data)
		free(snor->sfdp.data);

	if (snor->sfdp.data_copy)
		free(snor->sfdp.data_copy);

	if (snor->wp_regions) {
		free(snor->wp_regions);
		snor->wp_regions = NULL;
	}

	memset(&snor->sfdp, 0, sizeof(snor->sfdp));
	memset(&snor->param, 0, sizeof(snor->param));
	memset(&snor->ext_param, 0, sizeof(snor->ext_param));
	memset(&snor->state, 0, sizeof(snor->state));
	memset(&snor->uniform_erase_region, 0, sizeof(snor->uniform_erase_region));
}

static ufprog_status spi_nor_pre_init(struct spi_nor *snor)
{
	ufprog_status ret;

	snor->state.speed_low = SNOR_SPEED_LOW;
	snor->state.speed_high = SNOR_SPEED_LOW;

	snor->state.reg.sr_r = &sr_acc;
	snor->state.reg.sr_w = &sr_acc;

	snor->state.max_nvcr_pp_time_ms = SNOR_WRITE_NV_REG_TIMEOUT_MS;

	STATUS_CHECK_RET(ufprog_spi_set_cs_pol(snor->spi, 0));

	ret = ufprog_spi_set_mode(snor->spi, SPI_MODE_0);
	if (ret && ret != UFP_UNSUPPORTED) {
		ret = ufprog_spi_set_mode(snor->spi, SPI_MODE_3);
		if (ret && ret != UFP_UNSUPPORTED) {
			logm_err("Cannot set SPI controller to use either mode 0 nor mode 3\n");
			return ret;
		}
	}

	return UFP_OK;
}

static ufprog_status spi_nor_init(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				  struct spi_nor_flash_part_blank *bp)
{
	const struct spi_nor_vendor *vendor;

	if (bp->p.fixups && bp->p.fixups->pre_param_setup)
		STATUS_CHECK_RET(bp->p.fixups->pre_param_setup(snor, vp, bp));

	vendor = vp->vendor_init ? vp->vendor_init : vp->vendor;

	if (vendor && vendor->default_part_fixups && vendor->default_part_fixups->pre_param_setup &&
	    !(bp->p.flags & SNOR_F_BYPASS_VENDOR_FIXUPS))
		STATUS_CHECK_RET(vendor->default_part_fixups->pre_param_setup(snor, NULL, bp));

	STATUS_CHECK_RET(spi_nor_setup_param(snor, vendor, &bp->p));

	if (bp->p.fixups && bp->p.fixups->post_param_setup)
		STATUS_CHECK_RET(bp->p.fixups->post_param_setup(snor, bp));

	if (vendor && vendor->default_part_fixups && vendor->default_part_fixups->post_param_setup &&
	    !(bp->p.flags & SNOR_F_BYPASS_VENDOR_FIXUPS))
		STATUS_CHECK_RET(vendor->default_part_fixups->post_param_setup(snor, bp));

	STATUS_CHECK_RET(spi_nor_setup_param_final(snor, vendor, &bp->p));

	if (bp->p.fixups && bp->p.fixups->pre_chip_setup)
		STATUS_CHECK_RET(bp->p.fixups->pre_chip_setup(snor));

	if (vendor && vendor->default_part_fixups && vendor->default_part_fixups->pre_chip_setup &&
	    !(bp->p.flags & SNOR_F_BYPASS_VENDOR_FIXUPS))
		STATUS_CHECK_RET(vendor->default_part_fixups->pre_chip_setup(snor));

	memset(snor->param.vendor, 0, sizeof(snor->param.vendor));

	if (vp->vendor)
		snprintf(snor->param.vendor, sizeof(snor->param.vendor), "%s", vp->vendor->name);
	else
		snprintf(snor->param.vendor, sizeof(snor->param.vendor), "Unknown (%02X)", snor->param.id.id[0]);

	logm_dbg("Vendor: %s, Model: %s\n", snor->param.vendor, snor->param.model);

	STATUS_CHECK_RET(spi_nor_chip_setup(snor));

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nor_list_parts(struct spi_nor_part_list **outlist, const char *vendorid,
						   const char *match)
{
	struct spi_nor_part_list *partlist;
	uint32_t count;

	if (!outlist)
		return UFP_INVALID_PARAMETER;

	count = spi_nor_list_parts(vendorid, match, NULL, NULL);

	partlist = malloc(sizeof(*partlist) + count * sizeof(partlist->list[0]));
	if (!partlist) {
		logm_err("No memory for flash part list\n");
		return UFP_NOMEM;
	}

	partlist->num = count;

	spi_nor_list_parts(vendorid, match, NULL, partlist->list);

	*outlist = partlist;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nor_probe(struct spi_nor *snor, struct spi_nor_part_list **outlist,
					      struct spi_nor_id *retid)
{
	struct spi_nor_part_list *partlist;
	struct spi_nor_vendor_part vp;
	ufprog_status ret = UFP_OK;
	uint32_t count;

	if (!snor || !outlist)
		return UFP_INVALID_PARAMETER;

	spi_nor_reset_param(snor);

	STATUS_CHECK_RET(spi_nor_pre_init(snor));

	ufprog_spi_nor_bus_lock(snor);

	if (!spi_nor_probe_jedec_id(snor, &vp))
		ret = UFP_NOT_EXIST;

	ufprog_spi_nor_bus_unlock(snor);

	if (ret)
		return ret;

	count = spi_nor_list_parts(NULL, NULL, &vp.part->id, NULL);

	partlist = malloc(sizeof(*partlist) + count * sizeof(partlist->list[0]));
	if (!partlist) {
		logm_err("No memory for flash part list\n");
		return UFP_NOMEM;
	}

	partlist->num = count;

	spi_nor_list_parts(NULL, NULL, &vp.part->id, partlist->list);

	*outlist = partlist;

	if (retid)
		memcpy(retid, &vp.part->id, sizeof(*retid));

	return 0;
}

ufprog_status UFPROG_API ufprog_spi_nor_free_list(void *list)
{
	if (list)
		free(list);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nor_part_init(struct spi_nor *snor, const char *vendorid, const char *part,
						  ufprog_bool forced_init)
{
	struct spi_nor_vendor_part vp, vpreq = { 0 };
	const struct spi_nor_vendor *vendor = NULL;
	struct spi_nor_flash_part_blank bp;
	bool nosfdp = false;
	ufprog_status ret;
	size_t namelen;
	uint32_t i;

	if (!snor || !part)
		return UFP_INVALID_PARAMETER;

	spi_nor_reset_param(snor);

	STATUS_CHECK_RET(spi_nor_pre_init(snor));

	/* Find the requested vendor */
	if (vendorid) {
		vendor = spi_nor_find_vendor_by_id(vendorid);
		logm_err("Requested vendor name does not exist\n");
		return UFP_NOT_EXIST;
	}

	/* Find the requested part */
	if (!vendor) {
		if (!spi_nor_find_vendor_part_by_name(part, &vpreq))
			vpreq.part = NULL;
	} else {
		if (!spi_nor_vendor_find_part_by_name(part, vendor, &vpreq))
			vpreq.part = NULL;
	}

	if (!vpreq.part) {
		logm_err("Requested part name does not exist\n");
		return UFP_NOT_EXIST;
	}

	if (vpreq.part->flags & SNOR_F_NO_OP) {
		logm_err("This part can not be used\n");
		return UFP_FLASH_PART_NOT_SPECIFIED;
	}

	ufprog_spi_nor_bus_lock(snor);

	/* Check if ID matches */
	if (spi_nor_probe_jedec_id(snor, &vp)) {
		if (vp.part->id.len != vpreq.part->id.len || (
		    !spi_nor_id_match(vp.part->id.id, vpreq.part->id.id, vp.part->id_mask, vp.part->id.len) &&
		    !spi_nor_id_match(vp.part->id.id, vpreq.part->id.id, vpreq.part->id_mask, vp.part->id.len))) {
			if (!forced_init) {
				logm_err("Requested part JEDEC ID mismatch\n");
				ret = UFP_FLASH_PART_MISMATCH;
				goto out;
			} else {
				logm_warn("Requested part JEDEC ID mismatch\n");
				nosfdp = true;
			}
		}
	}

	spi_nor_prepare_blank_part(&bp, vpreq.part);

	if (!nosfdp) {
		if (spi_nor_probe_sfdp(snor, vpreq.vendor, &bp))
			spi_nor_locate_sfdp_vendor(snor, vpreq.part->id.id[0], true);
	}

	memcpy(&snor->param.id, &vpreq.part->id, sizeof(snor->param.id));

	if (!snor->state.cmd_buswidth_curr) {
		if (!spi_nor_recheck_cmd_buswidth(snor, &vpreq.part->id)) {
			logm_err("Unable to check flash bus width\n");
			ret = UFP_DEVICE_IO_ERROR;
			goto out;
		}
	}

	if (strcasecmp(part, vpreq.part->model)) {
		for (i = 0; i < vpreq.part->alias->num; i++) {
			if (!strcasecmp(part, vpreq.part->alias->items[i].model)) {
				namelen = strlen(vpreq.part->alias->items[i].model);
				if (namelen >= sizeof(bp.model))
					namelen = sizeof(bp.model) - 1;

				memcpy(bp.model, vpreq.part->alias->items[i].model, namelen);
				bp.model[namelen] = 0;

				break;
			}
		}
	}

	ret = spi_nor_init(snor, &vpreq, &bp);

out:
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nor_probe_init(struct spi_nor *snor)
{
	struct spi_nor_vendor_part vp = { 0 };
	struct spi_nor_flash_part_blank bp;
	ufprog_status ret;
	bool sfdp_probed;
	char idstr[20];

	if (!snor)
		return UFP_INVALID_PARAMETER;

	spi_nor_reset_param(snor);

	STATUS_CHECK_RET(spi_nor_pre_init(snor));

	ufprog_spi_nor_bus_lock(snor);

	spi_nor_probe_jedec_id(snor, &vp);

	if (vp.part && (vp.part->flags & SNOR_F_NO_OP)) {
		logm_err("This part does not support auto probing. Please manually select a matched part\n");
		return UFP_FLASH_PART_NOT_SPECIFIED;
	}

	spi_nor_prepare_blank_part(&bp, vp.part);

	/* Read SFDP. This is mandatory if JEDEC ID probing failed. */
	sfdp_probed = spi_nor_probe_sfdp(snor, vp.vendor, &bp);

	if ((!vp.part || !vp.part->size) && !sfdp_probed) {
		logm_errdbg("Unable to identify SPI-NOR chip\n");
		ret = UFP_FLASH_PART_NOT_RECOGNISED;
		goto out;
	}

	if (sfdp_probed) {
		if (!vp.part) {
			/*
			 * Only SFDP was probed and we havn't got the correct JEDEC ID here.
			 * Since the SFDP probing was successful, we already know the correct bus width of CMD phase,
			 * we can simply read out the JEDEC ID without trying.
			 * Only read default 3 bytes.
			 */
			snor->param.id.len = SPI_NOR_DFL_ID_LEN;

			ret = spi_nor_read_id(snor, SNOR_CMD_READ_ID, snor->param.id.id, snor->param.id.len, 0);

			if (!ret && snor->param.id.id[0] == snor->param.id.id[1] &&
			    snor->param.id.id[1] == snor->param.id.id[2] && snor->state.cmd_buswidth_curr > 1) {
				ret = spi_nor_read_id(snor, SNOR_CMD_READ_ID_MULTI, snor->param.id.id,
						      snor->param.id.len, 0);
			}

			if (ret) {
				logm_err("Unable to read correct JEDEC ID\n");
				goto out;
			}

			bin_to_hex_str(idstr, sizeof(idstr), snor->param.id.id, snor->param.id.len, true, true);
			logm_dbg("JEDEC ID: %s\n", idstr);
		}

		if (!vp.vendor)
			vp.vendor = spi_nor_find_vendor(snor->param.id.id[0]);

		spi_nor_locate_sfdp_vendor(snor, snor->param.id.id[0], true);
	}

	ret = spi_nor_init(snor, &vp, &bp);

out:
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}

ufprog_status spi_nor_reprobe_part(struct spi_nor *snor, struct spi_nor_vendor_part *vp,
				   struct spi_nor_flash_part_blank *bp, const struct spi_nor_vendor *vendor,
				   const char *part)
{
	struct spi_nor_vendor_part nvp;
	struct spi_nor_id id;

	if (!spi_nor_find_vendor_part_by_name(part, &nvp)) {
		logm_err("Failed to find part %s\n", part);
		return UFP_FAIL;
	}

	memcpy(vp, &nvp, sizeof(nvp));

	logm_dbg("Reprobing as %s\n", part);

	/* Keep original JEDEC ID */
	memcpy(&id, &bp->p.id, sizeof(id));

	spi_nor_prepare_blank_part(bp, vp->part);

	/* Restore JEDEC ID */
	memcpy(&bp->p.id, &id, sizeof(id));

	if (!vendor) {
		vendor = vp->vendor_init;
		if (!vendor)
			vendor = vp->vendor;
	}

	if (spi_nor_probe_sfdp(snor, vendor, bp))
		spi_nor_locate_sfdp_vendor(snor, snor->param.id.id[0], true);

	if (vp->part->fixups && vp->part->fixups->pre_param_setup)
		STATUS_CHECK_RET(vp->part->fixups->pre_param_setup(snor, vp, bp));

	return UFP_OK;
}

ufprog_bool UFPROG_API ufprog_spi_nor_valid(struct spi_nor *snor)
{
	if (!snor)
		return false;

	return snor->param.size > 0;
}

uint32_t UFPROG_API ufprog_spi_nor_flash_param_signature(struct spi_nor *snor)
{
	uint32_t crc = 0;

	if (!snor)
		return 0;

	if (!snor->param.size)
		return 0;

	crc = crc32(crc, &snor->param, sizeof(snor->param));

	if (snor->sfdp.data)
		crc = crc32(crc, snor->sfdp.data, snor->sfdp.size);

	if (snor->ext_param.otp)
		crc = crc32(crc, snor->ext_param.otp, sizeof(*snor->ext_param.otp));

	return crc;
}

ufprog_status UFPROG_API ufprog_spi_nor_info(struct spi_nor *snor, struct spi_nor_info *info)
{
	uint32_t i;

	if (!snor || !info)
		return UFP_INVALID_PARAMETER;

	memset(info, 0, sizeof(*info));

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	info->signature = ufprog_spi_nor_flash_param_signature(snor);
	info->vendor = snor->param.vendor;
	info->model = snor->param.model;
	info->size = snor->param.size;
	info->ndies = snor->param.ndies;
	info->page_size = snor->param.page_size;
	info->max_speed = snor->param.max_speed;
	info->read_io_info = snor->state.read_io_info;
	info->pp_io_info = snor->state.pp_io_info;
	info->cmd_bw = snor->state.cmd_buswidth;
	info->erase_regions = snor->ext_param.erase_regions;
	info->num_erase_regions = snor->ext_param.num_erase_regions;
	info->otp_erasable = snor->ext_param.ops.otp ? (snor->ext_param.ops.otp->erase ? true : false) : false;
	info->otp = snor->ext_param.ops.otp ? snor->ext_param.otp : NULL;
	info->regs = snor->ext_param.regs;

	if (snor->sfdp.size) {
		info->sfdp_data = snor->sfdp.data;
		info->sfdp_size = snor->sfdp.size;
	}

	memcpy(&info->id, &snor->param.id, sizeof(info->id));

	for (i = 0; i < SPI_NOR_MAX_ERASE_INFO; i++)
		info->erasesizes[i] = snor->param.erase_info.info[i].size;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nor_select_die(struct spi_nor *snor, uint32_t index)
{
	ufprog_status ret;

	if (!snor)
		return UFP_INVALID_PARAMETER;

	if (index == snor->state.curr_die)
		return UFP_OK;

	ret = spi_nor_select_die(snor, (uint8_t)index);
	if (!ret)
		snor->state.curr_die = index;

	return ret;
}

ufprog_status spi_nor_set_bus_width(struct spi_nor *snor, uint8_t buswidth)
{
	if (buswidth != snor->state.cmd_buswidth_curr) {
		switch (snor->state.cmd_buswidth_curr) {
		case 2:
			STATUS_CHECK_RET(spi_nor_dpi_control(snor, false));
			if (snor->ext_param.ops.setup_dpi)
				STATUS_CHECK_RET(snor->ext_param.ops.setup_dpi(snor, false));
			break;

		case 4:
			STATUS_CHECK_RET(spi_nor_qpi_control(snor, false));
			if (snor->ext_param.ops.setup_qpi)
				STATUS_CHECK_RET(snor->ext_param.ops.setup_qpi(snor, false));
		}

		switch (buswidth) {
		case 2:
			STATUS_CHECK_RET(spi_nor_dpi_control(snor, true));
			if (snor->ext_param.ops.setup_dpi)
				STATUS_CHECK_RET(snor->ext_param.ops.setup_dpi(snor, true));
			break;

		case 4:
			STATUS_CHECK_RET(spi_nor_qpi_control(snor, true));
			if (snor->ext_param.ops.setup_qpi)
				STATUS_CHECK_RET(snor->ext_param.ops.setup_qpi(snor, true));
		}

		snor->state.cmd_buswidth_curr = buswidth;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nor_set_bus_width(struct spi_nor *snor, uint8_t buswidth)
{
	if (!snor)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	return spi_nor_set_bus_width(snor, buswidth);
}

ufprog_status spi_nor_wait_busy(struct spi_nor *snor, uint32_t wait_ms)
{
	uint64_t tmo = os_get_timer_us() + wait_ms * 1000;
	uint8_t sr;

	do {
		STATUS_CHECK_RET(spi_nor_read_sr(snor, &sr));

		if (!(sr & SR_BUSY))
			break;
	} while (os_get_timer_us() <= tmo);

	/* Last check */
	if (sr & SR_BUSY)
		STATUS_CHECK_RET(spi_nor_read_sr(snor, &sr));

	if (!(sr & SR_BUSY))
		return UFP_OK;

	logm_err("Timed out waiting for flash idle\n");

	return UFP_TIMEOUT;
}

ufprog_status UFPROG_API ufprog_spi_nor_read_no_check(struct spi_nor *snor, uint64_t addr, size_t len, void *data)
{
	ufprog_status ret = UFP_OK;
	uint64_t chklen;

	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(snor->state.read_opcode, spi_mem_io_info_cmd_bw(snor->state.read_io_info)),
		SPI_MEM_OP_ADDR(snor->state.naddr, addr, spi_mem_io_info_addr_bw(snor->state.read_io_info)),
		SPI_MEM_OP_DUMMY(snor->state.read_ndummy, spi_mem_io_info_addr_bw(snor->state.read_io_info)),
		SPI_MEM_OP_DATA_IN(len, data, spi_mem_io_info_data_bw(snor->state.read_io_info))
	);

	if (snor->ext_param.pre_read_hook)
		STATUS_CHECK_RET(snor->ext_param.pre_read_hook(snor, addr, len, data));

	STATUS_CHECK_RET(spi_nor_set_high_speed(snor));

	while (len) {
		if (snor->state.die_read_granularity) {
			chklen = snor->state.die_read_granularity - op.addr.val % snor->state.die_read_granularity;
			if (op.data.len > chklen)
				op.data.len = chklen;
		}

		STATUS_CHECK_GOTO_RET(spi_nor_setup_addr(snor, &op.addr.val), ret, out);
		STATUS_CHECK_GOTO_RET(ufprog_spi_mem_adjust_op_size(snor->spi, &op), ret, out);
		STATUS_CHECK_GOTO_RET(ufprog_spi_mem_exec_op(snor->spi, &op), ret, out);

		op.data.buf.rx = (void *)((uintptr_t)op.data.buf.rx + op.data.len);

		addr += op.data.len;
		op.addr.val = addr;

		len -= op.data.len;
		op.data.len = len;
	}

out:
	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nor_read(struct spi_nor *snor, uint64_t addr, size_t len, void *data)
{
	ufprog_status ret;

	if (!snor || (len && !data))
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (addr >= snor->param.size || addr + len > snor->param.size)
		return UFP_FLASH_ADDRESS_OUT_OF_RANGE;

	ufprog_spi_nor_bus_lock(snor);

	STATUS_CHECK_GOTO_RET(spi_nor_set_bus_width(snor, spi_mem_io_info_cmd_bw(snor->state.read_io_info)), ret, out);
	STATUS_CHECK_GOTO_RET(ufprog_spi_nor_read_no_check(snor, addr, len, data), ret, out);
	STATUS_CHECK_GOTO_RET(spi_nor_set_bus_width(snor, snor->state.cmd_buswidth), ret, out);

out:
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}

static ufprog_status spi_nor_page_program(struct spi_nor *snor, uint64_t addr, size_t len, const void *data,
					  size_t *retlen)
{
	size_t proglen;

	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(snor->state.pp_opcode, spi_mem_io_info_cmd_bw(snor->state.pp_io_info)),
		SPI_MEM_OP_ADDR(snor->state.naddr, addr, spi_mem_io_info_addr_bw(snor->state.pp_io_info)),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_DATA_OUT(0, data, spi_mem_io_info_data_bw(snor->state.pp_io_info))
	);

	proglen = snor->param.page_size - (addr & (snor->param.page_size - 1));
	if (proglen > len)
		proglen = len;

	len = proglen;
	op.data.len = proglen;

	while (proglen) {
		STATUS_CHECK_RET(spi_nor_setup_addr(snor, &op.addr.val));

		STATUS_CHECK_RET(spi_nor_data_write_enable(snor));

		STATUS_CHECK_RET(ufprog_spi_mem_adjust_op_size(snor->spi, &op));

		STATUS_CHECK_RET(spi_nor_set_high_speed(snor));
		STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));
		STATUS_CHECK_RET(spi_nor_set_low_speed(snor));

		STATUS_CHECK_RET(spi_nor_wait_busy(snor, snor->param.max_pp_time_ms));

		op.data.buf.tx = (const void *)((uintptr_t)op.data.buf.tx + op.data.len);

		addr += op.data.len;
		op.addr.val = addr;

		proglen -= op.data.len;
		op.data.len = proglen;
	}

	if (retlen)
		*retlen = len;

	return UFP_OK;
}

static ufprog_status spi_nor_byte_program(struct spi_nor *snor, uint64_t addr, const void *data)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_PAGE_PROG, 1),
		SPI_MEM_OP_ADDR(snor->state.naddr, addr, 1),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_DATA_OUT(1, data, 1)
	);

	STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));
	STATUS_CHECK_RET(spi_nor_wait_busy(snor, SNOR_PP_TIMEOUT_MS));

	return UFP_OK;
}

static ufprog_status spi_nor_word_program(struct spi_nor *snor, uint64_t addr, const void *data, bool first)
{
	size_t len = 2;

	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(SNOR_CMD_AAI_WP, 1),
		SPI_MEM_OP_NO_ADDR,
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_DATA_OUT(2, data, 1)
	);

	if (first) {
		op.addr.buswidth = 1;
		op.addr.len = snor->state.naddr;
		op.addr.val = addr;
	}

	while (len) {
		STATUS_CHECK_RET(ufprog_spi_mem_adjust_op_size(snor->spi, &op));
		STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));
		STATUS_CHECK_RET(spi_nor_wait_busy(snor, SNOR_PP_TIMEOUT_MS));

		op.data.buf.tx = (const void *)((uintptr_t)op.data.buf.tx + op.data.len);

		addr += op.data.len;
		op.addr.val = addr;

		len -= op.data.len;
		op.data.len = len;
	}

	return UFP_OK;
}

static ufprog_status spi_nor_aai_write(struct spi_nor *snor, uint64_t addr, size_t len, const void *data,
				       size_t *retlen)
{
	ufprog_status ret = UFP_OK;
	size_t rlen = len;
	bool first = true;

	STATUS_CHECK_RET(spi_nor_set_high_speed(snor));

	if (addr % 2) {
		STATUS_CHECK_GOTO_RET(spi_nor_write_enable(snor), ret, out);
		STATUS_CHECK_GOTO_RET(spi_nor_byte_program(snor, addr, data), ret, out);

		len--;
		addr++;
		data = (const void *)((uintptr_t)data + 1);
	}

	if (len >= 2) {
		STATUS_CHECK_GOTO_RET(spi_nor_write_enable(snor), ret, out);

		while (len >= 2) {
			STATUS_CHECK_GOTO_RET(spi_nor_word_program(snor, addr, data, first), ret, out);

			len -= 2;
			addr += 2;
			data = (const void *)((uintptr_t)data + 2);
			first = false;
		}

		STATUS_CHECK_GOTO_RET(spi_nor_write_disable(snor), ret, out);
		STATUS_CHECK_GOTO_RET(spi_nor_wait_busy(snor, SNOR_PP_TIMEOUT_MS), ret, out);
	}

	if (len) {
		STATUS_CHECK_GOTO_RET(spi_nor_write_enable(snor), ret, out);
		STATUS_CHECK_GOTO_RET(spi_nor_byte_program(snor, addr, data), ret, out);
	}

	if (retlen)
		*retlen = rlen;

out:
	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nor_write_page_no_check(struct spi_nor *snor, uint64_t addr, size_t len,
							    const void *data, size_t *retlen)
{
	return snor->ext_param.write_page(snor, addr, len, data, retlen);
}

ufprog_status UFPROG_API ufprog_spi_nor_write_page(struct spi_nor *snor, uint64_t addr, size_t len, const void *data,
						   size_t *retlen)
{
	ufprog_status ret;

	if (!snor || (len && !data))
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (addr >= snor->param.size || addr + len > snor->param.size)
		return UFP_FLASH_ADDRESS_OUT_OF_RANGE;

	ufprog_spi_nor_bus_lock(snor);

	STATUS_CHECK_GOTO_RET(spi_nor_set_bus_width(snor, spi_mem_io_info_cmd_bw(snor->state.pp_io_info)), ret, out);
	STATUS_CHECK_GOTO_RET(snor->ext_param.write_page(snor, addr, len, data, retlen), ret, out);
	STATUS_CHECK_GOTO_RET(spi_nor_set_bus_width(snor, snor->state.cmd_buswidth), ret, out);

out:
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nor_write(struct spi_nor *snor, uint64_t addr, size_t len, const void *data)
{
	const uint8_t *p = data;
	ufprog_status ret;
	size_t retlen;

	if (!snor || (len && !data))
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (addr >= snor->param.size || addr + len > snor->param.size)
		return UFP_FLASH_ADDRESS_OUT_OF_RANGE;

	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, spi_mem_io_info_cmd_bw(snor->state.pp_io_info)));

	while (len) {
		STATUS_CHECK_GOTO_RET(snor->ext_param.write_page(snor, addr, len, p, &retlen), ret, out);

		addr += retlen;
		p += retlen;
		len -= retlen;
	}

out:
	STATUS_CHECK_RET(spi_nor_set_bus_width(snor, snor->state.cmd_buswidth));

	return UFP_OK;
}

static ufprog_status spi_nor_erase_block(struct spi_nor *snor, uint64_t addr, const struct spi_nor_erase_sector_info *ei)
{
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(ei->opcode, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_ADDR(snor->state.naddr, addr, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_NO_DUMMY,
		SPI_MEM_OP_NO_DATA
	);

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));
	STATUS_CHECK_RET(spi_nor_setup_addr(snor, &op.addr.val));
	STATUS_CHECK_RET(spi_nor_data_write_enable(snor));
	STATUS_CHECK_RET(ufprog_spi_mem_exec_op(snor->spi, &op));
	STATUS_CHECK_RET(spi_nor_wait_busy(snor, ei->max_erase_time_ms));

	return UFP_OK;
}

static const struct spi_nor_erase_region *spi_nor_get_erase_region_at(struct spi_nor *snor, uint64_t addr,
								      uint64_t *ret_region_offset)
{
	uint64_t region_offset = 0;
	uint32_t i;

	if (snor->ext_param.num_erase_regions == 1) {
		if (ret_region_offset)
			*ret_region_offset = 0;
		return &snor->ext_param.erase_regions[0];
	}

	for (i = 0; i < snor->ext_param.num_erase_regions; i++) {
		if (addr >= region_offset && addr < region_offset + snor->ext_param.erase_regions[i].size) {
			if (ret_region_offset)
				*ret_region_offset = region_offset;
			return &snor->ext_param.erase_regions[i];
		}

		region_offset += snor->ext_param.erase_regions[i].size;
	}

	return NULL;
}

const struct spi_nor_erase_region *UFPROG_API ufprog_spi_nor_get_erase_region_at(struct spi_nor *snor, uint64_t addr)
{
	if (!snor)
		return NULL;

	if (!snor->ext_param.num_erase_regions)
		return NULL;

	if (addr >= snor->param.size)
		return NULL;

	return spi_nor_get_erase_region_at(snor, addr, NULL);
}

ufprog_status UFPROG_API ufprog_spi_nor_get_erase_range(struct spi_nor *snor, uint64_t addr, uint64_t len,
							uint64_t *retaddr_start, uint64_t *retaddr_end)
{
	const struct spi_nor_erase_region *erg;
	uint64_t region_base, n;

	if (!snor || !retaddr_start || !retaddr_end || !len)
		return UFP_INVALID_PARAMETER;

	if (!snor->ext_param.num_erase_regions)
		return UFP_UNSUPPORTED;

	if (addr >= snor->param.size)
		return UFP_INVALID_PARAMETER;

	/* Calculate start addr */
	erg = spi_nor_get_erase_region_at(snor, addr, &region_base);
	if (!erg)
		return UFP_UNSUPPORTED;

	if (is_power_of_2(erg->min_erasesize)) {
		*retaddr_start = addr & ~((uint64_t)erg->min_erasesize - 1);
	} else {
		n = (addr - region_base) / erg->min_erasesize;
		*retaddr_start = region_base + n * erg->min_erasesize;
	}

	/* Calculate end addr */
	erg = spi_nor_get_erase_region_at(snor, addr + len - 1, &region_base);
	if (!erg)
		return UFP_UNSUPPORTED;

	if (is_power_of_2(erg->min_erasesize)) {
		*retaddr_end = ((addr + len + erg->min_erasesize - 1) & ~((uint64_t)erg->min_erasesize - 1));
	} else {
		n = (addr + len - region_base) / erg->min_erasesize;
		*retaddr_end = region_base + n * erg->min_erasesize;
	}

	return UFP_OK;
}

static ufprog_status spi_nor_erase_at(struct spi_nor *snor, uint64_t addr, uint64_t maxlen, uint32_t *ret_eraseszie)
{
	const struct spi_nor_erase_sector_info *ei = NULL;
	uint64_t erase_start, erase_end, region_base, n;
	const struct spi_nor_erase_region *erg;
	uint32_t i, erasesize, len_erased = 0;
	ufprog_status ret = UFP_OK;

	erg = spi_nor_get_erase_region_at(snor, addr, &region_base);
	if (!erg)
		return UFP_UNSUPPORTED;

	if (is_power_of_2(erg->min_erasesize)) {
		erase_start = addr & ~((uint64_t)erg->min_erasesize - 1);
		erase_end = (addr + maxlen) & ~((uint64_t)erg->min_erasesize - 1);
	} else {
		n = (addr - region_base) / erg->min_erasesize;
		erase_start = region_base + n * erg->min_erasesize;

		n = (addr + maxlen - region_base) / erg->min_erasesize;
		erase_end = region_base + n * erg->min_erasesize;
	}

	if (erase_end > region_base + erg->size)
		erase_end = region_base + erg->size;

	for (i = 0; i < SPI_NOR_MAX_ERASE_INFO; i++) {
		if (!(erg->erasesizes_mask & BIT(i)))
			continue;

		erasesize = snor->param.erase_info.info[i].size;
		if (erasesize > erg->size)
			erasesize = (uint32_t)erg->size;

		if (is_power_of_2(erasesize)) {
			if (erase_start & (erasesize - 1))
				continue;
		} else {
			if ((erase_start - region_base) % erasesize)
				continue;
		}

		if (erase_end - erase_start < erasesize)
			continue;

		if (!ei || erasesize > ei->size)
			ei = &snor->param.erase_info.info[i];
	}

	if (ei) {
		ret = spi_nor_erase_block(snor, erase_start, ei);
		if (ret) {
			logm_err("Failed to erase at 0x%" PRIx64 ", erase size 0x%x\n", erase_start, ei->size);
		} else {
			if (ei->size > erase_end - erase_start)
				len_erased = (uint32_t)(erase_end - erase_start);
			else
				len_erased = ei->size;
		}
	}

	*ret_eraseszie = len_erased;

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nor_erase_at(struct spi_nor *snor, uint64_t addr, uint64_t maxlen,
						 uint32_t *ret_eraseszie)
{
	ufprog_status ret;

	if (!snor || !maxlen || !ret_eraseszie)
		return UFP_INVALID_PARAMETER;

	ufprog_spi_nor_bus_lock(snor);
	ret = spi_nor_erase_at(snor, addr, maxlen, ret_eraseszie);
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nor_erase(struct spi_nor *snor, uint64_t addr, uint64_t len)
{
	ufprog_status ret = UFP_OK;
	uint64_t start, end;
	uint32_t size;

	if (!snor)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (addr >= snor->param.size || addr + len > snor->param.size)
		return UFP_FLASH_ADDRESS_OUT_OF_RANGE;

	ret = ufprog_spi_nor_get_erase_range(snor, addr, len, &start, &end);
	if (ret) {
		logm_err("Failed to calculate erase region\n");
		return ret;
	}

	ufprog_spi_nor_bus_lock(snor);

	while (start < end) {
		ret = spi_nor_erase_at(snor, start, end - start, &size);
		if (ret || !size)
			break;

		start += size;
	}

	ufprog_spi_nor_bus_unlock(snor);

	if (start != end) {
		logm_err("Erase not complete. 0x" PRIx64 " remained\n", end - start);
		return UFP_FAIL;
	}

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nor_read_uid(struct spi_nor *snor, void *data, uint32_t *retlen)
{
	ufprog_status ret;

	if (!snor)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	if (!snor->ext_param.ops.read_uid)
		return UFP_UNSUPPORTED;

	ufprog_spi_nor_bus_lock(snor);
	ret = snor->ext_param.ops.read_uid(snor, data, retlen);
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}
