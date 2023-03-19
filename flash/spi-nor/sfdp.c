/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash SFDP processing
 */

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <ufprog/log.h>
#include <ufprog/sizes.h>
#include <ufprog/spi-nor-opcode.h>
#include "core.h"

static const uint32_t sector_erase_time_units_ms[] = { 1, 16, 128, 1000 };
static const uint32_t page_program_units_us[] = { 8, 64 };

ufprog_status spi_nor_read_sfdp(struct spi_nor *snor, uint8_t buswidth, uint32_t addr, uint32_t len, void *data)
{
	struct ufprog_spi_mem_op op = SNOR_READ_SFDP_OP(buswidth, addr, 3, len, data);

	if (snor->param.flags & SNOR_F_SFDP_4B_MODE)
		op.addr.len = snor->state.a4b_mode ? 4 : 3;

	return ufprog_spi_mem_exec_op(snor->spi, &op);
}

static ufprog_bool spi_nor_supports_read_sfdp(struct spi_nor *snor, uint8_t buswidth, uint32_t addr, uint32_t len)
{
	struct ufprog_spi_mem_op op = SNOR_READ_SFDP_OP(buswidth, addr, 3, len, NULL);

	if (snor->param.flags & SNOR_F_SFDP_4B_MODE)
		op.addr.len = snor->state.a4b_mode ? 4 : 3;

	return ufprog_spi_mem_supports_op(snor->spi, &op);
}

static ufprog_status spi_nor_read_full_sfdp(struct spi_nor *snor, uint32_t nph)
{
	uint32_t i, addr, off, end, last_addr = 0;
	struct sfdp_param_header sfdp_phdr;
	ufprog_status ret;

	for (i = 0; i < nph; i++) {
		addr = sizeof(struct sfdp_header) + i * sizeof(sfdp_phdr);

		ret = spi_nor_read_sfdp(snor, snor->state.cmd_buswidth_curr, addr, sizeof(sfdp_phdr), &sfdp_phdr);
		if (ret) {
			logm_err("Unable to read SFDP parameter header %u at 0x%x\n", i, addr);
			return ret;
		}

		if (i == 0 && sfdp_phdr.id_msb != SFDP_PARAM_ID_MSB_JEDEC &&
		    sfdp_phdr.id_lsb != SFDP_PARAM_ID_LSB_JEDEC_BFPT) {
			logm_err("Unsupported SFDP BFT header\n");
			return UFP_FAIL;
		}

		off = ((uint32_t)sfdp_phdr.ptr[0]) | (((uint32_t)sfdp_phdr.ptr[1]) << 8) |
		      (((uint32_t)sfdp_phdr.ptr[2]) << 16);

		if (off % 4) {
			logm_warn("Unsupported SFDP Parameter header (%02x%02x) pointer %xh\n", sfdp_phdr.id_msb,
				  sfdp_phdr.id_lsb, off);
			continue;
		}

		end = off + sfdp_phdr.len * 4;
		if (end > last_addr)
			last_addr = end;
	}

	snor->sfdp.size = last_addr;
	snor->sfdp.data = malloc(snor->sfdp.size);
	if (!snor->sfdp.size) {
		logm_err("No memory for SFDP data\n");
		return UFP_NOMEM;
	}

	return spi_nor_read_sfdp(snor, snor->state.cmd_buswidth_curr, 0, snor->sfdp.size, snor->sfdp.data);
}

static void spi_nor_parse_sfdp_init(struct spi_nor *snor)
{
	struct sfdp_param_header *phdr;
	struct sfdp_header *hdr;
	uint32_t i, off;

	hdr = (struct sfdp_header *)snor->sfdp.data;
	phdr = (struct sfdp_param_header *)((uintptr_t)snor->sfdp.data + sizeof(struct sfdp_header));

	for (i = 0; i <= hdr->nph; i++) {
		off = ((uint32_t)phdr[i].ptr[0]) | (((uint32_t)phdr[i].ptr[1]) << 8) |
		      (((uint32_t)phdr[i].ptr[2]) << 16);

		switch (phdr[i].id_msb) {
		case SFDP_PARAM_ID_MSB_JEDEC:
			switch (phdr[i].id_lsb) {
			case SFDP_PARAM_ID_LSB_JEDEC_BFPT:
				snor->sfdp.bfpt_hdr = &phdr[i];
				snor->sfdp.bfpt = (uint32_t *)((uintptr_t)snor->sfdp.data + off);
				snor->sfdp.bfpt_dw_num = phdr[i].len;

				logm_dbg("SFDP Basic Function Parameter Table %u.%u, %u DWORDs\n",
					 phdr[i].major_ver, phdr[i].minor_ver, phdr[i].len);
				break;

			case SFDP_PARAM_ID_LSB_JEDEC_4BAIT:
				snor->sfdp.a4bit_hdr = &phdr[i];
				snor->sfdp.a4bit = (uint32_t *)((uintptr_t)snor->sfdp.data + off);
				snor->sfdp.a4bit_dw_num = phdr[i].len;

				logm_dbg("SFDP 4-Byte Address Instruction Table %u.%u, %u DWORDs\n",
					 phdr[i].major_ver, phdr[i].minor_ver, phdr[i].len);
				break;

			case SFDP_PARAM_ID_LSB_JEDEC_SMPT:
				snor->sfdp.smpt_hdr = &phdr[i];
				snor->sfdp.smpt = (uint32_t *)((uintptr_t)snor->sfdp.data + off);
				snor->sfdp.smpt_dw_num = phdr[i].len;

				logm_dbg("SFDP Sector Map Parameter Table %u.%u, %u DWORDs\n",
					 phdr[i].major_ver, phdr[i].minor_ver, phdr[i].len);
				break;

			default:
				logm_notice("Unprocessed table %02x%02x rev %u.%u, %u DWORDs\n", phdr[i].id_msb,
					    phdr[i].id_lsb, phdr[i].major_ver, phdr[i].minor_ver, phdr[i].len);
			}
			break;

		default:
			logm_notice("Unprocessed table %02x%02x rev %u.%u, %u DWORDs\n", phdr[i].id_msb, phdr[i].id_lsb,
				    phdr[i].major_ver, phdr[i].minor_ver, phdr[i].len);
		}
	}
}

static ufprog_status spi_nor_parse_sfdp_fill(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	uint32_t dw, read_io_caps, pp_io_caps, flags, val, val2, nei;
	struct spi_nor_erase_info ei, ei4b;

	/* Flash size */
	dw = sfdp_dw(snor->sfdp.bfpt, 2);
	if (dw & BFPT_DW2_FLASH_SIZE_4G_ABOVE)
		bp->p.size = 1ULL << FIELD_GET(BFPT_DW2_FLASH_SIZE, dw);
	else
		bp->p.size = FIELD_GET(BFPT_DW2_FLASH_SIZE, dw) + 1;
	bp->p.size /= 8;

	/* I/O bus for read (1-1-1 is always supported) */
	read_io_caps = BIT_SPI_MEM_IO_1_1_1;
	memcpy(&bp->read_opcodes_3b[SPI_MEM_IO_1_1_1], &default_read_opcodes_3b[SPI_MEM_IO_1_1_1],
	       sizeof(default_read_opcodes_3b[0]));

	dw = sfdp_dw(snor->sfdp.bfpt, 1);

	/* 1-1-4 and 1-4-4 */
	val = sfdp_dw(snor->sfdp.bfpt, 3);

	if (dw & BFPT_DW1_SUPPORT_1S_1S_4S_FAST_READ) {
		read_io_caps |= BIT_SPI_MEM_IO_1_1_4;
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].opcode = (uint8_t)FIELD_GET(BFPT_DW3_1S_1S_4S_FAST_READ_OPCODE, val);
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].ndummy = (uint8_t)FIELD_GET(BFPT_DW3_1S_1S_4S_FAST_READ_DUMMY_CLKS, val);
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_4].nmode = (uint8_t)FIELD_GET(BFPT_DW3_1S_1S_4S_FAST_READ_MODE_CLKS, val);
	}

	if (dw & BFPT_DW1_SUPPORT_1S_4S_4S_FAST_READ) {
		read_io_caps |= BIT_SPI_MEM_IO_1_4_4;
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].opcode = (uint8_t)FIELD_GET(BFPT_DW3_1S_4S_4S_FAST_READ_OPCODE, val);
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].ndummy = (uint8_t)FIELD_GET(BFPT_DW3_1S_4S_4S_FAST_READ_DUMMY_CLKS, val);
		bp->read_opcodes_3b[SPI_MEM_IO_1_4_4].nmode = (uint8_t)FIELD_GET(BFPT_DW3_1S_4S_4S_FAST_READ_MODE_CLKS, val);
	}

	/* 1-1-2 and 1-2-2 */
	val = sfdp_dw(snor->sfdp.bfpt, 4);

	if (dw & BFPT_DW1_SUPPORT_1S_1S_2S_FAST_READ) {
		read_io_caps |= BIT_SPI_MEM_IO_1_1_2;
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].opcode = (uint8_t)FIELD_GET(BFPT_DW4_1S_1S_2S_FAST_READ_OPCODE, val);
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].ndummy = (uint8_t)FIELD_GET(BFPT_DW4_1S_1S_2S_FAST_READ_DUMMY_CLKS, val);
		bp->read_opcodes_3b[SPI_MEM_IO_1_1_2].nmode = (uint8_t)FIELD_GET(BFPT_DW4_1S_1S_2S_FAST_READ_MODE_CLKS, val);
	}

	if (dw & BFPT_DW1_SUPPORT_1S_2S_2S_FAST_READ) {
		read_io_caps |= BIT_SPI_MEM_IO_1_2_2;
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].opcode = (uint8_t)FIELD_GET(BFPT_DW4_1S_2S_2S_FAST_READ_OPCODE, val);
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].ndummy = (uint8_t)FIELD_GET(BFPT_DW4_1S_2S_2S_FAST_READ_DUMMY_CLKS, val);
		bp->read_opcodes_3b[SPI_MEM_IO_1_2_2].nmode = (uint8_t)FIELD_GET(BFPT_DW4_1S_2S_2S_FAST_READ_MODE_CLKS, val);
	}

	/* 2-2-2 and 4-4-4 */
	dw = sfdp_dw(snor->sfdp.bfpt, 5);

	if (dw & BFPT_DW5_SUPPORT_2S_2S_2S_FAST_READ) {
		val = sfdp_dw(snor->sfdp.bfpt, 6);

		read_io_caps |= BIT_SPI_MEM_IO_2_2_2;
		bp->read_opcodes_3b[SPI_MEM_IO_2_2_2].opcode = (uint8_t)FIELD_GET(BFPT_DW6_2S_2S_2S_FAST_READ_OPCODE, val);
		bp->read_opcodes_3b[SPI_MEM_IO_2_2_2].ndummy = (uint8_t)FIELD_GET(BFPT_DW6_2S_2S_2S_FAST_READ_DUMMY_CLKS, val);
		bp->read_opcodes_3b[SPI_MEM_IO_2_2_2].nmode = (uint8_t)FIELD_GET(BFPT_DW6_2S_2S_2S_FAST_READ_MODE_CLKS, val);
	}

	if (dw & BFPT_DW5_SUPPORT_4S_4S_4S_FAST_READ) {
		val = sfdp_dw(snor->sfdp.bfpt, 7);

		read_io_caps |= BIT_SPI_MEM_IO_4_4_4;
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].opcode = (uint8_t)FIELD_GET(BFPT_DW7_4S_4S_4S_FAST_READ_OPCODE, val);
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].ndummy = (uint8_t)FIELD_GET(BFPT_DW7_4S_4S_4S_FAST_READ_DUMMY_CLKS, val);
		bp->read_opcodes_3b[SPI_MEM_IO_4_4_4].nmode = (uint8_t)FIELD_GET(BFPT_DW7_4S_4S_4S_FAST_READ_MODE_CLKS, val);
	}

	bp->p.read_io_caps = read_io_caps;
	if (read_io_caps)
		bp->p.read_opcodes_3b = bp->read_opcodes_3b;

	/* I/O bus for page program (1-1-1 is always supported) */
	pp_io_caps = BIT_SPI_MEM_IO_1_1_1;
	memcpy(&bp->pp_opcodes_3b[SPI_MEM_IO_1_1_1], &default_pp_opcodes_3b[SPI_MEM_IO_1_1_1],
	       sizeof(default_pp_opcodes_3b[0]));

	bp->p.pp_io_caps = pp_io_caps;
	if (pp_io_caps)
		bp->p.pp_opcodes_3b = bp->pp_opcodes_3b;

	if (snor->sfdp.bfpt_dw_num >= 11) {
		/* Page size */
		dw = sfdp_dw(snor->sfdp.bfpt, 11);
		bp->p.page_size = 1 << FIELD_GET(BFPT_DW11_PAGE_SIZE_SHIFT, dw);

		/* Page program max size */
		bp->p.max_pp_time_us = 2 * (FIELD_GET(BFPT_DW11_PAGE_BYTE_PROG_MAX_TIME_MULTIPLIER, dw) + 1) *
			(FIELD_GET(BFPT_DW11_PAGE_PROG_TYP_TIME, dw) + 1) *
			page_program_units_us[FIELD_GET(BFPT_DW11_PAGE_PROG_TYP_TIME_UNIT, dw)];
	}

	if (snor->sfdp.bfpt_dw_num >= 15) {
		dw = sfdp_dw(snor->sfdp.bfpt, 15);

		/* QPI enable method */
		val = FIELD_GET(BFPT_DW15_4S_4S_4S_EN_SEQ, dw);
		if (val & DW15_4S_4S_4S_EN_SEQ_QER_38H)
			bp->p.qpi_en_type = QPI_EN_QER_38H;
		else if (val & DW15_4S_4S_4S_EN_SEQ_38H)
			bp->p.qpi_en_type = QPI_EN_38H;
		else if (val & DW15_4S_4S_4S_EN_SEQ_35H)
			bp->p.qpi_en_type = QPI_EN_35H;
		else if (val)
			logm_notice("QPI enable type defined in SFDP is not supported\n");

		/* QPI disable method */
		val = FIELD_GET(BFPT_DW15_4S_4S_4S_DIS_SEQ, dw);
		if (val & DW15_4S_4S_4S_DIS_SEQ_FFH)
			bp->p.qpi_dis_type = QPI_DIS_FFH;
		else if (val & DW15_4S_4S_4S_DIS_SEQ_F5H)
			bp->p.qpi_dis_type = QPI_DIS_F5H;
		else if (val)
			logm_notice("QPI disable type defined in SFDP is not supported\n");

		/* Quad-Enable method */
		val = FIELD_GET(BFPT_DW15_QE_REQ, dw);
		if (val == DW15_QE_REQ_NONE)
			bp->p.qe_type = QE_DONT_CARE;
		else if (val == DW15_QE_REQ_SR2_BIT1_WR_SR1 || val == DW15_QE_REQ_SR2_BIT1_WR_SR1_NC ||
			 val == DW15_QE_REQ_SR2_BIT1_WR_SR1_05H_35H_01H)
			bp->p.qe_type = QE_SR2_BIT1_WR_SR1;
		else if (val == DW15_QE_REQ_SR1_BIT6)
			bp->p.qe_type = QE_SR1_BIT6;
		else if (val == DW15_QE_REQ_SR2_BIT7)
			bp->p.qe_type = QE_SR2_BIT7;
		else if (val == DW15_QE_REQ_SR2_BIT1)
			bp->p.qe_type = QE_SR2_BIT1;
	}

	if (snor->sfdp.bfpt_dw_num >= 16) {
		dw = sfdp_dw(snor->sfdp.bfpt, 16);

		if (bp->p.size > SZ_16M) {
			/* 4-byte addressing */
			val = FIELD_GET(BFPT_DW16_ENTER_4B_CAPS, dw);
			val2 = FIELD_GET(BFPT_DW16_EXIT_4B_CAPS, dw);

			flags = 0;

			if ((val & DW16_ENTER_4B_B7H) && (val2 & DW16_EXIT_4B_E9H))
				flags |= SNOR_4B_F_B7H_E9H;

			if ((val & DW16_ENTER_4B_WREN_B7H) && (val2 & DW16_EXIT_4B_WREN_E9H))
				flags |= SNOR_4B_F_WREN_B7H_E9H;

			if ((val & DW16_ENTER_4B_EAR) && (val2 & DW16_EXIT_4B_EAR))
				flags |= SNOR_4B_F_EAR;

			if ((val & DW16_ENTER_4B_BANK) && (val2 & DW16_EXIT_4B_BANK))
				flags |= SNOR_4B_F_BANK;

			if (val & DW16_ENTER_4B_OPCODE)
				flags |= SNOR_4B_F_OPCODE;

			if (val & DW16_ENTER_4B_ALWAYS)
				flags |= SNOR_4B_F_ALWAYS;

			if (((val & DW16_ENTER_4B_NVCR) || (val2 & DW16_EXIT_4B_NVCR)) && !flags)
				logm_warn("Enabling/Disabling 4-byte addressing using NVCR is not supported\n");

			bp->p.a4b_flags = flags;

			if (!flags) {
				if (val & DW16_ENTER_4B_B7H)
					bp->p.a4b_en_type = A4B_EN_B7H;
				else if (val & DW16_ENTER_4B_WREN_B7H)
					bp->p.a4b_en_type = A4B_EN_WREN_B7H;
				else if (val & DW16_ENTER_4B_EAR)
					bp->p.a4b_en_type = A4B_EN_EAR;
				else if (val & DW16_ENTER_4B_BANK)
					bp->p.a4b_en_type = A4B_EN_BANK;
				else
					bp->p.a4b_en_type = A4B_EN_NONE;

				if (val2 & DW16_EXIT_4B_E9H)
					bp->p.a4b_dis_type = A4B_DIS_E9H;
				else if (val2 & DW16_EXIT_4B_WREN_E9H)
					bp->p.a4b_dis_type = A4B_DIS_WREN_E9H;
				else if (val2 & DW16_EXIT_4B_EAR)
					bp->p.a4b_dis_type = A4B_DIS_EAR;
				else if (val2 & DW16_EXIT_4B_BANK)
					bp->p.a4b_dis_type = A4B_DIS_BANK;
				else if (val2 & DW16_EXIT_4B_SOFT_RESET)
					bp->p.a4b_dis_type = A4B_DIS_66H_99H;
				else
					bp->p.a4b_dis_type = A4B_DIS_NONE;

				if (!bp->p.a4b_en_type) {
					logm_err("No method defined by SFDP for entering 4-byte addressing mode");
					return UFP_UNSUPPORTED;
				}
			}
		}

		/* Soft reset method */
		val = FIELD_GET(BFPT_DW16_SOFT_RESET_RESCUE_SEQ_CAPS, dw);
		flags = 0;

		if (val & DW16_SOFT_RESET_SEQ_DRIVE_FH_4IO_8CLKS)
			flags |= SNOR_SOFT_RESET_DRV_FH_4IO_8CLKS;

		if (val & DW16_SOFT_RESET_SEQ_4B_MODE_DRIVE_FH_4IO_10CLKS)
			flags |= SNOR_SOFT_RESET_DRV_FH_4IO_10CLKS_4B;

		if (val & DW16_SOFT_RESET_SEQ_DRIVE_FH_4IO_16CLKS)
			flags |= SNOR_SOFT_RESET_DRV_FH_4IO_16CLKS;

		if (val & DW16_SOFT_RESET_SEQ_F0H)
			flags |= SNOR_SOFT_RESET_OPCODE_F0H;

		if (val & DW16_SOFT_RESET_SEQ_66H_99H)
			flags |= SNOR_SOFT_RESET_OPCODE_66H_99H;

		bp->p.soft_reset_flags = flags;

		/* Status register protection bits */
		val = FIELD_GET(BFPT_DW16_SR1_WR_NV_CAPS, dw);

		if (val & DW16_SR1_MIXED_WREN_06H_REQ)
			bp->p.flags |= SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE;
		else if (val & DW16_SR1_NV_V_PWR_LAST_NV_WREN_06H_V_WREN_50H_REQ)
			bp->p.flags |= SNOR_F_SR_NON_VOLATILE | SNOR_F_SR_VOLATILE_WREN_50H;
		else if (val & DW16_SR1_V_PWR_ALL_1_WREN_50H_REQ)
			bp->p.flags |= SNOR_F_SR_VOLATILE_WREN_50H;
		else if (val & DW16_SR1_V_PWR_ALL_1_WREN_06H_REQ)
			bp->p.flags |= SNOR_F_SR_VOLATILE;
		else if (val & DW16_SR1_NV_PWR_LAST_WREN_06H_REQ)
			bp->p.flags |= SNOR_F_SR_NON_VOLATILE;
	}

	/* Erase type */
	memset(&ei, 0, sizeof(ei));
	nei = 0;

	dw = sfdp_dw(snor->sfdp.bfpt, 8);

	val = FIELD_GET(BFPT_DW8_ERASE_TYPE1_SIZE_SHIFT, dw);
	if (val) {
		ei.info[0].size = 1 << val;
		ei.info[0].opcode = (uint8_t)FIELD_GET(BFPT_DW8_ERASE_TYPE1_OPCODE, dw);
		nei++;
	}

	val = FIELD_GET(BFPT_DW8_ERASE_TYPE2_SIZE_SHIFT, dw);
	if (val) {
		ei.info[1].size = 1 << val;
		ei.info[1].opcode = (uint8_t)FIELD_GET(BFPT_DW8_ERASE_TYPE2_OPCODE, dw);
		nei++;
	}

	dw = sfdp_dw(snor->sfdp.bfpt, 9);

	val = FIELD_GET(BFPT_DW9_ERASE_TYPE3_SIZE_SHIFT, dw);
	if (val) {
		ei.info[2].size = 1 << val;
		ei.info[2].opcode = (uint8_t)FIELD_GET(BFPT_DW9_ERASE_TYPE3_OPCODE, dw);
		nei++;
	}

	val = FIELD_GET(BFPT_DW9_ERASE_TYPE4_SIZE_SHIFT, dw);
	if (val) {
		ei.info[3].size = 1 << val;
		ei.info[3].opcode = (uint8_t)FIELD_GET(BFPT_DW9_ERASE_TYPE4_OPCODE, dw);
		nei++;
	}

	/* Erase time */
	if (snor->sfdp.bfpt_dw_num >= 10) {
		dw = sfdp_dw(snor->sfdp.bfpt, 10);
		val2 = 2 * (FIELD_GET(BFPT_DW10_SECTOR_ERASE_MAX_TIME_MULTIPLIER, dw) + 1);

		if (ei.info[0].size) {
			ei.info[0].max_erase_time_ms = val2 * (FIELD_GET(BFPT_DW10_SECTOR_T1_ERASE_TYP_TIME, dw) + 1) *
				sector_erase_time_units_ms[FIELD_GET(BFPT_DW10_SECTOR_T1_ERASE_TYP_TIME_UNIT, dw)];
		}

		if (ei.info[1].size) {
			ei.info[1].max_erase_time_ms = val2 * (FIELD_GET(BFPT_DW10_SECTOR_T2_ERASE_TYP_TIME, dw) + 1) *
				sector_erase_time_units_ms[FIELD_GET(BFPT_DW10_SECTOR_T2_ERASE_TYP_TIME_UNIT, dw)];
		}

		if (ei.info[2].size) {
			ei.info[2].max_erase_time_ms = val2 * (FIELD_GET(BFPT_DW10_SECTOR_T3_ERASE_TYP_TIME, dw) + 1) *
				sector_erase_time_units_ms[FIELD_GET(BFPT_DW10_SECTOR_T3_ERASE_TYP_TIME_UNIT, dw)];
		}

		if (ei.info[3].size) {
			ei.info[3].max_erase_time_ms = val2 * (FIELD_GET(BFPT_DW10_SECTOR_T4_ERASE_TYP_TIME, dw) + 1) *
				sector_erase_time_units_ms[FIELD_GET(BFPT_DW10_SECTOR_T4_ERASE_TYP_TIME_UNIT, dw)];
		}
	}

	memcpy(&bp->erase_info_3b, &ei, sizeof(ei));
	if (nei) {
		bp->p.erase_info_3b = &bp->erase_info_3b;
	} else {
		if (snor->sfdp.smpt) {
			logm_err("No erase type defined for Sector Map in SFDP\n");
			return UFP_UNSUPPORTED;
		}
	}

	if (snor->sfdp.a4bit && bp->p.size > SZ_16M) {
		dw = sfdp_dw(snor->sfdp.a4bit, 1);
		memset(&ei4b, 0, sizeof(ei4b));

		if ((dw & A4BIT_DW1_SUPPORT_1S_1S_1S_FAST_READ) && (read_io_caps & BIT_SPI_MEM_IO_1_1_1)) {
			bp->p.read_opcodes_4b = bp->read_opcodes_4b;
			memcpy(&bp->read_opcodes_4b[SPI_MEM_IO_1_1_1], &bp->read_opcodes_3b[SPI_MEM_IO_1_1_1],
				sizeof(bp->read_opcodes_3b[SPI_MEM_IO_1_1_1]));
			bp->read_opcodes_4b[SPI_MEM_IO_1_1_1].opcode = SNOR_CMD_4B_FAST_READ;
		}

		if ((dw & A4BIT_DW1_SUPPORT_1S_1S_2S_FAST_READ) && (read_io_caps & BIT_SPI_MEM_IO_1_1_2)) {
			bp->p.read_opcodes_4b = bp->read_opcodes_4b;
			memcpy(&bp->read_opcodes_4b[SPI_MEM_IO_1_1_2], &bp->read_opcodes_3b[SPI_MEM_IO_1_1_2],
				sizeof(bp->read_opcodes_3b[SPI_MEM_IO_1_1_2]));
			bp->read_opcodes_4b[SPI_MEM_IO_1_1_2].opcode = SNOR_CMD_4B_FAST_READ_DUAL_OUT;
		}

		if ((dw & A4BIT_DW1_SUPPORT_1S_2S_2S_FAST_READ) && (read_io_caps & BIT_SPI_MEM_IO_1_2_2)) {
			bp->p.read_opcodes_4b = bp->read_opcodes_4b;
			memcpy(&bp->read_opcodes_4b[SPI_MEM_IO_1_2_2], &bp->read_opcodes_3b[SPI_MEM_IO_1_2_2],
				sizeof(bp->read_opcodes_3b[SPI_MEM_IO_1_2_2]));
			bp->read_opcodes_4b[SPI_MEM_IO_1_2_2].opcode = SNOR_CMD_4B_FAST_READ_DUAL_IO;
		}

		if ((dw & A4BIT_DW1_SUPPORT_1S_2S_2S_FAST_READ) && (read_io_caps & BIT_SPI_MEM_IO_2_2_2)) {
			bp->p.read_opcodes_4b = bp->read_opcodes_4b;
			memcpy(&bp->read_opcodes_4b[SPI_MEM_IO_2_2_2], &bp->read_opcodes_3b[SPI_MEM_IO_2_2_2],
				sizeof(bp->read_opcodes_3b[SPI_MEM_IO_2_2_2]));
			bp->read_opcodes_4b[SPI_MEM_IO_2_2_2].opcode = SNOR_CMD_4B_FAST_READ_DUAL_IO;
		}

		if ((dw & A4BIT_DW1_SUPPORT_1S_1S_4S_FAST_READ) && (read_io_caps & BIT_SPI_MEM_IO_1_1_4)) {
			bp->p.read_opcodes_4b = bp->read_opcodes_4b;
			memcpy(&bp->read_opcodes_4b[SPI_MEM_IO_1_1_4], &bp->read_opcodes_3b[SPI_MEM_IO_1_1_4],
				sizeof(bp->read_opcodes_3b[SPI_MEM_IO_1_1_4]));
			bp->read_opcodes_4b[SPI_MEM_IO_1_1_4].opcode = SNOR_CMD_4B_FAST_READ_QUAD_OUT;
		}

		if ((dw & A4BIT_DW1_SUPPORT_1S_4S_4S_FAST_READ) && (read_io_caps & BIT_SPI_MEM_IO_1_4_4)) {
			bp->p.read_opcodes_4b = bp->read_opcodes_4b;
			memcpy(&bp->read_opcodes_4b[SPI_MEM_IO_1_4_4], &bp->read_opcodes_3b[SPI_MEM_IO_1_4_4],
				sizeof(bp->read_opcodes_3b[SPI_MEM_IO_1_4_4]));
			bp->read_opcodes_4b[SPI_MEM_IO_1_4_4].opcode = SNOR_CMD_4B_FAST_READ_QUAD_IO;
		}

		if ((dw & A4BIT_DW1_SUPPORT_1S_4S_4S_FAST_READ) && (read_io_caps & BIT_SPI_MEM_IO_4_4_4)) {
			bp->p.read_opcodes_4b = bp->read_opcodes_4b;
			memcpy(&bp->read_opcodes_4b[SPI_MEM_IO_4_4_4], &bp->read_opcodes_3b[SPI_MEM_IO_4_4_4],
				sizeof(bp->read_opcodes_3b[SPI_MEM_IO_4_4_4]));
			bp->read_opcodes_4b[SPI_MEM_IO_4_4_4].opcode = SNOR_CMD_4B_FAST_READ_QUAD_IO;
		}

		if ((dw & A4BIT_DW1_SUPPORT_1S_1S_1S_PAGE_PROG) && (pp_io_caps & BIT_SPI_MEM_IO_1_1_1)) {
			bp->p.pp_opcodes_4b = bp->pp_opcodes_4b;
			memcpy(&bp->pp_opcodes_4b[SPI_MEM_IO_1_1_1], &bp->pp_opcodes_3b[SPI_MEM_IO_1_1_1],
				sizeof(bp->read_opcodes_3b[SPI_MEM_IO_1_1_1]));
			bp->pp_opcodes_4b[SPI_MEM_IO_1_1_1].opcode = SNOR_CMD_4B_PAGE_PROG;
		}

		if (read_io_caps)
			bp->p.read_opcodes_4b = bp->read_opcodes_4b;

		/* Note: 1-1-4/1-4-4 for page program is not defined by SFDP */
		if (pp_io_caps)
			bp->p.pp_opcodes_4b = bp->pp_opcodes_4b;

		/* 4B erase opcodes */
		val = sfdp_dw(snor->sfdp.a4bit, 2);

		if ((dw & A4BIT_DW1_SUPPORT_ERASE_T1) && ei.info[0].size) {
			memcpy(&ei4b.info[0], &ei.info[0], sizeof(ei.info[0]));
			ei4b.info[0].opcode = (uint8_t)FIELD_GET(A4BIT_DW2_ERASE_TYPE1_OPCODE, val);
		}

		if ((dw & A4BIT_DW1_SUPPORT_ERASE_T2) && ei.info[1].size) {
			memcpy(&ei4b.info[1], &ei.info[1], sizeof(ei.info[1]));
			ei4b.info[1].opcode = (uint8_t)FIELD_GET(A4BIT_DW2_ERASE_TYPE2_OPCODE, val);
		}

		if ((dw & A4BIT_DW1_SUPPORT_ERASE_T3) && ei.info[2].size) {
			memcpy(&ei4b.info[2], &ei.info[2], sizeof(ei.info[2]));
			ei4b.info[2].opcode = (uint8_t)FIELD_GET(A4BIT_DW2_ERASE_TYPE3_OPCODE, val);
		}

		if ((dw & A4BIT_DW1_SUPPORT_ERASE_T4) && ei.info[3].size) {
			memcpy(&ei4b.info[3], &ei.info[3], sizeof(ei.info[3]));
			ei4b.info[3].opcode = (uint8_t)FIELD_GET(A4BIT_DW2_ERASE_TYPE4_OPCODE, val);
		}

		memcpy(&bp->erase_info_4b, &ei4b, sizeof(ei4b));
		if (nei)
			bp->p.erase_info_4b = &bp->erase_info_4b;
	}

	return UFP_OK;
}

static void spi_nor_set_erase_type_time(struct spi_nor_erase_info *ei, uint32_t size, uint32_t time_ms)
{
	uint32_t i;

	if (!ei)
		return;

	for (i = 0; i < SPI_NOR_MAX_ERASE_INFO; i++) {
		if (ei->info[i].size == size) {
			ei->info[i].max_erase_time_ms = time_ms;
			return;
		}
	}
}

static void spi_nor_parse_sfdp_fill_time(struct spi_nor *snor, struct spi_nor_flash_part_blank *bp)
{
	uint32_t dw, val, et, multi, time_ms;

	if (snor->sfdp.bfpt_dw_num >= 11) {
		/* Page program max size */
		dw = sfdp_dw(snor->sfdp.bfpt, 11);

		bp->p.max_pp_time_us = 2 * (FIELD_GET(BFPT_DW11_PAGE_BYTE_PROG_MAX_TIME_MULTIPLIER, dw) + 1) *
			(FIELD_GET(BFPT_DW11_PAGE_PROG_TYP_TIME, dw) + 1) *
			page_program_units_us[FIELD_GET(BFPT_DW11_PAGE_PROG_TYP_TIME_UNIT, dw)];
	}

	/* Erase time */
	if (snor->sfdp.bfpt_dw_num >= 10) {
		if (!bp->p.erase_info_3b && !bp->p.erase_info_4b) {
			if (!bp->p.erase_info_3b) {
				spi_nor_gen_erase_info(&bp->p, &default_erase_opcodes_3b, &bp->erase_info_3b);
				bp->p.erase_info_3b = &bp->erase_info_3b;
			}

			if (bp->p.size > SZ_16M && !bp->p.erase_info_4b) {
				if ((bp->p.a4b_flags & SNOR_4B_F_OPCODE) ||
				    (!bp->p.a4b_flags && bp->p.a4b_en_type == A4B_EN_4B_OPCODE)) {
					spi_nor_gen_erase_info(&bp->p, &default_erase_opcodes_4b, &bp->erase_info_4b);
					bp->p.erase_info_4b = &bp->erase_info_4b;
				}
			}
		}

		et = sfdp_dw(snor->sfdp.bfpt, 10);
		multi = 2 * (FIELD_GET(BFPT_DW10_SECTOR_ERASE_MAX_TIME_MULTIPLIER, et) + 1);

		dw = sfdp_dw(snor->sfdp.bfpt, 8);
		val = FIELD_GET(BFPT_DW8_ERASE_TYPE1_SIZE_SHIFT, dw);
		if (val) {
			time_ms = multi * (FIELD_GET(BFPT_DW10_SECTOR_T1_ERASE_TYP_TIME, et) + 1) *
				sector_erase_time_units_ms[FIELD_GET(BFPT_DW10_SECTOR_T1_ERASE_TYP_TIME_UNIT, et)];
			if (bp->p.erase_info_3b)
				spi_nor_set_erase_type_time(&bp->erase_info_3b, 1 << val, time_ms);
			if (bp->p.erase_info_4b)
				spi_nor_set_erase_type_time(&bp->erase_info_4b, 1 << val, time_ms);
		}

		val = FIELD_GET(BFPT_DW8_ERASE_TYPE2_SIZE_SHIFT, dw);
		if (val) {
			time_ms = multi * (FIELD_GET(BFPT_DW10_SECTOR_T2_ERASE_TYP_TIME, et) + 1) *
				sector_erase_time_units_ms[FIELD_GET(BFPT_DW10_SECTOR_T2_ERASE_TYP_TIME_UNIT, et)];
			if (bp->p.erase_info_3b)
				spi_nor_set_erase_type_time(&bp->erase_info_3b, 1 << val, time_ms);
			if (bp->p.erase_info_4b)
				spi_nor_set_erase_type_time(&bp->erase_info_4b, 1 << val, time_ms);
		}

		dw = sfdp_dw(snor->sfdp.bfpt, 9);

		val = FIELD_GET(BFPT_DW9_ERASE_TYPE3_SIZE_SHIFT, dw);
		if (val) {
			time_ms = multi * (FIELD_GET(BFPT_DW10_SECTOR_T3_ERASE_TYP_TIME, et) + 1) *
				sector_erase_time_units_ms[FIELD_GET(BFPT_DW10_SECTOR_T3_ERASE_TYP_TIME_UNIT, et)];
			if (bp->p.erase_info_3b)
				spi_nor_set_erase_type_time(&bp->erase_info_3b, 1 << val, time_ms);
			if (bp->p.erase_info_4b)
				spi_nor_set_erase_type_time(&bp->erase_info_4b, 1 << val, time_ms);
		}

		val = FIELD_GET(BFPT_DW9_ERASE_TYPE4_SIZE_SHIFT, dw);
		if (val) {
			time_ms = multi * (FIELD_GET(BFPT_DW10_SECTOR_T4_ERASE_TYP_TIME, et) + 1) *
				sector_erase_time_units_ms[FIELD_GET(BFPT_DW10_SECTOR_T4_ERASE_TYP_TIME_UNIT, et)];
			if (bp->p.erase_info_3b)
				spi_nor_set_erase_type_time(&bp->erase_info_3b, 1 << val, time_ms);
			if (bp->p.erase_info_4b)
				spi_nor_set_erase_type_time(&bp->erase_info_4b, 1 << val, time_ms);
		}
	}
}

static ufprog_status spi_nor_try_read_sfdp_header(struct spi_nor *snor, struct sfdp_header *hdr)
{
	ufprog_status ret = UFP_UNSUPPORTED;

	if (spi_nor_supports_read_sfdp(snor, snor->state.cmd_buswidth_curr, 0, sizeof(struct sfdp_header)))
		ret = spi_nor_read_sfdp(snor, snor->state.cmd_buswidth_curr, 0, sizeof(struct sfdp_header), hdr);

	if (ret == UFP_UNSUPPORTED)
		return UFP_UNSUPPORTED;

	if (!ret) {
		if (le32toh(hdr->signature) == SFDP_SIGNATURE)
			return UFP_OK;
	}

	return UFP_FAIL;
}

bool spi_nor_probe_sfdp(struct spi_nor *snor, const struct spi_nor_vendor *vendor, struct spi_nor_flash_part_blank *bp)
{
	struct sfdp_header sfdp_hdr;
	ufprog_status ret;

	if (!snor->state.cmd_buswidth_curr)
		snor->state.cmd_buswidth_curr = 1;

	STATUS_CHECK_RET(spi_nor_set_low_speed(snor));

	ret = spi_nor_try_read_sfdp_header(snor, &sfdp_hdr);
	if (ret == UFP_UNSUPPORTED)
		return false;
	else if (!ret)
		goto parse_sfdp;

	if (snor->state.cmd_buswidth_curr > 1) {
		/* We already know the current bus width of I/O. Just set back to SPI mode and retry */
		switch (snor->state.cmd_buswidth_curr) {
		case 2:
			if (bp->p.ops && bp->p.ops->dpi_dis) {
				/* Use part's default dpi_dis() and disable DPI */
				ret = bp->p.ops->dpi_dis(snor);
				if (!ret) {
					snor->state.cmd_buswidth_curr = 1;

					ret = spi_nor_try_read_sfdp_header(snor, &sfdp_hdr);
					if (!ret)
						goto parse_sfdp;
				}
			}

			if (vendor && vendor->default_part_ops && vendor->default_part_ops->dpi_dis) {
				/* Use vendor's default dpi_dis() and disable DPI */
				ret = vendor->default_part_ops->dpi_dis(snor);
				if (!ret) {
					snor->state.cmd_buswidth_curr = 1;

					ret = spi_nor_try_read_sfdp_header(snor, &sfdp_hdr);
					if (!ret)
						goto parse_sfdp;
				}
			}
			break;

		case 4:
			if (bp->p.ops && bp->p.ops->qpi_dis) {
				/* Use part's default qpi_dis() and disable QPI */
				ret = bp->p.ops->qpi_dis(snor);
				if (!ret) {
					snor->state.cmd_buswidth_curr = 1;

					ret = spi_nor_try_read_sfdp_header(snor, &sfdp_hdr);
					if (!ret)
						goto parse_sfdp;
				}
			}

			if (bp->p.qpi_dis_type) {
				/* Select proper qpi_dis() and disable QPI */
				switch (bp->p.qpi_dis_type) {
				case QPI_DIS_FFH:
					ret = spi_nor_disable_qpi_ffh(snor);
					break;

				case QPI_DIS_F5H:
					ret = spi_nor_disable_qpi_f5h(snor);
					break;

				case QPI_DIS_66H_99H:
					ret = spi_nor_disable_qpi_66h_99h(snor);
					break;

				case QPI_DIS_NONE:
				default:
					ret = UFP_FAIL;
				}

				if (!ret) {
					snor->state.cmd_buswidth_curr = 1;

					ret = spi_nor_try_read_sfdp_header(snor, &sfdp_hdr);
					if (!ret)
						goto parse_sfdp;
				}
			}

			if (vendor && vendor->default_part_ops && vendor->default_part_ops->qpi_dis) {
				/* Use vendor's default qpi_dis() and disable QPI */
				ret = vendor->default_part_ops->qpi_dis(snor);
				if (!ret) {
					snor->state.cmd_buswidth_curr = 1;

					ret = spi_nor_try_read_sfdp_header(snor, &sfdp_hdr);
					if (!ret)
						goto parse_sfdp;
				}
			}
		}
	} else if (!bp->p.size) {
		/* We don't know the correct bus width of I/O. Try DPI and QPI where possible */

		/* QPI cmd */
		snor->state.cmd_buswidth_curr = 4;

		ret = spi_nor_try_read_sfdp_header(snor, &sfdp_hdr);
		if (!ret)
			goto parse_sfdp;

		/* DPI cmd */
		snor->state.cmd_buswidth_curr = 2;

		ret = spi_nor_try_read_sfdp_header(snor, &sfdp_hdr);
		if (!ret)
			goto parse_sfdp;
	}

	/* Unable to set to SPI mode. Fail directly. */
	logm_dbg("Unable to read SFDP. SFDP may not be available\n");
	return false;

parse_sfdp:
	STATUS_CHECK_RET(spi_nor_read_full_sfdp(snor, sfdp_hdr.nph + 1));

	logm_dbg("SFDP %u.%u found\n", sfdp_hdr.major_ver, sfdp_hdr.minor_ver);

	spi_nor_parse_sfdp_init(snor);

	if (bp->p.flags & SNOR_F_NO_SFDP) {
		logm_dbg("SFDP will not be used for I/O setup\n");
		spi_nor_parse_sfdp_fill_time(snor, bp);
		return true;
	}

	ret = spi_nor_parse_sfdp_fill(snor, bp);
	if (ret)
		return false;

	return true;
}

static uint8_t spi_nor_smpt_get_naddr(struct spi_nor *snor, uint32_t type)
{
	switch (type) {
	case CMD_DW1_NO_ADDRESS:
		return 0;
	case CMD_DW1_3B_ADDRESS:
		return 3;
	case CMD_DW1_4B_ADDRESS:
		return 4;
	case CMD_DW1_VARIABLE_ADDRESS:
	default:
		return snor->state.naddr;
	}
}

static uint8_t spi_nor_smpt_get_read_ndummy(struct spi_nor *snor, uint32_t ndummy)
{
	if (ndummy == FIELD_MAX(SMPT_CMD_DW1_DUMMY_CLKS))
		return snor->state.read_ndummy;

	return (uint8_t)ndummy;
}

static void spi_nor_smpt_adjust_erasesizes_mask(struct spi_nor *snor, uint32_t *mask)
{
	uint32_t i;

	for (i = 0; i < SPI_NOR_MAX_ERASE_INFO; i++) {
		if (!snor->param.erase_info.info[i].size)
			*mask &= !BIT(i);
	}
}

bool spi_nor_parse_sfdp_smpt(struct spi_nor *snor)
{
	uint8_t val, mask, idx = 0, cid = 0;
	uint64_t total_size = 0;
	bool found = false;
	uint32_t i, dw;

	struct ufprog_spi_mem_op op = SPI_MEM_OP(
		SPI_MEM_OP_CMD(0, 1),
		SPI_MEM_OP_ADDR(0, 0, 1),
		SPI_MEM_OP_DUMMY(0, 1),
		SPI_MEM_OP_DATA_IN(1, &val, 1)
	);

	if (snor->param.flags & SNOR_F_NO_SFDP)
		return true;

	if (!snor->sfdp.smpt)
		return true;

	for (i = 1; i <= snor->sfdp.smpt_dw_num; i += 2) {
		dw = sfdp_dw(snor->sfdp.smpt, i);
		if (dw & SMPT_DW1_DESCRIPTOR_TYPE)
			break;

		mask = FIELD_GET(SMPT_CMD_DW1_READ_DATA_MASK, dw);
		op.cmd.opcode = FIELD_GET(SMPT_CMD_DW1_DETECTION_OPCODE, dw);
		op.dummy.len = spi_nor_smpt_get_read_ndummy(snor, FIELD_GET(SMPT_CMD_DW1_DUMMY_CLKS, dw));
		op.addr.len = spi_nor_smpt_get_naddr(snor, FIELD_GET(SMPT_CMD_DW1_ADDRESS_LENGTH, dw));
		op.addr.val = sfdp_dw(snor->sfdp.smpt, i + 1);

		if (!ufprog_spi_mem_supports_op(snor->spi, &op)) {
			logm_err("Controller does not support detecting sector map configuration\n");
			return UFP_UNSUPPORTED;
		}

		if (ufprog_spi_mem_exec_op(snor->spi, &op))
			return false;

		cid = (cid << 1) | (!!(val & mask));
	}

	logm_dbg("Current in-use Sector Map Configuration ID: %u\n", cid);

	while (i <= snor->sfdp.smpt_dw_num) {
		dw = sfdp_dw(snor->sfdp.smpt, i);
		if (FIELD_GET(SMPT_MAP_DW1_CONFIGURATION_ID, dw) == cid) {
			found = true;
			break;
		}

		if (dw & SMPT_DW1_SEQ_END_INDICATOR)
			break;

		/* increment the table index to the next map */
		i += FIELD_GET(SMPT_MAP_DW1_REGION_COUNT, dw) + 2;
	}

	if (!found) {
		logm_err("Sector Map with Configuration ID %u not found\n", cid);
		return false;
	}

	dw = sfdp_dw(snor->sfdp.smpt, i);

	snor->ext_param.num_erase_regions = FIELD_GET(SMPT_MAP_DW1_REGION_COUNT, dw) + 1;
	if (snor->sfdp.smpt_dw_num - i + 1 < snor->ext_param.num_erase_regions) {
		logm_err("Incomplete SFDP Sector Map Parameter Table data\n");
		snor->ext_param.num_erase_regions = 0;
		return false;
	}

	snor->ext_param.erase_regions = calloc(snor->ext_param.num_erase_regions,
					       sizeof(*snor->ext_param.erase_regions));
	if (!snor->ext_param.erase_regions) {
		logm_err("No memory for erase region data\n");
		snor->ext_param.num_erase_regions = 0;
		return false;
	}

	i++;

	while (idx < snor->ext_param.num_erase_regions) {
		dw = sfdp_dw(snor->sfdp.smpt, i);

		snor->ext_param.erase_regions[idx].size = (FIELD_GET(SMPT_MAP_DW2_REGION_SIZE, dw) + 1) * 256;
		snor->ext_param.erase_regions[idx].erasesizes_mask = dw & SMPT_MAP_DW2_ERASE_TYPE_MASK;
		spi_nor_smpt_adjust_erasesizes_mask(snor, &snor->ext_param.erase_regions[idx].erasesizes_mask);
		spi_nor_fill_erase_region_erasesizes(snor, &snor->ext_param.erase_regions[idx]);

		total_size += snor->ext_param.erase_regions[idx].size;

		idx++;
		i++;
	}

	if (total_size != snor->param.size) {
		logm_err("Sector Map defined in SFDP does not cover the entire flash\n");
		free(snor->ext_param.erase_regions);
		snor->ext_param.erase_regions = NULL;
		snor->ext_param.num_erase_regions = 0;
		return false;
	}

	return true;
}

bool spi_nor_locate_sfdp_vendor(struct spi_nor *snor, uint8_t mfr_id, bool match_jedec_msb)
{
	struct sfdp_param_header *phdr;
	struct sfdp_header *hdr;
	uint32_t i, off;

	if (!snor->sfdp.data)
		return false;

	hdr = (struct sfdp_header *)snor->sfdp.data;
	phdr = (struct sfdp_param_header *)((uintptr_t)snor->sfdp.data + sizeof(struct sfdp_header));

	for (i = 0; i <= hdr->nph; i++) {
		off = ((uint32_t)phdr[i].ptr[0]) | (((uint32_t)phdr[i].ptr[1]) << 8) |
			(((uint32_t)phdr[i].ptr[2]) << 16);

		if (match_jedec_msb && phdr[i].id_msb != SFDP_PARAM_ID_MSB_JEDEC)
			continue;

		if (phdr[i].id_lsb == mfr_id) {
			snor->sfdp.vendor_hdr = &phdr[i];
			snor->sfdp.vendor = (uint32_t *)((uintptr_t)snor->sfdp.data + off);
			snor->sfdp.vendor_dw_num = phdr[i].len;

			logm_dbg("SFDP Vendor Parameter Table %u.%u, %u DWORDs\n",
				 phdr[i].major_ver, phdr[i].minor_ver, phdr[i].len);

			return true;
		}
	}

	return false;
}
