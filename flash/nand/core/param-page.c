// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Common parameter page helpers
 */

#include <string.h>
#include <ufprog/bits.h>
#include <ufprog/nand.h>
#include <ufprog/nand-param-page.h>

ufprog_bool UFPROG_API ufprog_pp_check_recover(void *pp, uint16_t crc_base, uint32_t len, uint32_t total_len,
					       uint32_t signature)
{
	const void *srcbufs[PARAM_PAGE_MIN_COUNT];
	uint8_t *data = (uint8_t *)pp, *cp;
	uint16_t crc, ocrc;
	uint32_t i, cnt;

	cnt = total_len / len;

	for (i = 0; i < cnt; i++) {
		cp = data + i * len;

		ocrc = ufprog_pp_read_u16(cp, len - PP_CRC_VAL_LEN);
		crc = ufprog_pp_calc_crc(crc_base, cp, len - PP_CRC_VAL_LEN);

		if (ocrc == crc) {
			if (i)
				memcpy(pp, cp, len);

			return true;
		}
	}

	if (cnt < PARAM_PAGE_MIN_COUNT)
		return false;

	for (i = 0; i < PARAM_PAGE_MIN_COUNT; i++)
		srcbufs[i] = &data[i * PARAM_PAGE_MIN_COUNT];

	bitwise_majority(srcbufs, PARAM_PAGE_MIN_COUNT, pp, len);

	crc = ufprog_pp_calc_crc(crc_base, pp, len - PP_CRC_VAL_LEN);

	if (ufprog_pp_read_u16(pp, len - PP_CRC_VAL_LEN) == crc)
		return ufprog_pp_read_u32(pp, PP_SIGNATURE_OFFS) == signature;

	return false;
}

uint16_t UFPROG_API ufprog_pp_calc_crc(uint16_t crc, const void *pp, uint32_t len)
{
	const uint8_t *data = (const uint8_t *)pp;
	uint32_t i;

	while (len--) {
		crc ^= *data++ << 8;

		for (i = 0; i < 8; i++)
			crc = (crc << 1) ^ ((crc & 0x8000) ? 0x8005 : 0);
	}

	return crc;
}

uint32_t UFPROG_API ufprog_pp_read_str(const void *pp, char *buf, size_t size, uint32_t offs, uint32_t len)
{
	const uint8_t *data = (const uint8_t *)pp + offs, *p = data + len - 1;
	uint32_t i;

	if (!pp)
		return 0;

	while (p >= data) {
		if (*p != ' ')
			break;

		len--;
		p--;
	}

	if (!len) {
		*buf = 0;
		return 0;
	}

	p = data;

	for (i = 0; i < len && size - 1; i++, size--) {
		if (p[i] < 0x20 || p[i] >= 0x7f)
			buf[i] = '?';
		else
			buf[i] = p[i];
	}

	buf[i] = 0;

	return i;
}

ufprog_bool UFPROG_API ufprog_pp_resolve_memorg(const void *pp, struct nand_memorg *memorg)
{
	if (!pp || !memorg)
		return false;

	memset(memorg, 0, sizeof(*memorg));

	memorg->page_size = ufprog_pp_read_u32(pp, PP_PAGE_SIZE_OFFS);
	memorg->oob_size = ufprog_pp_read_u16(pp, PP_SPARE_SIZE_OFFS);
	memorg->pages_per_block = ufprog_pp_read_u32(pp, PP_PAGES_PER_BLOCK_OFFS);
	memorg->blocks_per_lun = ufprog_pp_read_u32(pp, PP_BLOCKS_PER_LUN_OFFS);
	memorg->luns_per_cs = ufprog_pp_read_u8(pp, PP_LUNS_PER_CE_OFFS);
	memorg->num_chips = 1;

	return true;
}
