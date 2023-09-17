// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND flash part related
 */

#include <string.h>
#include <ufprog/log.h>
#include <ufprog/sizes.h>
#include <ufprog/spi-nand-opcode.h>
#include "part.h"

const struct spi_nand_io_opcode default_rd_opcodes_4d[__SPI_MEM_IO_MAX] = {
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_1, SNAND_CMD_FAST_READ_FROM_CACHE, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_2, SNAND_CMD_READ_FROM_CACHE_DUAL_OUT, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_2_2, SNAND_CMD_READ_FROM_CACHE_DUAL_IO, 2, 4),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_4, SNAND_CMD_READ_FROM_CACHE_QUAD_OUT, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_4_4, SNAND_CMD_READ_FROM_CACHE_QUAD_IO, 2, 4),
};

const struct spi_nand_io_opcode default_rd_opcodes_q2d[__SPI_MEM_IO_MAX] = {
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_1, SNAND_CMD_FAST_READ_FROM_CACHE, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_2, SNAND_CMD_READ_FROM_CACHE_DUAL_OUT, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_2_2, SNAND_CMD_READ_FROM_CACHE_DUAL_IO, 2, 4),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_4, SNAND_CMD_READ_FROM_CACHE_QUAD_OUT, 2, 8),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_4_4, SNAND_CMD_READ_FROM_CACHE_QUAD_IO, 2, 2),
};

const struct spi_nand_io_opcode default_pl_opcodes[__SPI_MEM_IO_MAX] = {
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_1, SNAND_CMD_PROGRAM_LOAD, 2, 0),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_4, SNAND_CMD_PROGRAM_LOAD_QUAD_IN, 2, 0),
};

const struct spi_nand_io_opcode default_upd_opcodes[__SPI_MEM_IO_MAX] = {
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_1, SNAND_CMD_RND_PROGRAM_LOAD, 2, 0),
	SNAND_IO_OPCODE(SPI_MEM_IO_1_1_4, SNAND_CMD_RND_PROGRAM_LOAD_QUAD_IN, 2, 0),
};

const struct nand_memorg snand_memorg_512m_2k_64 = SNAND_MEMORG(2048, 64, 64, 512, 1, 1);
const struct nand_memorg snand_memorg_512m_2k_128 = SNAND_MEMORG(2048, 128, 64, 512, 1, 1);
const struct nand_memorg snand_memorg_1g_2k_64 = SNAND_MEMORG(2048, 64, 64, 1024, 1, 1);
const struct nand_memorg snand_memorg_2g_2k_64 = SNAND_MEMORG(2048, 64, 64, 2048, 1, 1);
const struct nand_memorg snand_memorg_2g_2k_120 = SNAND_MEMORG(2048, 120, 64, 2048, 1, 1);
const struct nand_memorg snand_memorg_4g_2k_64 = SNAND_MEMORG(2048, 64, 64, 4096, 1, 1);
const struct nand_memorg snand_memorg_1g_2k_120 = SNAND_MEMORG(2048, 120, 64, 1024, 1, 1);
const struct nand_memorg snand_memorg_1g_2k_128 = SNAND_MEMORG(2048, 128, 64, 1024, 1, 1);
const struct nand_memorg snand_memorg_2g_2k_128 = SNAND_MEMORG(2048, 128, 64, 2048, 1, 1);
const struct nand_memorg snand_memorg_4g_2k_128 = SNAND_MEMORG(2048, 128, 64, 4096, 1, 1);
const struct nand_memorg snand_memorg_4g_4k_240 = SNAND_MEMORG(4096, 240, 64, 2048, 1, 1);
const struct nand_memorg snand_memorg_4g_4k_256 = SNAND_MEMORG(4096, 256, 64, 2048, 1, 1);
const struct nand_memorg snand_memorg_8g_2k_128 = SNAND_MEMORG(2048, 128, 64, 8192, 1, 1);
const struct nand_memorg snand_memorg_8g_4k_256 = SNAND_MEMORG(4096, 256, 64, 4096, 1, 1);
const struct nand_memorg snand_memorg_1g_2k_64_2p = SNAND_MEMORG(2048, 64, 64, 1024, 1, 2);
const struct nand_memorg snand_memorg_2g_2k_64_2p = SNAND_MEMORG(2048, 64, 64, 2048, 1, 2);
const struct nand_memorg snand_memorg_2g_2k_64_2d = SNAND_MEMORG(2048, 64, 64, 1024, 2, 1);
const struct nand_memorg snand_memorg_2g_2k_128_2p = SNAND_MEMORG(2048, 128, 64, 2048, 1, 2);
const struct nand_memorg snand_memorg_4g_2k_64_2p = SNAND_MEMORG(2048, 64, 64, 4096, 1, 2);
const struct nand_memorg snand_memorg_4g_2k_128_2p_2d = SNAND_MEMORG(2048, 128, 64, 2048, 2, 2);
const struct nand_memorg snand_memorg_8g_4k_256_2d = SNAND_MEMORG(4096, 256, 64, 2048, 2, 1);
const struct nand_memorg snand_memorg_8g_2k_128_2p_4d = SNAND_MEMORG(2048, 128, 64, 2048, 4, 2);

void spi_nand_prepare_blank_part(struct spi_nand_flash_part_blank *bp, const struct spi_nand_flash_part *refpart)
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

	if (refpart->memorg) {
		memcpy(&bp->memorg, refpart->memorg, sizeof(bp->memorg));
		bp->p.memorg = &bp->memorg;
	}

	if (refpart->rd_opcodes) {
		memcpy(&bp->rd_opcodes, refpart->rd_opcodes, sizeof(bp->rd_opcodes));
		bp->p.rd_opcodes = bp->rd_opcodes;
	}

	if (refpart->pl_opcodes) {
		memcpy(&bp->pl_opcodes, refpart->pl_opcodes, sizeof(bp->pl_opcodes));
		bp->p.pl_opcodes = bp->pl_opcodes;
	}

	if (refpart->upd_opcodes) {
		memcpy(&bp->upd_opcodes, refpart->upd_opcodes, sizeof(bp->upd_opcodes));
		bp->p.upd_opcodes = bp->upd_opcodes;
	}
}

void spi_nand_blank_part_fill_default_opcodes(struct spi_nand_flash_part_blank *bp)
{
	if (!bp->p.rd_opcodes) {
		memcpy(&bp->rd_opcodes, &default_rd_opcodes_4d, sizeof(bp->rd_opcodes));
		bp->p.rd_opcodes = bp->rd_opcodes;

		if (!bp->p.rd_io_caps)
			bp->p.rd_io_caps = BIT_SPI_MEM_IO_1_1_1;
	}

	if (!bp->p.pl_opcodes) {
		memcpy(&bp->pl_opcodes, &default_pl_opcodes, sizeof(bp->pl_opcodes));
		bp->p.pl_opcodes = bp->pl_opcodes;

		if (!bp->p.pl_io_caps)
			bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1;
	}

	if (!bp->p.upd_opcodes) {
		memcpy(&bp->upd_opcodes, &default_upd_opcodes, sizeof(bp->upd_opcodes));
		bp->p.upd_opcodes = bp->upd_opcodes;

		if (!bp->p.pl_io_caps)
			bp->p.pl_io_caps = BIT_SPI_MEM_IO_1_1_1;
	}
}

const struct spi_nand_flash_part *spi_nand_find_part(const struct spi_nand_flash_part *parts, size_t count,
						     enum spi_nand_id_type type, const uint8_t *id)
{
	size_t i;

	if (!parts || !count)
		return NULL;

	for (i = 0; i < count; i++) {
		if (parts[i].id.type == type && parts[i].id.val.len &&
		    !memcmp(parts[i].id.val.id, id, parts[i].id.val.len))
			return &parts[i];
	}

	return NULL;
}

const struct spi_nand_flash_part *spi_nand_find_part_by_name(const struct spi_nand_flash_part *parts, size_t count,
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
