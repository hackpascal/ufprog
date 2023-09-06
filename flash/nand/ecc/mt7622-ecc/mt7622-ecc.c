/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ECC driver for MediaTek MT7622/MT7629
 */

#include <malloc.h>
#include <stdbool.h>
#include <string.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include <ufprog/ecc.h>

#define MT7622_ECC_DRV_API_VER_MAJOR		1
#define MT7622_ECC_DRV_API_VER_MINOR		0

#define MT7622_ECC_SECTOR_SIZE			512
#define MT7622_ECC_MAX_SECTORS			8
#define MT7622_ECC_FDM_SIZE			8
#define MT7622_ECC_FDM_ECC_SIZE			1

#define MT7622_ECC_PAGE_LAYOUT_RAW_MAX_ENTRIES	(5 * MT7622_ECC_MAX_SECTORS + 2)
#define MT7622_ECC_PAGE_LAYOUT_MAX_ENTRIES	(4 * MT7622_ECC_MAX_SECTORS + 2)

struct ufprog_ecc_instance {
	uint32_t page_size;
	uint32_t spare_size;
	uint32_t ecc_steps;
	uint32_t spare_per_sector;
	uint32_t raw_sector_size;
	uint32_t ecc_parity_bits;
	uint32_t ecc_strength;
	uint32_t ecc_bytes;
	bool bbm_swap;

	struct nand_page_layout *page_layout;
	struct nand_page_layout *page_layout_canonical;

	struct nand_ecc_status *ecc_status;
};

static const uint8_t mt7622_spare_sizes[] = { 16, 26, 27, 28 };
static const uint8_t mt7622_ecc_caps[] = { 4, 6, 8, 10, 12 };

ufprog_status UFPROG_API ufprog_plugin_init(void)
{
	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_plugin_cleanup(void)
{
	return UFP_OK;
}

uint32_t UFPROG_API ufprog_plugin_api_version(void)
{
	return MAKE_VERSION(MT7622_ECC_DRV_API_VER_MAJOR, MT7622_ECC_DRV_API_VER_MINOR);
}

const char *UFPROG_API ufprog_plugin_desc(void)
{
	return "MediaTek MT7622 on-chip ECC";
}

static inline void do_bm_swap(uint8_t *bm1, uint8_t *bm2)
{
	uint8_t tmp = *bm1;
	*bm1 = *bm2;
	*bm2 = tmp;
}

static void mt7622_ecc_bm_swap_raw(struct ufprog_ecc_instance *ecc, uint8_t *buf)
{
	uint32_t fdm_bbm_pos;

	if (!ecc->bbm_swap || ecc->ecc_steps == 1)
		return;

	fdm_bbm_pos = (ecc->ecc_steps - 1) * ecc->raw_sector_size + MT7622_ECC_SECTOR_SIZE;
	do_bm_swap(&buf[fdm_bbm_pos], &buf[ecc->page_size]);
}

static void mt7622_ecc_bm_swap(struct ufprog_ecc_instance *ecc, uint8_t *buf)
{
	uint32_t buf_bbm_pos, fdm_bbm_pos;

	if (!ecc->bbm_swap || ecc->ecc_steps == 1)
		return;

	buf_bbm_pos = ecc->page_size - (ecc->ecc_steps - 1) * ecc->spare_per_sector;
	fdm_bbm_pos = ecc->page_size + (ecc->ecc_steps - 1) * MT7622_ECC_FDM_SIZE;
	do_bm_swap(&buf[fdm_bbm_pos], &buf[buf_bbm_pos]);
}

static void mt7622_ecc_fdm_bm_swap_raw(struct ufprog_ecc_instance *ecc, uint8_t *buf)
{
	uint32_t fdm_bbm_pos1, fdm_bbm_pos2;

	if (!ecc->bbm_swap || ecc->ecc_steps == 1)
		return;

	fdm_bbm_pos1 = MT7622_ECC_SECTOR_SIZE;
	fdm_bbm_pos2 = (ecc->ecc_steps - 1) * ecc->raw_sector_size + MT7622_ECC_SECTOR_SIZE;
	do_bm_swap(&buf[fdm_bbm_pos1], &buf[fdm_bbm_pos2]);
}

static void mt7622_ecc_fdm_bm_swap(struct ufprog_ecc_instance *ecc, uint8_t *buf)
{
	uint32_t fdm_bbm_pos1, fdm_bbm_pos2;

	if (!ecc->bbm_swap || ecc->ecc_steps == 1)
		return;

	fdm_bbm_pos1 = ecc->page_size;
	fdm_bbm_pos2 = ecc->page_size + (ecc->ecc_steps - 1) * MT7622_ECC_FDM_SIZE;
	do_bm_swap(&buf[fdm_bbm_pos1], &buf[fdm_bbm_pos2]);
}

static void mt7622_ecc_page_layout_gen(struct ufprog_ecc_instance *ecc)
{
	struct nand_page_layout *pglyt = ecc->page_layout;
	uint32_t i;

	for (i = 0; i < (ecc->bbm_swap ? ecc->ecc_steps - 1 : ecc->ecc_steps); i++) {
		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_DATA;
		pglyt->entries[pglyt->count].num = MT7622_ECC_SECTOR_SIZE;
		pglyt->count++;

		if (!ecc->bbm_swap && !i)
			pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_MARKER;
		else
			pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_OOB_DATA;

		pglyt->entries[pglyt->count].num = MT7622_ECC_FDM_ECC_SIZE;
		pglyt->count++;

		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_OOB_FREE;
		pglyt->entries[pglyt->count].num = MT7622_ECC_FDM_SIZE - MT7622_ECC_FDM_ECC_SIZE;
		pglyt->count++;

		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_ECC_PARITY;
		pglyt->entries[pglyt->count].num = ecc->ecc_bytes;
		pglyt->count++;

		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_UNUSED;
		pglyt->entries[pglyt->count].num = ecc->spare_per_sector - MT7622_ECC_FDM_SIZE - ecc->ecc_bytes;
		pglyt->count++;
	}

	if (ecc->bbm_swap) {
		uint32_t part_len = (ecc->ecc_steps - 1) * ecc->raw_sector_size + MT7622_ECC_SECTOR_SIZE -
				    ecc->page_size;

		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_DATA;
		pglyt->entries[pglyt->count].num = part_len;
		pglyt->count++;

		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_MARKER;
		pglyt->entries[pglyt->count].num = 1;
		pglyt->count++;

		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_DATA;
		pglyt->entries[pglyt->count].num = MT7622_ECC_SECTOR_SIZE - part_len;
		pglyt->count++;

		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_OOB_FREE;
		pglyt->entries[pglyt->count].num = MT7622_ECC_FDM_SIZE - MT7622_ECC_FDM_ECC_SIZE;
		pglyt->count++;

		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_ECC_PARITY;
		pglyt->entries[pglyt->count].num = ecc->ecc_bytes;
		pglyt->count++;

		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_UNUSED;
		pglyt->entries[pglyt->count].num = ecc->spare_per_sector - MT7622_ECC_FDM_SIZE - ecc->ecc_bytes;
		pglyt->count++;
	}

	if (ecc->spare_size - ecc->ecc_steps * ecc->spare_per_sector) {
		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_UNUSED;
		pglyt->entries[pglyt->count].num = ecc->spare_per_sector - MT7622_ECC_FDM_ECC_SIZE;
		pglyt->count++;
	}
}

static void mt7622_ecc_page_layout_gen_canonical(struct ufprog_ecc_instance *ecc)
{
	struct nand_page_layout *pglyt = ecc->page_layout_canonical;
	uint32_t i;

	pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_DATA;
	pglyt->entries[pglyt->count].num = ecc->page_size;
	pglyt->count++;

	for (i = 0; i < ecc->ecc_steps; i++) {
		if (!i)
			pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_MARKER;
		else
			pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_OOB_DATA;

		pglyt->entries[pglyt->count].num = MT7622_ECC_FDM_ECC_SIZE;
		pglyt->count++;

		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_OOB_FREE;
		pglyt->entries[pglyt->count].num = MT7622_ECC_FDM_SIZE - MT7622_ECC_FDM_ECC_SIZE;
		pglyt->count++;
	}

	for (i = 0; i < ecc->ecc_steps; i++) {
		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_ECC_PARITY;
		pglyt->entries[pglyt->count].num = ecc->ecc_bytes;
		pglyt->count++;

		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_UNUSED;
		pglyt->entries[pglyt->count].num = ecc->spare_per_sector - MT7622_ECC_FDM_SIZE - ecc->ecc_bytes;
		pglyt->count++;
	}

	if (ecc->spare_size - ecc->ecc_steps * ecc->spare_per_sector) {
		pglyt->entries[pglyt->count].type = NAND_PAGE_BYTE_UNUSED;
		pglyt->entries[pglyt->count].num = ecc->spare_per_sector - MT7622_ECC_FDM_ECC_SIZE;
		pglyt->count++;
	}
}

ufprog_status UFPROG_API ufprog_ecc_create_instance(uint32_t page_size, uint32_t spare_size, struct json_object *config,
						    struct ufprog_ecc_instance **outinst)
{
	uint32_t i, ecc_steps, spare_per_step, spare_per_sector = 0, msg_size, max_ecc_bytes;
	uint32_t ecc_parity_bits, max_ecc_strength, ecc_strength = 0;
	uint32_t ecc_status_len, pglyt_len, pglyt_c_len;
	struct ufprog_ecc_instance *ecc;
	ufprog_bool bbm_swap = false;

	if (!page_size || !spare_size || !outinst)
		return UFP_INVALID_PARAMETER;

	if (config)
		STATUS_CHECK_RET(json_read_bool(config, "bad-block-marker-swap", &bbm_swap));

	ecc_steps = page_size / MT7622_ECC_SECTOR_SIZE;
	if (ecc_steps > MT7622_ECC_MAX_SECTORS) {
		logm_err("Page size is not supported\n");
		return UFP_UNSUPPORTED;
	}

	spare_per_step = spare_size / ecc_steps;

	for (i = ARRAY_SIZE(mt7622_spare_sizes); i > 0; i--) {
		if (mt7622_spare_sizes[i - 1] <= spare_per_step) {
			spare_per_sector = mt7622_spare_sizes[i - 1];
			break;
		}
	}

	if (!spare_per_sector) {
		logm_err("OOB size is not supported\n");
		return UFP_UNSUPPORTED;
	}

	msg_size = MT7622_ECC_SECTOR_SIZE + MT7622_ECC_FDM_ECC_SIZE;
	max_ecc_bytes = spare_per_sector - MT7622_ECC_FDM_SIZE;

	ecc_parity_bits = fls(1 + 8 * msg_size);
	max_ecc_strength = max_ecc_bytes * 8 / ecc_parity_bits;

	for (i = ARRAY_SIZE(mt7622_ecc_caps); i > 0; i--) {
		if (mt7622_ecc_caps[i - 1] <= max_ecc_strength) {
			ecc_strength = mt7622_ecc_caps[i - 1];
			break;
		}
	}

	if (!ecc_strength) {
		logm_err("Page size %u+%u is not supported\n", page_size, spare_size);
		return UFP_UNSUPPORTED;
	}

	ecc_status_len = ecc_steps * sizeof(((struct nand_ecc_status *)0)->step_bitflips[0]);
	pglyt_len = sizeof(*ecc->page_layout) + MT7622_ECC_PAGE_LAYOUT_RAW_MAX_ENTRIES *
		sizeof(ecc->page_layout->entries[0]);
	pglyt_c_len = sizeof(*ecc->page_layout) + MT7622_ECC_PAGE_LAYOUT_MAX_ENTRIES *
		sizeof(ecc->page_layout->entries[0]);

	ecc = calloc(sizeof(*ecc) + ecc_status_len + pglyt_len + pglyt_c_len, 1);
	if (!ecc) {
		logm_err("No memory for MT7622 ECC driver instance\n");
		return UFP_NOMEM;
	}

	ecc->ecc_status = (void *)((uintptr_t)ecc + sizeof(*ecc));
	ecc->page_layout = (void *)((uintptr_t)ecc->ecc_status + ecc_status_len);
	ecc->page_layout_canonical = (void *)((uintptr_t)ecc->page_layout + pglyt_len);

	ecc->ecc_status->per_step = true;

	ecc->page_size = page_size;
	ecc->spare_size = spare_size;
	ecc->ecc_steps = ecc_steps;
	ecc->spare_per_sector = spare_per_sector;
	ecc->raw_sector_size = MT7622_ECC_SECTOR_SIZE + spare_per_sector;
	ecc->ecc_parity_bits = ecc_parity_bits;
	ecc->ecc_strength = ecc_strength;
	ecc->ecc_bytes = (ecc_strength * ecc_parity_bits + 7) / 8;
	ecc->bbm_swap = bbm_swap;

	mt7622_ecc_page_layout_gen(ecc);
	mt7622_ecc_page_layout_gen_canonical(ecc);

	/* XXX: initialize BCH here */

	*outinst = ecc;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_ecc_free_instance(struct ufprog_ecc_instance *inst)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	free(inst);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_ecc_chip_get_config(struct ufprog_ecc_instance *inst,
						    struct nand_ecc_config *ret_ecccfg)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	ret_ecccfg->step_size = MT7622_ECC_SECTOR_SIZE;
	ret_ecccfg->strength_per_step = (uint16_t)inst->ecc_strength;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_ecc_chip_get_bbm_config(struct ufprog_ecc_instance *inst,
							struct nand_bbm_config *ret_bbmcfg)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	memset(ret_bbmcfg, 0, sizeof(*ret_bbmcfg));

	ufprog_ecc_bbm_add_page(&ret_bbmcfg->pages, 0);

	if (inst->bbm_swap) {
		ufprog_ecc_bbm_add_check_pos(&ret_bbmcfg->check, inst->page_size);
		ufprog_ecc_bbm_add_mark_pos(&ret_bbmcfg->mark, inst->page_size);
		ret_bbmcfg->flags = ECC_F_BBM_MERGE_PAGE;
	} else {
		ufprog_ecc_bbm_add_check_pos(&ret_bbmcfg->check, MT7622_ECC_SECTOR_SIZE);
		ufprog_ecc_bbm_add_mark_pos(&ret_bbmcfg->mark, MT7622_ECC_SECTOR_SIZE);
		ufprog_ecc_bbm_add_mark_pos(&ret_bbmcfg->mark, inst->page_size);
		ret_bbmcfg->flags = ECC_F_BBM_MERGE_PAGE | ECC_F_BBM_MARK_WHOLE_PAGE;
	}

	ret_bbmcfg->check.width = 8;
	ret_bbmcfg->mark.bytes = 1;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_ecc_chip_encode_page(struct ufprog_ecc_instance *inst, void *page)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	/* XXX: encode page here */

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_ecc_chip_decode_page(struct ufprog_ecc_instance *inst, void *page)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	/* XXX: decode page here */

	return UFP_OK;
}

const struct nand_ecc_status *UFPROG_API ufprog_ecc_chip_get_status(struct ufprog_ecc_instance *inst)
{
	if (!inst)
		return NULL;

	return inst->ecc_status;
}

const struct nand_page_layout *UFPROG_API ufprog_ecc_chip_get_page_layout(struct ufprog_ecc_instance *inst,
									  ufprog_bool canonical)
{
	if (!inst)
		return NULL;

	if (canonical)
		return inst->page_layout_canonical;

	return inst->page_layout;
}

static inline void *canonical_page_data(const void *page, uint32_t step)
{
	return (uint8_t *)page + step * MT7622_ECC_SECTOR_SIZE;
}

static inline void *canonical_page_fdm(struct ufprog_ecc_instance *ecc, const void *page, uint32_t step)
{
	return (uint8_t *)page + ecc->page_size + step * MT7622_ECC_FDM_SIZE;
}

static inline void *canonical_page_ecc(struct ufprog_ecc_instance *ecc, const void *page, uint32_t step)
{
	return (uint8_t *)page + ecc->page_size + ecc->ecc_steps * MT7622_ECC_FDM_SIZE +
		step * (ecc->spare_per_sector - MT7622_ECC_FDM_SIZE);
}

ufprog_status UFPROG_API ufprog_ecc_chip_convert_page_layout(struct ufprog_ecc_instance *inst, const void *src,
							     void *out, ufprog_bool from_canonical)
{
	const uint8_t *s = src;
	uint8_t *d = out;
	uint32_t i;

	if (!inst)
		return UFP_INVALID_PARAMETER;

	if (from_canonical) {
		for (i = 0; i < inst->ecc_steps; i++) {
			memcpy(d, canonical_page_data(s, i), MT7622_ECC_SECTOR_SIZE);
			memcpy(d + MT7622_ECC_SECTOR_SIZE, canonical_page_fdm(inst, s, i), MT7622_ECC_FDM_SIZE);
			memcpy(d + MT7622_ECC_SECTOR_SIZE + MT7622_ECC_FDM_SIZE, canonical_page_ecc(inst, s, i),
			       inst->spare_per_sector - MT7622_ECC_FDM_SIZE);
			d += inst->raw_sector_size;
		}

		memcpy(d, s + inst->ecc_steps * inst->raw_sector_size,
		       inst->spare_size - inst->ecc_steps * inst->spare_per_sector);

		if (inst->bbm_swap) {
			mt7622_ecc_fdm_bm_swap_raw(inst, out);
			mt7622_ecc_bm_swap_raw(inst, out);
		}

		return UFP_OK;
	}

	for (i = 0; i < inst->ecc_steps; i++) {
		memcpy(canonical_page_data(d, i), s, MT7622_ECC_SECTOR_SIZE);
		memcpy(canonical_page_fdm(inst, d, i), s + MT7622_ECC_SECTOR_SIZE, MT7622_ECC_FDM_SIZE);
		memcpy(canonical_page_ecc(inst, d, i), s + MT7622_ECC_SECTOR_SIZE + MT7622_ECC_FDM_SIZE,
		       inst->spare_per_sector - MT7622_ECC_FDM_SIZE);
		s += inst->raw_sector_size;
	}

	memcpy(d + inst->ecc_steps * inst->raw_sector_size, s,
	       inst->spare_size - inst->ecc_steps * inst->spare_per_sector);

	if (inst->bbm_swap) {
		mt7622_ecc_bm_swap(inst, out);
		mt7622_ecc_fdm_bm_swap(inst, out);
	}

	return UFP_OK;
}
