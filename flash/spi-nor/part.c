// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash part related
 */

#include <string.h>
#include <ufprog/log.h>
#include <ufprog/sizes.h>
#include <ufprog/spi-nor-opcode.h>
#include "part.h"

const struct spi_nor_io_opcode default_read_opcodes_3b[__SPI_MEM_IO_MAX] = {
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_1, SNOR_CMD_FAST_READ, 8, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_2, SNOR_CMD_FAST_READ_DUAL_OUT, 8, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_1_2_2, SNOR_CMD_FAST_READ_DUAL_IO, 4, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_2_2_2, SNOR_CMD_FAST_READ_DUAL_IO, 8, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_4, SNOR_CMD_FAST_READ_QUAD_OUT, 8, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_1_4_4, SNOR_CMD_FAST_READ_QUAD_IO, 6, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_4_4_4, SNOR_CMD_FAST_READ_QUAD_IO, 8, 0),
};

const struct spi_nor_io_opcode default_read_opcodes_4b[__SPI_MEM_IO_MAX] = {
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_1, SNOR_CMD_4B_FAST_READ, 8, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_2, SNOR_CMD_4B_FAST_READ_DUAL_OUT, 8, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_1_2_2, SNOR_CMD_4B_FAST_READ_DUAL_IO, 4, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_2_2_2, SNOR_CMD_4B_FAST_READ_DUAL_IO, 8, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_4, SNOR_CMD_4B_FAST_READ_QUAD_OUT, 8, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_1_4_4, SNOR_CMD_4B_FAST_READ_QUAD_IO, 6, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_4_4_4, SNOR_CMD_4B_FAST_READ_QUAD_IO, 8, 0),
};

const struct spi_nor_io_opcode default_pp_opcodes_3b[__SPI_MEM_IO_MAX] = {
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_1, SNOR_CMD_PAGE_PROG, 0, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_2, SNOR_CMD_PAGE_PROG_DUAL_IN, 0, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_4, SNOR_CMD_PAGE_PROG_QUAD_IN, 0, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_4_4_4, SNOR_CMD_PAGE_PROG, 0, 0),
};

const struct spi_nor_io_opcode default_pp_opcodes_4b[__SPI_MEM_IO_MAX] = {
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_1, SNOR_CMD_4B_PAGE_PROG, 0, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_1_1_4, SNOR_CMD_4B_PAGE_PROG_QUAD_IN, 0, 0),
	SNOR_IO_OPCODE(SPI_MEM_IO_4_4_4, SNOR_CMD_4B_PAGE_PROG, 0, 0),
};

const struct spi_nor_erase_info default_erase_opcodes_3b = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_4K, SNOR_CMD_SECTOR_ERASE),
	SNOR_ERASE_SECTOR(SZ_32K, SNOR_CMD_SECTOR_ERASE_32K),
	SNOR_ERASE_SECTOR(SZ_64K, SNOR_CMD_BLOCK_ERASE)
);

const struct spi_nor_erase_info default_erase_opcodes_4b = SNOR_ERASE_SECTORS(
	SNOR_ERASE_SECTOR(SZ_4K, SNOR_CMD_4B_SECTOR_ERASE),
	SNOR_ERASE_SECTOR(SZ_64K, SNOR_CMD_4B_BLOCK_ERASE)
);

const struct spi_nor_reg_field_values reg_field_values_yes_no = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "No"),
	VALUE_ITEM(1, "Yes"),
);

const struct spi_nor_reg_field_values reg_field_values_yes_no_rev = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "Yes"),
	VALUE_ITEM(1, "No"),
);

const struct spi_nor_reg_field_values reg_field_values_true_false = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "False"),
	VALUE_ITEM(1, "True"),
);

const struct spi_nor_reg_field_values reg_field_values_true_false_rev = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "True"),
	VALUE_ITEM(1, "False"),
);

const struct spi_nor_reg_field_values reg_field_values_on_off = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "Off"),
	VALUE_ITEM(1, "On"),
);

const struct spi_nor_reg_field_values reg_field_values_on_off_rev = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "On"),
	VALUE_ITEM(1, "Off"),
);

const struct spi_nor_reg_field_values reg_field_values_enabled_disabled = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "Disabled"),
	VALUE_ITEM(1, "Enabled"),
);

const struct spi_nor_reg_field_values reg_field_values_enabled_disabled_rev = SNOR_REG_FIELD_VALUES(
	VALUE_ITEM(0, "Enabled"),
	VALUE_ITEM(1, "Disabled"),
);

void spi_nor_prepare_blank_part(struct spi_nor_flash_part_blank *bp, const struct spi_nor_flash_part *refpart)
{
	size_t len;

	memset(bp, 0, sizeof(*bp));

	if (!refpart)
		return;

	memcpy(&bp->p, refpart, sizeof(bp->p));

	if (refpart->model) {
		len = strlen(refpart->model);
		if (len >= sizeof(bp->model))
			len = sizeof(bp->model) - 1;

		memcpy(bp->model, refpart->model, len);
		bp->model[len] = 0;

		bp->p.model = bp->model;
	}

	if (refpart->erase_info_3b) {
		memcpy(&bp->erase_info_3b, refpart->erase_info_3b, sizeof(bp->erase_info_3b));
		bp->p.erase_info_3b = &bp->erase_info_3b;
	}

	if (refpart->erase_info_4b) {
		memcpy(&bp->erase_info_4b, refpart->erase_info_4b, sizeof(bp->erase_info_4b));
		bp->p.erase_info_4b = &bp->erase_info_4b;
	}

	if (refpart->read_opcodes_3b) {
		memcpy(bp->read_opcodes_3b, refpart->read_opcodes_3b, sizeof(bp->read_opcodes_3b));
		bp->p.read_opcodes_3b = bp->read_opcodes_3b;
	}

	if (refpart->read_opcodes_4b) {
		memcpy(bp->read_opcodes_4b, refpart->read_opcodes_4b, sizeof(bp->read_opcodes_4b));
		bp->p.read_opcodes_4b = bp->read_opcodes_4b;
	}

	if (refpart->pp_opcodes_3b) {
		memcpy(bp->pp_opcodes_3b, refpart->pp_opcodes_3b, sizeof(bp->pp_opcodes_3b));
		bp->p.pp_opcodes_3b = bp->pp_opcodes_3b;
	}

	if (refpart->pp_opcodes_4b) {
		memcpy(bp->pp_opcodes_4b, refpart->pp_opcodes_4b, sizeof(bp->pp_opcodes_4b));
		bp->p.pp_opcodes_4b = bp->pp_opcodes_4b;
	}
}

void spi_nor_blank_part_fill_default_opcodes(struct spi_nor_flash_part_blank *bp)
{
	if (!bp->p.erase_info_3b) {
		memcpy(&bp->erase_info_3b, &default_erase_opcodes_3b, sizeof(bp->erase_info_3b));
		bp->p.erase_info_3b = &bp->erase_info_3b;
	}

	if (!bp->p.erase_info_4b) {
		memcpy(&bp->erase_info_4b, &default_erase_opcodes_4b, sizeof(bp->erase_info_4b));
		bp->p.erase_info_4b = &bp->erase_info_4b;
	}

	if (!bp->p.read_opcodes_3b) {
		memcpy(bp->read_opcodes_3b, default_read_opcodes_3b, sizeof(bp->read_opcodes_3b));
		bp->p.read_opcodes_3b = bp->read_opcodes_3b;
	}

	if (!bp->p.pp_opcodes_3b) {
		memcpy(bp->pp_opcodes_3b, default_pp_opcodes_3b, sizeof(bp->pp_opcodes_3b));
		bp->p.pp_opcodes_3b = bp->pp_opcodes_3b;
	}

	if (bp->p.size > SZ_16M && ((bp->p.a4b_flags & SNOR_4B_F_OPCODE) || (bp->p.a4b_en_type == A4B_EN_4B_OPCODE))) {
		if (!bp->p.read_opcodes_4b) {
			memcpy(bp->read_opcodes_4b, default_read_opcodes_4b, sizeof(bp->read_opcodes_4b));
			bp->p.read_opcodes_4b = bp->read_opcodes_4b;
		}

		if (!bp->p.pp_opcodes_4b) {
			memcpy(bp->pp_opcodes_4b, default_pp_opcodes_4b, sizeof(bp->pp_opcodes_4b));
			bp->p.pp_opcodes_4b = bp->pp_opcodes_4b;
		}
	}
}

bool spi_nor_id_match(const uint8_t *id1, const uint8_t *id2, const uint8_t *mask, uint32_t len)
{
	uint32_t i;

	if (!mask)
		return !memcmp(id1, id2, len);

	for (i = 0; i < len; i++) {
		if ((id1[i] & mask[i]) != (id2[i] & mask[i]))
			return false;
	}

	return true;
}

const struct spi_nor_flash_part *spi_nor_find_part(const struct spi_nor_flash_part *parts, size_t count,
						   const uint8_t *id)
{
	size_t i;

	if (!parts || !count)
		return NULL;

	for (i = 0; i < count; i++) {
		if (parts[i].id.len && spi_nor_id_match(parts[i].id.id, id, parts[i].id_mask, parts[i].id.len))
			return &parts[i];
	}

	return NULL;
}

const struct spi_nor_flash_part *spi_nor_find_part_by_name(const struct spi_nor_flash_part *parts, size_t count,
							   const char *model)
{
	size_t i, j;

	if (!parts || !count)
		return NULL;

	for (i = 0; i < count; i++) {
		if (!strcasecmp(parts[i].model, model))
			return &parts[i];

		if (!parts[i].alias || !parts[i].alias->num)
			continue;

		for (j = 0; j < parts[i].alias->num; j++) {
			if (!strcasecmp(parts[i].alias->items[j].model, model))
				return &parts[i];
		}
	}

	return NULL;
}

