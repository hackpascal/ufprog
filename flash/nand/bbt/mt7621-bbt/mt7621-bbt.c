/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ECC driver for MediaTek MT7622/MT7629
 */

#include <malloc.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <ufprog/log.h>
#include <ufprog/config.h>
#include <ufprog/bitmap.h>
#include <ufprog/api_bbt.h>
#include <ufprog/nand.h>

#define MT7621_BBT_DRV_API_VER_MAJOR		1
#define MT7621_BBT_DRV_API_VER_MINOR		0

#define FACT_BBT_BLOCK_NUM			32	/* use the last 32 blocks for factory bbt table */
#define FACT_BBT_SIGNATURE_OOB_OFS		1
#define FACT_BBT_SIGNATURE_LEN			7
#define FACT_BBT_BLOCK_REDUNDENT_NUM		4

enum mt7621_nand_bbt_gen_state {
	MT7621_BBT_ST_GOOD,
	MT7621_BBT_ST_BAD = 3,

	__MT7621_BBT_ST_MAX
};

struct ufprog_bbt_instance {
	struct ufprog_bitmap *bm;
	struct nand_chip *nand;
	uint32_t config;
	uint32_t bbt_block;
	uint8_t *page_cache[2];
	bool changed;

	struct nand_info info;
};

static const uint8_t oob_signature[] = "mtknand";

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
	return MAKE_VERSION(MT7621_BBT_DRV_API_VER_MAJOR, MT7621_BBT_DRV_API_VER_MINOR);
}

const char *UFPROG_API ufprog_plugin_desc(void)
{
	return "MediaTek MT7621 NAND Bad Block Table";
}

static ufprog_status mt7621_nand_bbt_read(struct ufprog_bbt_instance *bbt, uint32_t block)
{
	uint32_t page, size_left, chksz, sig_ofs;
	bool check_oob = true;
	uint8_t *bbt_data;
	ufprog_status ret;

	bbt_data = bitmap_data(bbt->bm);
	size_left = (uint32_t)bitmap_data_size(bbt->bm);
	sig_ofs = bbt->info.memorg.page_size + FACT_BBT_SIGNATURE_OOB_OFS;

	ret = ufprog_nand_checkbad(bbt->nand, NULL, block);
	if (ret)
		return ret;

	page = block << bbt->info.maux.pages_per_block_shift;

	do {
		ret = ufprog_nand_read_page(bbt->nand, page, bbt->page_cache[1], false);
		if (ret)
			return ret;

		ret = ufprog_nand_convert_page_format(bbt->nand, bbt->page_cache[1], bbt->page_cache[0], false);
		if (ret)
			return ret;

		if (check_oob) {
			if (memcmp(bbt->page_cache[0] + sig_ofs, oob_signature, FACT_BBT_SIGNATURE_LEN))
				return UFP_FAIL;
		}

		chksz = bbt->info.memorg.page_size;
		if (chksz > size_left)
			chksz = size_left;

		memcpy(bbt_data, bbt->page_cache[0], chksz);

		check_oob = false;
		bbt_data += chksz;
		size_left -= chksz;
		page++;
	} while (size_left);

	return UFP_OK;
}

static ufprog_status mt7621_nand_bbt_write(struct ufprog_bbt_instance *bbt, uint32_t block)
{
	uint32_t page, size_left, chksz, sig_ofs;
	const uint8_t *bbt_data;
	bool write_oob = true;
	ufprog_status ret;

	bbt_data = bitmap_data(bbt->bm);
	size_left = (uint32_t)bitmap_data_size(bbt->bm);
	sig_ofs = bbt->info.memorg.page_size + FACT_BBT_SIGNATURE_OOB_OFS;

	ret = ufprog_nand_checkbad(bbt->nand, NULL, block);
	if (ret)
		return ret;

	page = block << bbt->info.maux.pages_per_block_shift;

	ret = ufprog_nand_erase_block(bbt->nand, page);
	if (ret)
		return ret;

	do {
		chksz = bbt->info.memorg.page_size;
		if (chksz > size_left)
			chksz = size_left;

		memcpy(bbt->page_cache[1], bbt_data, chksz);
		memset(bbt->page_cache[1] + chksz, 0xff, bbt->info.maux.oob_page_size - chksz);

		if (write_oob)
			memcpy(bbt->page_cache[1] + sig_ofs, oob_signature, FACT_BBT_SIGNATURE_LEN);

		ret = ufprog_nand_convert_page_format(bbt->nand, bbt->page_cache[1], bbt->page_cache[0], true);
		if (ret)
			return ret;

		ret = ufprog_nand_write_page(bbt->nand, page, bbt->page_cache[0], false);
		if (ret)
			return ret;

		/* Verify data */
		memset(bbt->page_cache[0], 0xff, bbt->info.maux.oob_page_size);

		ret = ufprog_nand_read_page(bbt->nand, page, bbt->page_cache[1], false);
		if (ret)
			return ret;

		ret = ufprog_nand_convert_page_format(bbt->nand, bbt->page_cache[1], bbt->page_cache[0], false);
		if (ret)
			return ret;

		if (memcmp(bbt->page_cache[0], bbt_data, chksz))
			return UFP_FAIL;

		if (write_oob && memcmp(bbt->page_cache[0] + sig_ofs, oob_signature, FACT_BBT_SIGNATURE_LEN))
			return UFP_FAIL;

		write_oob = false;
		bbt_data += chksz;
		size_left -= chksz;
		page++;
	} while (size_left);

	return UFP_OK;
}

static ufprog_status mt7621_nand_bbt_load(struct ufprog_bbt_instance *bbt)
{
	uint32_t block;
	ufprog_status ret;

	for (block = bbt->info.maux.block_count - 1; block > bbt->info.maux.block_count - FACT_BBT_BLOCK_NUM; block--) {
		ret = mt7621_nand_bbt_read(bbt, block);
		if (!ret) {
			bbt->bbt_block = block;
			bbt->config &= ~BBT_F_READ_ONLY;
			logm_info("Factory BBT found at block %u [0x%08" PRIx64 "]\n", block,
				  (uint64_t)block << bbt->info.maux.block_shift);
			return UFP_OK;
		}
	}

	bbt->bbt_block = 0;

	return UFP_OK;
}

static ufprog_status mt7621_nand_bbt_save(struct ufprog_bbt_instance *bbt)
{
	ufprog_status ret;
	uint32_t block;

	if (!bbt->bbt_block)
		bbt->bbt_block = bbt->info.maux.block_count - 1;

	for (block = bbt->bbt_block; block > bbt->info.maux.block_count - FACT_BBT_BLOCK_NUM; block--) {
		ret = mt7621_nand_bbt_write(bbt, block);
		if (!ret) {
			logm_info("BBT has been updated at block %u [0x08%" PRIx64 "]\n", block,
				  (uint64_t)block << bbt->info.maux.block_shift);
			bbt->bbt_block = block;
			bbt->changed = false;
			return UFP_OK;
		}
	}

	return UFP_FAIL;
}

static ufprog_status mt7621_nand_bbt_reprobe_block(struct ufprog_bbt_instance *bbt, uint32_t block,
						   enum mt7621_nand_bbt_gen_state *retstate)
{
	struct nand_chip *nand = bbt->nand;
	ufprog_status ret;

	ret = ufprog_nand_checkbad(nand, NULL, block);
	if (!ret)
		*retstate = MT7621_BBT_ST_GOOD;
	else if(ret == UFP_FAIL)
		*retstate = MT7621_BBT_ST_BAD;
	else
		return ret;

	bitmap_set(bbt->bm, block, *retstate);

	return UFP_OK;
}

static ufprog_status mt7621_nand_bbt_rescan(struct ufprog_bbt_instance *bbt, bool *retchanged)
{
	enum mt7621_nand_bbt_gen_state state;
	uint32_t block, ostate, cnt = 0;
	ufprog_status ret;

	logm_info("Scanning for bad blocks\n");

	for (block = 0; block < bbt->info.maux.block_count; block++) {
		bitmap_get(bbt->bm, block, &ostate);

		ret = mt7621_nand_bbt_reprobe_block(bbt, block, &state);
		if (ret)
			return ret;

		if (state == MT7621_BBT_ST_BAD)
			logm_info("Bad block %u at 0x%08" PRIx64 "\n", block,
				  (uint64_t)block << bbt->info.maux.block_shift);

		if (state != (enum mt7621_nand_bbt_gen_state)ostate)
			cnt++;
	}

	if (cnt)
		*retchanged = true;

	return UFP_OK;
}

static ufprog_status mt7621_nand_bbt_reprobe(struct ufprog_bbt_instance *bbt)
{
	ufprog_status ret;

	if (bitmap_data_size(bbt->bm) <= bbt->info.maux.block_size)
		mt7621_nand_bbt_load(bbt);
	else
		bitmap_reset(bbt->bm);

	ret = mt7621_nand_bbt_rescan(bbt, &bbt->changed);
	if (ret)
		return ret;

	if (!(bbt->config & BBT_F_READ_ONLY) && bbt->changed)
		mt7621_nand_bbt_save(bbt);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_bbt_create_instance(struct nand_chip *nand, struct json_object *config,
						    struct ufprog_bbt_instance **outinst)
{
	struct ufprog_bbt_instance *bbt;
	struct nand_info info;
	ufprog_status ret;

	if (!nand || !outinst)
		return UFP_INVALID_PARAMETER;

	ufprog_nand_info(nand, &info);

	bbt = calloc(sizeof(*bbt) + info.maux.oob_page_size * 2, 1);
	if (!bbt) {
		logm_err("No memory for MT7621 BBT instance\n");
		return UFP_NOMEM;
	}

	bbt->page_cache[0] = (uint8_t *)((uintptr_t)bbt + sizeof(*bbt));
	bbt->page_cache[1] = (uint8_t *)((uintptr_t)bbt->page_cache[0] + info.maux.oob_page_size);

	bbt->nand = nand;
	bbt->config = BBT_F_FULL_SCAN | BBT_F_READ_ONLY | BBT_F_PROTECTION;

	memcpy(&bbt->info, &info, sizeof(info));

	ret = bitmap_create(BM_CELL_TYPE_1B, fls(__MT7621_BBT_ST_MAX) - 1, info.maux.block_count, MT7621_BBT_ST_GOOD,
			    &bbt->bm);
	if (ret) {
		logm_err("No memory for BBT bitmap\n");
		free(bbt);
		return ret;
	}

	ret = mt7621_nand_bbt_reprobe(bbt);
	if (ret) {
		bitmap_free(bbt->bm);
		free(bbt);
		return ret;
	}

	*outinst = bbt;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_bbt_free_instance(struct ufprog_bbt_instance *inst)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	bitmap_free(inst->bm);

	free(inst);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_bbt_reprobe(struct ufprog_bbt_instance *inst)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	return mt7621_nand_bbt_reprobe(inst);
}

ufprog_status UFPROG_API ufprog_bbt_commit(struct ufprog_bbt_instance *inst)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	if (!(inst->config & BBT_F_READ_ONLY) && inst->changed)
		return mt7621_nand_bbt_save(inst);

	if (!inst->bbt_block)
		return UFP_FAIL;

	return UFP_UNSUPPORTED;
}

ufprog_status UFPROG_API ufprog_bbt_modify_config(struct ufprog_bbt_instance *inst, uint32_t clr, uint32_t set)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	inst->config &= ~clr;
	inst->config |= set | BBT_F_FULL_SCAN;

	return UFP_OK;
}

uint32_t UFPROG_API ufprog_bbt_get_config(struct ufprog_bbt_instance *inst)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	return inst->config;
}

ufprog_status UFPROG_API ufprog_bbt_get_state(struct ufprog_bbt_instance *inst, uint32_t block,
					      uint32_t /* enum nand_bbt_gen_state */ *state)
{
	ufprog_status ret;
	uint32_t val;

	if (!inst)
		return UFP_INVALID_PARAMETER;

	if (block >= inst->info.maux.block_count)
		return UFP_INVALID_PARAMETER;

	ret = bitmap_get(inst->bm, block, &val);
	if (ret)
		return ret;

	if (val == MT7621_BBT_ST_GOOD)
		*state = BBT_ST_GOOD;
	else
		*state = BBT_ST_BAD;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_bbt_set_state(struct ufprog_bbt_instance *inst, uint32_t block,
					      uint32_t /* enum nand_bbt_gen_state */ state)
{
	uint32_t val, ostate;
	ufprog_status ret;

	if (!inst)
		return UFP_INVALID_PARAMETER;

	if (block >= inst->info.maux.block_count || state >= __MT7621_BBT_ST_MAX)
		return UFP_INVALID_PARAMETER;

	if (state == BBT_ST_GOOD || state == BBT_ST_ERASED)
		val = MT7621_BBT_ST_GOOD;
	else if (state == BBT_ST_BAD)
		val = MT7621_BBT_ST_BAD;
	else
		return UFP_INVALID_PARAMETER;

	ret = bitmap_get(inst->bm, block, &ostate);
	if (ret)
		return ret;

	if (val != ostate)
		inst->changed = true;

	if (state != BBT_ST_GOOD && inst->bbt_block && inst->bbt_block == block) {
		if (state == BBT_ST_BAD)
			logm_warn("BBT block is marked bad, BBT auto writeback disabled\n");
		else if (state == BBT_ST_ERASED)
			logm_warn("BBT block is erased, BBT auto writeback disabled\n");

		inst->bbt_block = 0;
		inst->config |= BBT_F_READ_ONLY;
	}

	return bitmap_set(inst->bm, block, val);
}

ufprog_bool UFPROG_API ufprog_bbt_is_reserved(struct ufprog_bbt_instance *inst, uint32_t block)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	if (!(inst->config & BBT_F_PROTECTION) || !inst->bbt_block)
		return false;

	return block == inst->bbt_block;
}
