// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash register access definitions
 */

#include <stdbool.h>
#include <ufprog/log.h>
#include <ufprog/bits.h>
#include <ufprog/spi-nor-opcode.h>
#include "core.h"
#include "regs.h"

const struct spi_nor_reg_access sr_acc = SNOR_REG_ACC_NORMAL_SR(SNOR_CMD_READ_SR, SNOR_CMD_WRITE_SR);
const struct spi_nor_reg_access cr_acc = SNOR_REG_ACC_NORMAL(SNOR_CMD_READ_CR, SNOR_CMD_WRITE_CR);
const struct spi_nor_reg_access sr3_acc = SNOR_REG_ACC_NORMAL(SNOR_CMD_READ_SR3, SNOR_CMD_WRITE_SR3);
const struct spi_nor_reg_access srcr_acc = SNOR_REG_ACC_SRCR(SNOR_CMD_READ_SR, SNOR_CMD_READ_CR, SNOR_CMD_WRITE_SR);
const struct spi_nor_reg_access ear_acc = SNOR_REG_ACC_NORMAL(SNOR_CMD_READ_EAR, SNOR_CMD_WRITE_EAR);
const struct spi_nor_reg_access br_acc = SNOR_REG_ACC_NORMAL(SNOR_CMD_READ_BANK, SNOR_CMD_WRITE_BANK);
const struct spi_nor_reg_access scur_acc = SNOR_REG_ACC_NORMAL(SNOR_CMD_READ_SCUR, SNOR_CMD_WRITE_SCUR);
const struct spi_nor_reg_access nvcr_acc = SNOR_REG_ACC_XVCR(SNOR_CMD_READ_NVCR, SNOR_CMD_WRITE_NVCR, 2);
const struct spi_nor_reg_access vcr_acc = SNOR_REG_ACC_XVCR(SNOR_CMD_READ_VCR, SNOR_CMD_WRITE_VCR, 1);
const struct spi_nor_reg_access evcr_acc = SNOR_REG_ACC_XVCR(SNOR_CMD_READ_EVCR, SNOR_CMD_WRITE_EVCR, 1);

const struct spi_nor_reg_access cr2v_800003h_acc = {
	.type = SNOR_REG_NORMAL,
	.num = 1,
	.desc[0] = { /* Must poll manually */
		.flags = SNOR_REGACC_F_ADDR_4B_MODE | SNOR_REGACC_F_DATA_ACC_TIMING | SNOR_REGACC_F_NO_POLL |
			 SNOR_REGACC_F_NO_WRDIS,
		.ndata = 1,
		.addr = 0x800003,
		.read_opcode = SNOR_CMD_READ_AR,
		.write_opcode = SNOR_CMD_WRITE_AR,
	},
};

static const struct spi_nor_reg_field_item w25q_sr_no_lb_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(6, 1, "SEC", "Sector Protect"),
	SNOR_REG_FIELD(7, 1, "SRP0", "Status Register Protect 0"),
	SNOR_REG_FIELD(8, 1, "SRP1", "Status Register Protect 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
};

static const struct spi_nor_reg_def w25q_sr_no_lb = SNOR_REG_DEF("SR", "Status Register", &srcr_acc,
								 w25q_sr_no_lb_fields);

const struct snor_reg_info w25q_no_lb_regs = SNOR_REG_INFO(&w25q_sr_no_lb);

static const struct spi_nor_reg_field_item w25q_sr_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(6, 1, "SEC", "Sector Protect"),
	SNOR_REG_FIELD(7, 1, "SRP0", "Status Register Protect 0"),
	SNOR_REG_FIELD(8, 1, "SRP1", "Status Register Protect 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(9, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(11, 1, "LB1", "Security Register Lock Bit 1"),
	SNOR_REG_FIELD(12, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(13, 1, "LB3", "Security Register Lock Bit 3"),
	SNOR_REG_FIELD(14, 1, "CMP", "Complement Protect"),
};

static const struct spi_nor_reg_def w25q_sr = SNOR_REG_DEF("SR", "Status Register", &srcr_acc, w25q_sr_fields);

const struct snor_reg_info w25q_regs = SNOR_REG_INFO(&w25q_sr);

static const struct spi_nor_reg_field_item w25q_sr1_fields[] = {
	SNOR_REG_FIELD(2, 1, "BP0", "Block Protect Bit 0"),
	SNOR_REG_FIELD(3, 1, "BP1", "Block Protect Bit 1"),
	SNOR_REG_FIELD(4, 1, "BP2", "Block Protect Bit 2"),
	SNOR_REG_FIELD(5, 1, "TB", "Top/Bottom Block Protect"),
	SNOR_REG_FIELD(6, 1, "SEC", "Sector Protect"),
	SNOR_REG_FIELD(7, 1, "SRP0", "Status Register Protect 0"),
};

const struct spi_nor_reg_def w25q_sr1 = SNOR_REG_DEF("SR1", "Status Register 1", &sr_acc, w25q_sr1_fields);

static const struct spi_nor_reg_field_item w25q_sr2_fields[] = {
	SNOR_REG_FIELD(0, 1, "SRP1", "Status Register Protect 1"),
	SNOR_REG_FIELD_ENABLED_DISABLED(1, 1, "QE", "Quad Enable"),
	SNOR_REG_FIELD(3, 1, "LB1", "Security Register Lock Bit 1"),
	SNOR_REG_FIELD(4, 1, "LB2", "Security Register Lock Bit 2"),
	SNOR_REG_FIELD(5, 1, "LB3", "Security Register Lock Bit 3"),
	SNOR_REG_FIELD(6, 1, "CMP", "Complement Protect"),
};

const struct spi_nor_reg_def w25q_sr2 = SNOR_REG_DEF("SR2", "Status Register 2", &cr_acc, w25q_sr2_fields);

static const struct spi_nor_reg_field_item w25q_sr3_fields[] = {
	SNOR_REG_FIELD_FULL(2, 1, "WPS", "Write Protection Selection", &w25q_sr3_wps_values),
	SNOR_REG_FIELD_FULL(5, 3, "DRV", "Output Driver Stringth", &w25q_sr3_drv_values),
	SNOR_REG_FIELD_FULL(7, 1, "HOLD/RST", "/HOLD or /RESET Function", &w25q_sr3_hold_rst_values),
};

const struct spi_nor_reg_def w25q_sr3 = SNOR_REG_DEF("SR3", "Status Register 3", &sr3_acc, w25q_sr3_fields);

const struct spi_nor_reg_field_values w25q_sr3_drv_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "100%"),
	VALUE_ITEM(1, "75%"),
	VALUE_ITEM(2, "50%"),
	VALUE_ITEM(3, "25%"),
);

const struct spi_nor_reg_field_values w25q_sr3_hold_rst_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "/HOLD"),
	VALUE_ITEM(1, "/RESET"),
);

const struct spi_nor_reg_field_values w25q_sr3_adp_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "3-Byte Address Mode"),
	VALUE_ITEM(1, "4-Byte Address Mode"),
);

const struct spi_nor_reg_field_values w25q_sr3_wps_values = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "Legacy BP Bits"),
	VALUE_ITEM(1, "Individual Block Lock Bits"),
);

uint32_t UFPROG_API ufprog_spi_nor_get_reg_bytes(const struct spi_nor_reg_access *access)
{
	uint32_t i, n = 0;

	for (i = 0; i < access->num; i++)
		n += access->desc[i].ndata;

	return n;
}

static ufprog_status spi_nor_read_reg_desc(struct spi_nor *snor, const struct spi_nor_reg_desc *desc, uint32_t *retval)
{
	uint8_t data[sizeof(*retval)];
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(desc->read_opcode, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_ADDR(desc->naddr, desc->addr, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_DUMMY(desc->ndummy_read, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_DATA_IN(desc->ndata, data, snor->state.cmd_buswidth_curr)
	);
	ufprog_status ret;

	if (desc->ndata > sizeof(data))
		return UFP_UNSUPPORTED;

	if (desc->flags & SNOR_REGACC_F_ADDR_4B_MODE)
		op.addr.len = snor->state.a4b_mode ? 4 : 3;

	if (desc->flags & SNOR_REGACC_F_DATA_ACC_TIMING) {
		if (spi_mem_io_info_cmd_bw(snor->state.read_io_info) == snor->state.cmd_buswidth_curr)
			op.dummy.len = snor->state.read_ndummy;
		else
			op.dummy.len = 1; /* We have to use 1 here */
	}

	if (!ufprog_spi_mem_supports_op(snor->spi, &op))
		return UFP_UNSUPPORTED;

	if (desc->flags & SNOR_REGACC_F_DATA_ACC_TIMING)
		STATUS_CHECK_RET(spi_nor_set_low_speed(snor));

	ret = ufprog_spi_mem_exec_op(snor->spi, &op);
	if (ret) {
		logm_err("Failed to read register using opcode %02X\n", desc->read_opcode);
		return ret;
	}

	if (desc->ndata == 1) {
		*retval = data[0];
		return UFP_OK;
	}

	if (desc->ndata == 2) {
		if (desc->flags & SNOR_REGACC_F_BIG_ENDIAN)
			*retval = data[1] | (uint32_t)data[0] << 8;
		else
			*retval = data[0] | (uint32_t)data[1] << 8;
		return UFP_OK;
	}

	if (desc->ndata == 3) {
		if (desc->flags & SNOR_REGACC_F_BIG_ENDIAN)
			*retval = data[2] | ((uint32_t)data[1] << 8) | ((uint32_t)data[0] << 8);
		else
			*retval = data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[1] << 24);
		return UFP_OK;
	}

	if (desc->flags & SNOR_REGACC_F_BIG_ENDIAN)
		*retval = data[3] | (uint32_t)data[2] << 8 | (uint32_t)data[1] << 16 | (uint32_t)data[0] << 24;
	else
		*retval = data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;

	return UFP_OK;
}

ufprog_status spi_nor_read_reg_acc(struct spi_nor *snor, const struct spi_nor_reg_access *access, uint32_t *retval)
{
	uint32_t i, val = 0, shift = 0, data;
	ufprog_status ret;

	if (access->pre_acc)
		STATUS_CHECK_RET(access->pre_acc(snor, access));

	for (i = 0; i < access->num; i++) {
		STATUS_CHECK_GOTO_RET(spi_nor_read_reg_desc(snor, &access->desc[i], &data), ret, out);

		if (access->read_big_endian) {
			val <<= shift;
			val |= data;
		} else {
			val |= data << shift;
		}

		shift += access->desc[i].ndata * 8;
	}

	*retval = val;

out:
	if (access->post_acc)
		return access->post_acc(snor, access);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_spi_nor_read_reg(struct spi_nor *snor, const struct spi_nor_reg_access *access,
						 uint32_t *retval)
{
	ufprog_status ret;

	if (!snor || !access || !retval)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	ufprog_spi_nor_bus_lock(snor);
	ret = spi_nor_read_reg_acc(snor, access, retval);
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}

static ufprog_status spi_nor_write_reg_desc(struct spi_nor *snor, const struct spi_nor_reg_desc *desc, uint32_t val,
					    uint32_t ndata, bool volatile_write)
{
	bool poll = false, wren = false;
	ufprog_status ret = UFP_OK;
	uint8_t data[sizeof(val)];
	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(desc->write_opcode, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_ADDR(desc->naddr, desc->addr, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_DUMMY(desc->ndummy_write, snor->state.cmd_buswidth_curr),
		SPI_MEM_OP_DATA_OUT(ndata, data, snor->state.cmd_buswidth_curr)
	);

	if (!ndata) {
		ndata = desc->ndata;
		op.data.len = ndata;
	}

	if (ndata > sizeof(data))
		return UFP_UNSUPPORTED;

	if (desc->flags & SNOR_REGACC_F_ADDR_4B_MODE)
		op.addr.len = snor->state.a4b_mode ? 4 : 3;

	if (!ufprog_spi_mem_supports_op(snor->spi, &op))
		return UFP_UNSUPPORTED;

	if (desc->flags & SNOR_REGACC_F_BIG_ENDIAN) {
		if (ndata == 1) {
			data[0] = val & 0xff;
		} else if (ndata == 2) {
			data[0] = (val >> 8) & 0xff;
			data[1] = val & 0xff;
		} else if (ndata == 3) {
			data[0] = (val >> 16) & 0xff;
			data[1] = (val >> 8) & 0xff;
			data[2] = val & 0xff;
		} else {
			data[0] = (val >> 24) & 0xff;
			data[1] = (val >> 16) & 0xff;
			data[2] = (val >> 8) & 0xff;
			data[3] = val & 0xff;
		}
	} else {
		data[0] = val & 0xff;

		if (ndata >= 2)
			data[1] = (val >> 8) & 0xff;

		if (ndata >= 3)
			data[2] = (val >> 16) & 0xff;

		if (ndata == 4)
			data[3] = (val >> 24) & 0xff;
	}

	if (volatile_write && !(desc->flags & SNOR_REGACC_F_VOLATILE_NO_WREN)) {
		if (desc->flags & SNOR_REGACC_F_SR) {
			if (snor->param.flags & SNOR_F_SR_VOLATILE_WREN_50H)
				STATUS_CHECK_RET(spi_nor_volatile_write_enable(snor));
			else if (snor->param.flags & (SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE))
				wren = true;
		} else if (desc->flags & SNOR_REGACC_F_VOLATILE_WREN_50H) {
			STATUS_CHECK_RET(spi_nor_volatile_write_enable(snor));
		} else if (desc->flags & SNOR_REGACC_F_HAS_VOLATILE_WR_OPCODE) {
			op.cmd.opcode = desc->write_opcode_volatile;
		}
	} else if (!volatile_write && !(desc->flags & SNOR_REGACC_F_NO_WREN)) {
		if (desc->flags & SNOR_REGACC_F_SR) {
			if (snor->param.flags & (SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE))
				wren = true;
			else if (snor->param.flags & SNOR_F_SR_VOLATILE_WREN_50H)
				STATUS_CHECK_RET(spi_nor_volatile_write_enable(snor));
		} else {
			wren = true;
		}
	}

	if (wren) {
		STATUS_CHECK_RET(spi_nor_write_enable(snor));
		poll = !(desc->flags & SNOR_REGACC_F_NO_POLL);
	}

	if (desc->flags & SNOR_REGACC_F_DATA_ACC_TIMING)
		STATUS_CHECK_GOTO_RET(spi_nor_set_low_speed(snor), ret, out);

	STATUS_CHECK_GOTO_RET(ufprog_spi_mem_exec_op(snor->spi, &op), ret, out);

	if (poll)
		ret = spi_nor_wait_busy(snor, snor->state.max_nvcr_pp_time_ms);

out:
	if (wren && !(desc->flags & SNOR_REGACC_F_NO_WRDIS))
		spi_nor_write_disable(snor);

	return ret;
}

ufprog_status spi_nor_write_reg_acc(struct spi_nor *snor, const struct spi_nor_reg_access *access, uint32_t val,
				    bool volatile_write)
{
	uint32_t i, bits, mask, data, shift = 0, ndata = 0;
	ufprog_status ret = UFP_OK;

	if (access->pre_acc)
		STATUS_CHECK_RET(access->pre_acc(snor, access));

	if (access->type == SNOR_REG_READ_MULTI_WRITE_ONCE) {
		for (i = 0; i < access->num; i++)
			ndata += access->desc[i].ndata;

		ret = spi_nor_write_reg_desc(snor, &access->desc[0], val, ndata, volatile_write);
		goto out;
	}

	if (access->write_big_endian) {
		for (i = 0; i < access->num; i++)
			shift += access->desc[i].ndata * 8;
	}

	for (i = 0; i < access->num; i++) {
		bits = access->desc[i].ndata * 8;
		mask = (1 << bits) - 1;

		if (access->write_big_endian) {
			shift -= bits;
			data = val >> shift;
			data &= mask;
		} else {
			data = val & mask;
			val >>= bits;
		}

		ret = spi_nor_write_reg_desc(snor, &access->desc[i], data, 0, volatile_write);
		if (ret)
			break;
	}

out:
	if (access->post_acc)
		ret =access->post_acc(snor, access);

	return ret;
}

ufprog_status UFPROG_API ufprog_spi_nor_write_reg(struct spi_nor *snor, const struct spi_nor_reg_access *access,
						  uint32_t val)
{
	ufprog_status ret;

	if (!snor || !access)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	ufprog_spi_nor_bus_lock(snor);
	ret = spi_nor_write_reg_acc(snor, access, val, false);
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}

ufprog_status spi_nor_update_reg_acc(struct spi_nor *snor, const struct spi_nor_reg_access *access, uint32_t clr,
				     uint32_t set, bool volatile_write)
{
	uint32_t val;

	STATUS_CHECK_RET(spi_nor_read_reg_acc(snor, access, &val));

	val &= ~clr;
	val |= set;

	return spi_nor_write_reg_acc(snor, access, val, volatile_write);
}

ufprog_status UFPROG_API ufprog_spi_nor_update_reg(struct spi_nor *snor, const struct spi_nor_reg_access *access,
						   uint32_t clr, uint32_t set)
{
	ufprog_status ret;

	if (!snor || !access)
		return UFP_INVALID_PARAMETER;

	if (!snor->param.size)
		return UFP_FLASH_NOT_PROBED;

	ufprog_spi_nor_bus_lock(snor);
	ret = spi_nor_update_reg_acc(snor, access, clr, set, false);
	ufprog_spi_nor_bus_unlock(snor);

	return ret;
}
