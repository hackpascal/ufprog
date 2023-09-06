// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Builtin FTL implementation
 */

#include <malloc.h>
#include <string.h>
#include <inttypes.h>
#include <ufprog/log.h>
#include <ufprog/bitmap.h>
#include <ufprog/bbt-ram.h>
#include <ufprog/ftl-basic.h>
#include "internal/nand-internal.h"
#include "internal/bbt-internal.h"
#include "internal/ftl-internal.h"

#define FTL_SKB_RETRIES					3

struct nand_ftl_basic {
	struct ufprog_nand_ftl nftl;
	struct ufprog_nand_bbt *bbt;
	struct ufprog_bitmap *bm;
	uint32_t flags;
	bool free_bbt;
};

static void ftl_basic_unusable_block_notice(struct nand_ftl_basic *bftl, uint32_t block, bool bad)
{
	uint32_t set = 0;
	uint64_t addr;

	bitmap_get(bftl->bm, block, &set);
	if (set)
		return;

	addr = (uint64_t)block << bftl->nftl.nand->maux.block_shift;

	if (bad)
		logm_info("Skipped bad block %u at 0x%" PRIx64 "\n", block, addr);
	else
		logm_info("Skipped reserved block %u at 0x%" PRIx64 "\n", block, addr);

	bitmap_set(bftl->bm, block, 1);
}

static ufprog_status ftl_basic_is_good_block(struct nand_ftl_basic *bftl, uint32_t block)
{
	struct nand_chip *nand = bftl->nftl.nand;
	uint32_t block_state = BBT_ST_UNKNOWN;
	ufprog_status ret;

	if (ufprog_bbt_is_reserved(bftl->bbt, block)) {
		ftl_basic_unusable_block_notice(bftl, block, false);
		return UFP_FAIL;
	}

	ret = ufprog_bbt_get_state(bftl->bbt, block, &block_state);
	if (ret || block_state == BBT_ST_UNKNOWN) {
		logm_err("Failed to get state of block %u at 0x%" PRIx64 "\n", block,
			 (uint64_t)block << nand->maux.block_shift);
		return ret ? ret : UFP_DEVICE_IO_ERROR;
	}

	if (block_state == BBT_ST_BAD) {
		ftl_basic_unusable_block_notice(bftl, block, true);
		return UFP_FAIL;
	}

	return UFP_OK;
}

static ufprog_status ftl_basic_get_block_page(struct nand_ftl_basic *bftl, const struct ufprog_ftl_part *part,
						uint32_t page, uint32_t *retblock, uint32_t *retendblock,
						uint32_t *retpage)
{
	uint32_t part_base_block, part_end_block, curr_block, offset_block, offset_page, logical_block = 0;
	struct nand_chip *nand = bftl->nftl.nand;
	ufprog_status ret;

	if (bftl->flags & FTL_BASIC_F_DONT_CHECK_BAD) {
		if (part) {
			*retblock = part->base_block + (page >> nand->maux.pages_per_block_shift);
			*retendblock = part->base_block + part->block_count;

		} else {
			*retblock = page >> nand->maux.pages_per_block_shift;
			*retendblock = nand->maux.block_count;
		}

		if (retpage)
			*retpage = page & nand->maux.pages_per_block_mask;

		return UFP_OK;
	}

	if (part) {
		part_base_block = part->base_block;
		part_end_block = part->base_block + part->block_count;
		offset_block = page >> nand->maux.pages_per_block_shift;
	} else {
		part_base_block = page >> nand->maux.pages_per_block_shift;
		part_end_block = nand->maux.block_count;
		offset_block = 0;
	}

	offset_page = page & nand->maux.pages_per_block_mask;

	curr_block = part_base_block;

	while (curr_block < part_end_block && logical_block < offset_block) {
		ret = ftl_basic_is_good_block(bftl, curr_block);
		if (ret) {
			if (ret != UFP_FAIL)
				return ret;

			curr_block++;
			continue;
		}

		if (logical_block == offset_block)
			break;

		logical_block++;
		curr_block++;
	}

	if (logical_block != offset_block) {
		logm_err("No enough space for page %u\n", page);
		return UFP_FLASH_ADDRESS_OUT_OF_RANGE;
	}

	*retblock = curr_block;
	*retendblock = part_end_block;

	if (retpage)
		*retpage = offset_page;

	return UFP_OK;
}

static ufprog_status ftl_basic_free(struct ufprog_nand_ftl *ftl)
{
	struct nand_ftl_basic *bftl = container_of(ftl, struct nand_ftl_basic, nftl);
	ufprog_status ret;

	if (bftl->free_bbt) {
		ret = ufprog_bbt_commit(bftl->bbt);
		if (ret)
			logm_warn("Failed to commit BBT\n");

		ufprog_bbt_free(bftl->bbt);
	}

	bitmap_free(bftl->bm);

	free(bftl);

	return UFP_OK;
}

static uint64_t ftl_basic_get_size(struct ufprog_nand_ftl *ftl)
{
	struct nand_ftl_basic *bftl = container_of(ftl, struct nand_ftl_basic, nftl);

	return bftl->nftl.nand->maux.size;
}

static ufprog_status ftl_basic_read_pages(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					    uint32_t page, uint32_t count, void *buf, bool raw, uint32_t flags,
					    uint32_t *retcount, struct ufprog_ftl_callback *cb)
{
	uint32_t curr_block, end_block, offset_page, curr_page, curr_cnt, retcnt, rdcnt = 0;
	struct nand_ftl_basic *bftl = container_of(ftl, struct nand_ftl_basic, nftl);
	struct nand_chip *nand = bftl->nftl.nand;
	ufprog_status ret;
	uint8_t *p = buf;
	void *rdbuf;

	if (retcount)
		*retcount = 0;

	ret = ftl_basic_get_block_page(bftl, part, page, &curr_block, &end_block, &offset_page);
	if (ret) /* Get rid of warning of no initial assignment of ret */
		return ret;

	while (count) {
		if (curr_block >= end_block) {
			logm_err("No enough space for read at block %u\n", curr_block);
			ret = UFP_FLASH_ADDRESS_OUT_OF_RANGE;
			break;
		}

		if (!(bftl->flags & FTL_BASIC_F_DONT_CHECK_BAD)) {
			ret = ftl_basic_is_good_block(bftl, curr_block);
			if (ret) {
				if (ret != UFP_FAIL)
					break;

				curr_block++;
				continue;
			}
		}

		curr_page = (curr_block << nand->maux.pages_per_block_shift) + offset_page;
		curr_cnt = nand->memorg.pages_per_block - offset_page;
		if (curr_cnt > count)
			curr_cnt = count;

		if (cb && cb->buffer)
			rdbuf = cb->buffer;
		else
			rdbuf = p;

		if (cb && cb->pre) {
			ret = cb->pre(cb, curr_cnt);
			if (ret)
				break;
		}

		ret = ufprog_nand_read_pages(nand, curr_page, curr_cnt, rdbuf, raw, flags, &retcnt);

		if (cb && retcnt) {
			ret = cb->post(cb, retcnt);
			if (ret) {
				rdcnt += retcnt;
				break;
			}
		}

		if (ret) {
			logm_warn("Failed to read block %u at 0x%" PRIx64 "\n", curr_block,
				  (uint64_t)(curr_page + retcnt) << nand->maux.page_shift);

			if (ret == UFP_ECC_UNCORRECTABLE) {
				if (!(flags & NAND_READ_F_IGNORE_ECC_ERROR))
					break;
			} else if (!(flags & NAND_READ_F_IGNORE_IO_ERROR)) {
				break;
			}

			ret = UFP_OK;
		}

		offset_page = 0;
		rdcnt += curr_cnt;
		count -= curr_cnt;
		curr_block++;
		p += curr_cnt * nand->maux.oob_page_size;
	}

	if (retcount)
		*retcount = rdcnt;

	return ret;
}

static ufprog_status ftl_basic_read_page(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					   uint32_t page, void *buf, bool raw)
{
	return ftl_basic_read_pages(ftl, part, page, 1, buf, raw, 0, NULL, NULL);
}

static ufprog_status ftl_basic_write_pages(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					     uint32_t page, uint32_t count, const void *buf, bool raw,
					     bool ignore_error, uint32_t *retcount, struct ufprog_ftl_callback *cb)
{
	uint32_t curr_block, end_block, offset_page, curr_page, curr_cnt, retries, retcnt, wrcnt = 0;
	struct nand_ftl_basic *bftl = container_of(ftl, struct nand_ftl_basic, nftl);
	struct nand_chip *nand = bftl->nftl.nand;
	const uint8_t *p = buf;
	const void *wrbuf;
	ufprog_status ret;

	if (retcount)
		*retcount = 0;

	ret = ftl_basic_get_block_page(bftl, part, page, &curr_block, &end_block, &offset_page);
	if (ret) /* Get rid of warning of no initial assignment of ret */
		return ret;

	retries = FTL_SKB_RETRIES;

	while (count && retries) {
		if (curr_block >= end_block) {
			logm_err("No enough space for write at block %u\n", curr_block);
			ret = UFP_FLASH_ADDRESS_OUT_OF_RANGE;
			break;
		}

		if (!(bftl->flags & FTL_BASIC_F_DONT_CHECK_BAD)) {
			ret = ftl_basic_is_good_block(bftl, curr_block);
			if (ret) {
				if (ret != UFP_FAIL)
					break;

				curr_block++;
				continue;
			}
		}

		curr_page = (curr_block << nand->maux.pages_per_block_shift) + offset_page;
		curr_cnt = nand->memorg.pages_per_block - offset_page;
		if (curr_cnt > count)
			curr_cnt = count;

		if (cb && cb->buffer)
			wrbuf = cb->buffer;
		else
			wrbuf = p;

		if (cb && cb->pre) {
			ret = cb->pre(cb, curr_cnt);
			if (ret)
				break;
		}

		ret = ufprog_nand_write_pages(nand, curr_page, curr_cnt, wrbuf, raw, ignore_error, &retcnt);

		if (cb && retcnt) {
			ret = cb->post(cb, retcnt);
			if (ret) {
				wrcnt += retcnt;
				break;
			}
		}

		if (ret) {
			logm_warn("Failed to write block %u at 0x%" PRIx64 ", starting torture test ...\n", curr_block,
				  (uint64_t)(curr_page + retcnt) << nand->maux.page_shift);

			/* Do torture test only when we're writing to the beginning of the block */
			if (offset_page)
				break;

			ret = ufprog_nand_torture_block(nand, curr_block);
			if (ret) {
				if (!ignore_error) {
					logm_warn("Torture test failed on block %u. Aborting ...\n");
					break;
				}

				logm_warn("Torture test failed on block %u. Marking it bad ...\n", curr_block);

				ret = ufprog_nand_markbad(nand, NULL, curr_block);
				if (ret)
					break;

				ufprog_bbt_set_state(bftl->bbt, curr_block, BBT_ST_BAD);

				curr_block++;
			} else {
				logm_info("Torture test passed on block %u. Retrying ...\n", curr_block);

				ufprog_bbt_set_state(bftl->bbt, curr_block, BBT_ST_ERASED);
			}

			retries--;
			continue;
		}

		offset_page = 0;
		wrcnt += curr_cnt;
		count -= curr_cnt;
		curr_block++;
		p += curr_cnt * nand->maux.oob_page_size;
		retries = FTL_SKB_RETRIES;
	}

	if (retcount)
		*retcount = wrcnt;

	return ret;
}

static ufprog_status ftl_basic_write_page(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					    uint32_t page, const void *buf, bool raw)
{
	return ftl_basic_write_pages(ftl, part, page, 1, buf, raw, false, NULL, NULL);
}

static ufprog_status ftl_basic_erase_blocks(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					    uint32_t block, uint32_t count, ufprog_bool spread, uint32_t *retcount,
					    struct ufprog_ftl_callback *cb)
{
	struct nand_ftl_basic *bftl = container_of(ftl, struct nand_ftl_basic, nftl);
	uint32_t curr_block, end_block, retries, ecnt = 0;
	struct nand_chip *nand = bftl->nftl.nand;
	ufprog_status ret = UFP_FAIL;

	STATUS_CHECK_RET(ftl_basic_get_block_page(bftl, part, block << nand->maux.pages_per_block_shift, &curr_block,
						  &end_block, NULL));

	retries = FTL_SKB_RETRIES;

	while (count && retries) {
		if (curr_block >= end_block) {
			logm_err("No enough space for erase at block %u\n", curr_block);
			ret = UFP_FLASH_ADDRESS_OUT_OF_RANGE;
			break;
		}

		if (!(bftl->flags & FTL_BASIC_F_DONT_CHECK_BAD)) {
			ret = ftl_basic_is_good_block(bftl, curr_block);
			if (ret) {
				if (ret != UFP_FAIL)
					break;

				curr_block++;
				continue;
			}
		}

		ret = ufprog_nand_erase_block(nand, curr_block << nand->maux.pages_per_block_shift);
		if (ret) {
			logm_warn("Failed to erase block %u at 0x%" PRIx64 ", starting torture test ...\n", curr_block,
				  curr_block << nand->maux.block_shift);

			ret = ufprog_nand_torture_block(nand, curr_block);
			if (ret) {
				if (!spread) {
					logm_warn("Torture test failed on block %u. Aborting ...\n");
					break;
				}

				logm_warn("Torture test failed on block %u. Marking it bad ...\n", curr_block);

				ret = ufprog_nand_markbad(nand, NULL, curr_block);
				if (ret)
					break;

				ufprog_bbt_set_state(bftl->bbt, curr_block, BBT_ST_BAD);

				curr_block++;
				retries--;
				continue;
			}

			logm_info("Torture test passed on block %u\n", curr_block);
		}

		ufprog_bbt_set_state(bftl->bbt, curr_block, BBT_ST_ERASED);

		count--;
		ecnt++;
		curr_block++;
		retries = FTL_SKB_RETRIES;

		if (cb) {
			ret = cb->post(cb, 1);
			if (ret)
				break;
		}
	}

	if (retcount)
		*retcount = ecnt;

	return ret;
}

static ufprog_status ftl_basic_erase_block(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					   uint32_t page, ufprog_bool spread)
{
	return ftl_basic_erase_blocks(ftl, part, page >> ftl->nand->maux.pages_per_block_shift, 1, spread, NULL, NULL);
}

static ufprog_status ftl_basic_block_checkbad(struct ufprog_nand_ftl *ftl, uint32_t block)
{
	struct nand_ftl_basic *bftl = container_of(ftl, struct nand_ftl_basic, nftl);

	return ufprog_bbt_is_bad(bftl->bbt, block);
}

static ufprog_status ftl_basic_block_markbad(struct ufprog_nand_ftl *ftl, uint32_t block)
{
	struct nand_ftl_basic *bftl = container_of(ftl, struct nand_ftl_basic, nftl);
	ufprog_status ret;

	ret = ufprog_nand_markbad(ftl->nand, NULL, block);
	if (!ret)
		ufprog_bbt_set_state(bftl->bbt, block, BBT_ST_BAD);

	return ret;
}

ufprog_status UFPROG_API ufprog_ftl_basic_create(const char *name, struct nand_chip *nand, struct ufprog_nand_bbt *bbt,
						 uint32_t flags, struct ufprog_nand_ftl **outftl)
{
	struct nand_ftl_basic *bftl;
	ufprog_status ret;
	size_t namelen;

	if (!name || !nand || !outftl)
		return UFP_INVALID_PARAMETER;

	namelen = strlen(name);

	bftl = calloc(1, sizeof(*bftl) + namelen + 1);
	if (!bftl)
		return UFP_NOMEM;

	bftl->nftl.nand = nand;
	bftl->bbt = bbt;
	bftl->flags = flags;

	ret = bitmap_create(BM_CELL_TYPE_PTR, 1, nand->maux.block_count, 0, &bftl->bm);
	if (ret) {
		logm_err("Failed to create block bitmap\n");
		free(bftl);
		return ret;
	}

	if (!bftl->bbt) {
		ret = ufprog_bbt_ram_create("bbt", nand, &bftl->bbt);
		if (ret) {
			logm_err("Failed to create default BBT\n");
			bitmap_free(bftl->bm);
			free(bftl);
			return ret;
		}

		bftl->free_bbt = true;
	}

	bftl->nftl.name = (char *)bftl + sizeof(*bftl);
	memcpy(bftl->nftl.name, name, namelen + 1);

	bftl->nftl.free_ni = ftl_basic_free;
	bftl->nftl.get_size = ftl_basic_get_size;
	bftl->nftl.read_page = ftl_basic_read_page;
	bftl->nftl.read_pages = ftl_basic_read_pages;
	bftl->nftl.write_page = ftl_basic_write_page;
	bftl->nftl.write_pages = ftl_basic_write_pages;
	bftl->nftl.erase_block = ftl_basic_erase_block;
	bftl->nftl.erase_blocks = ftl_basic_erase_blocks;
	bftl->nftl.block_checkbad = ftl_basic_block_checkbad;
	bftl->nftl.block_markbad = ftl_basic_block_markbad;

	*outftl = &bftl->nftl;

	return UFP_OK;
}
