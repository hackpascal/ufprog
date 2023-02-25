// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * CRC32 checksum helpers
 */

#include <stdbool.h>
#include <ufprog/crc32.h>

static uint32_t crc32_reflected_table[CRC32_TABLE_NUM_ENTRIES];
static uint32_t crc32_normal_table[CRC32_TABLE_NUM_ENTRIES];

/* Reflected */
uint32_t crc32_reflected_cal(uint32_t crc, const void *data, size_t length, const uint32_t *crc32_table)
{
	const uint8_t *buf = (const uint8_t *)data;

	while (length--)
		crc = crc32_table[(uint8_t)(crc ^ *buf++)] ^ (crc >> 8);

	return crc;
}

void crc32_reflected_init(uint32_t *crc32_table, uint32_t poly)
{
	int i, j;
	uint32_t v;

	for (i = 0; i < CRC32_TABLE_NUM_ENTRIES; i++) {
		v = i;
		for (j = 0; j < 8; j++)
			v = (v >> 1) ^ ((v & 1) ? poly : 0);

		crc32_table[i] = v;
	}
}

/* Normal */
uint32_t crc32_normal_cal(uint32_t crc, const void *data, size_t length, const uint32_t *crc32_table)
{
	const uint8_t *buf = (const uint8_t *)data;

	while (length--)
		crc = crc32_table[(uint8_t)((crc >> 24) ^ *buf++)] ^ (crc << 8);

	return crc;
}

void crc32_normal_init(uint32_t *crc32_table, uint32_t poly)
{
	int i, j;
	uint32_t v;

	for (i = 0; i < CRC32_TABLE_NUM_ENTRIES; i++) {
		v = i << 24;
		for (j = 0; j < 8; j++)
			v = (v << 1) ^ ((v & (1 << 31)) ? poly : 0);

		crc32_table[i] = v;
	}
}

/* Default polynomial calculation */
uint32_t crc32_no_comp(uint32_t crc, const void *data, size_t length)
{
	return crc32_reflected_cal(crc, data, length, crc32_reflected_table);
}

uint32_t crc32_be_no_comp(uint32_t crc, const void *data, size_t length)
{
	return crc32_normal_cal(crc, data, length, crc32_normal_table);
}

void make_crc_table(void)
{
	static bool init = false;

	if (init)
		return;

	crc32_reflected_init(crc32_reflected_table, CRC32_REFLECTED_POLYNOMIAL);
	crc32_normal_init(crc32_normal_table, CRC32_NORMAL_POLYNOMIAL);

	init = true;
}
