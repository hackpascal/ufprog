// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Simple Flash Translation Layer (FTL) management
 */

#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <ufprog/log.h>
#include "internal/nand-internal.h"
#include "internal/ecc-internal.h"
#include "internal/ftl-internal.h"

static ufprog_status generic_ftl_read_pages(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					    uint32_t page, uint32_t count, void *buf, bool raw, uint32_t flags,
					    uint32_t *retcount, struct ufprog_ftl_callback *cb);

static ufprog_status generic_ftl_write_pages(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					     uint32_t page, uint32_t count, const void *buf, bool raw,
					     ufprog_bool ignore_error, uint32_t *retcount,
					     struct ufprog_ftl_callback *cb);

static ufprog_status generic_ftl_erase_blocks(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					      uint32_t block, uint32_t count, ufprog_bool spread, uint32_t *retcount,
					      struct ufprog_ftl_callback *cb);

ufprog_status UFPROG_API ufprog_ftl_create(const char *drvname, const char *name, struct nand_chip *nand,
					   struct json_object *config, struct ufprog_nand_ftl **outftl)
{
	struct ufprog_ftl_instance *ftlinst;
	struct ufprog_ftl_driver *drv;
	struct ufprog_nand_ftl *ftl;
	ufprog_status ret;
	size_t namelen;

	if (!drvname || !outftl || !nand)
		return UFP_INVALID_PARAMETER;

	ret = ufprog_load_ftl_driver(drvname, &drv);
	if (ret)
		return ret;

	ret = drv->create_instance(nand, config, &ftlinst);
	if (ret)
		goto cleanup_unload_driver;

	ret = ufprog_ftl_add_instance(drv, ftlinst);
	if (ret)
		goto cleanup_free_instance;

	if (!name)
		name = drvname;

	namelen = strlen(name);

	ftl = calloc(1, sizeof(*ftl) + namelen + 1);
	if (!ftl) {
		log_err("No memory for FTL\n");
		ret = UFP_NOMEM;
		goto cleanup_remove_instance;
	}

	ftl->name = (char *)ftl + sizeof(*ftl);
	memcpy(ftl->name, name, namelen + 1);

	ftl->driver = drv;
	ftl->instance = ftlinst;
	ftl->nand = nand;

	*outftl = ftl;
	return UFP_OK;

cleanup_remove_instance:
	ufprog_ftl_remove_instance(drv, ftlinst);

cleanup_free_instance:
	drv->free_instance(ftlinst);

cleanup_unload_driver:
	ufprog_unload_ftl_driver(drv);

	return ret;
}

ufprog_status UFPROG_API ufprog_ftl_free(struct ufprog_nand_ftl *ftl)
{
	if (!ftl)
		return UFP_INVALID_PARAMETER;

	if (ftl->driver && ftl->instance) {
		ufprog_ftl_remove_instance(ftl->driver, ftl->instance);
		ftl->driver->free_instance(ftl->instance);
		ufprog_unload_ftl_driver(ftl->driver);
	} else {
		if (ftl->free_ni)
			return ftl->free_ni(ftl);
	}

	free(ftl);

	return UFP_OK;
}

const char *UFPROG_API ufprog_ftl_name(struct ufprog_nand_ftl *ftl)
{
	if (!ftl)
		return NULL;

	return ftl->name;
}

uint64_t UFPROG_API ufprog_ftl_get_size(struct ufprog_nand_ftl *ftl)
{
	if (!ftl)
		return 0;

	if (!ftl->size) {
		if (ftl->driver && ftl->instance)
			ftl->size = ftl->driver->get_size(ftl->instance);
		else
			ftl->size = ftl->get_size(ftl);

		ftl->ftl_total_pages = (uint32_t)(ftl->size >> ftl->nand->maux.page_shift);
	}

	return ftl->size;
}

static bool validate_virt_part_info(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part, uint32_t page,
				    uint32_t count)
{
	uint32_t part_page_start, part_pages;

	if (!ftl->size)
		ufprog_ftl_get_size(ftl);

	if (part) {
		part_page_start = part->base_block << ftl->nand->maux.pages_per_block_shift;
		part_pages = part->block_count << ftl->nand->maux.pages_per_block_shift;

		if (part_page_start >= ftl->ftl_total_pages || (part_page_start + part_pages) > ftl->ftl_total_pages)
			return false;

		if (page >= part_pages || (page + count) > part_pages)
			return false;
	} else {
		if (page >= ftl->ftl_total_pages)
			return false;
	}

	return true;
}

static inline ufprog_status ftl_read_page(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					  uint32_t page, void *buf, ufprog_bool raw)
{
	if (ftl->driver && ftl->instance)
		return ftl->driver->read_page(ftl->instance, part, page, buf, raw);

	return ftl->read_page(ftl, part, page, buf, raw);
}

ufprog_status UFPROG_API ufprog_ftl_read_page(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					      uint32_t page, void *buf, ufprog_bool raw)
{
	if (!ftl || !buf)
		return UFP_INVALID_PARAMETER;

	if (!validate_virt_part_info(ftl, part, page, 1))
		return UFP_INVALID_PARAMETER;

	return ftl_read_page(ftl, part, page, buf, raw);
}

ufprog_status UFPROG_API ufprog_ftl_read_pages(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					       uint32_t page, uint32_t count, void *buf, ufprog_bool raw,
					       uint32_t flags, uint32_t *retcount, struct ufprog_ftl_callback *cb)
{
	if (retcount)
		*retcount = 0;

	if (!ftl)
		return UFP_INVALID_PARAMETER;

	if (!buf) {
		if (!cb || !cb->buffer)
			return UFP_INVALID_PARAMETER;
	}

	if (!validate_virt_part_info(ftl, part, page, count))
		return UFP_INVALID_PARAMETER;

	if (!count)
		return UFP_OK;

	if (ftl->driver && ftl->driver->read_pages && ftl->instance)
		return ftl->driver->read_pages(ftl->instance, part, page, count, buf, raw, flags, retcount, cb);

	if (ftl->read_pages)
		return ftl->read_pages(ftl, part, page, count, buf, raw, flags, retcount, cb);

	return generic_ftl_read_pages(ftl, part, page, count, buf, raw, flags, retcount, cb);
}

static inline ufprog_status ftl_write_page(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					   uint32_t page, const void *buf, ufprog_bool raw)
{
	if (ftl->driver && ftl->instance)
		return ftl->driver->write_page(ftl->instance, part, page, buf, raw);

	return ftl->write_page(ftl, part, page, buf, raw);
}

ufprog_status UFPROG_API ufprog_ftl_write_page(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					       uint32_t page, const void *buf, ufprog_bool raw)
{
	if (!ftl || !buf)
		return UFP_INVALID_PARAMETER;

	if (!validate_virt_part_info(ftl, part, page, 1))
		return UFP_INVALID_PARAMETER;

	return ftl_write_page(ftl, part, page, buf, raw);
}

ufprog_status UFPROG_API ufprog_ftl_write_pages(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
						uint32_t page, uint32_t count, const void *buf, ufprog_bool raw,
						ufprog_bool ignore_error, uint32_t *retcount,
						struct ufprog_ftl_callback *cb)
{
	if (retcount)
		*retcount = 0;

	if (!ftl)
		return UFP_INVALID_PARAMETER;

	if (!buf) {
		if (!cb || !cb->buffer)
			return UFP_INVALID_PARAMETER;
	}

	if (!validate_virt_part_info(ftl, part, page, count))
		return UFP_INVALID_PARAMETER;

	if (!count)
		return UFP_OK;

	if (ftl->driver && ftl->driver->write_pages && ftl->instance)
		return ftl->driver->write_pages(ftl->instance, part, page, count, buf, raw, ignore_error, retcount, cb);

	if (ftl->write_pages)
		return ftl->write_pages(ftl, part, page, count, buf, raw, ignore_error, retcount, cb);

	return generic_ftl_write_pages(ftl, part, page, count, buf, raw, ignore_error, retcount, cb);
}

static inline ufprog_status ftl_erase_block(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					    uint32_t page, ufprog_bool spread)
{
	if (ftl->driver && ftl->instance)
		return ftl->driver->erase_block(ftl->instance, part, page, spread);

	return ftl->erase_block(ftl, part, page, spread);
}

ufprog_status UFPROG_API ufprog_ftl_erase_block(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
						uint32_t page, ufprog_bool spread)
{
	if (!ftl)
		return UFP_INVALID_PARAMETER;

	if (!validate_virt_part_info(ftl, part, page, 1))
		return UFP_INVALID_PARAMETER;

	return ftl_erase_block(ftl, part, page, spread);
}

ufprog_status UFPROG_API ufprog_ftl_erase_blocks(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
						 uint32_t block, uint32_t count, ufprog_bool spread, uint32_t *retcount,
						 struct ufprog_ftl_callback *cb)
{
	if (retcount)
		*retcount = 0;

	if (!ftl)
		return UFP_INVALID_PARAMETER;

	if (!validate_virt_part_info(ftl, part, block << ftl->nand->maux.pages_per_block_shift,
				     count << ftl->nand->maux.pages_per_block_shift))
		return UFP_INVALID_PARAMETER;

	if (!count)
		return UFP_OK;

	if (ftl->driver && ftl->driver->erase_blocks && ftl->instance)
		return ftl->driver->erase_blocks(ftl->instance, part, block, count, spread, retcount, cb);

	if (ftl->erase_blocks)
		return ftl->erase_blocks(ftl, part, block, count, spread, retcount, cb);

	return generic_ftl_erase_blocks(ftl, part, block, count, spread, retcount, cb);
}

ufprog_status UFPROG_API ufprog_ftl_block_checkbad(struct ufprog_nand_ftl *ftl, uint32_t block)
{
	if (!ftl)
		return UFP_INVALID_PARAMETER;

	if (ftl->driver && ftl->instance)
		return ftl->driver->block_checkbad(ftl->instance, block);

	return ftl->block_checkbad(ftl, block);
}

ufprog_status UFPROG_API ufprog_ftl_block_markbad(struct ufprog_nand_ftl *ftl, uint32_t block)
{
	if (!ftl)
		return UFP_INVALID_PARAMETER;

	if (ftl->driver && ftl->instance)
		return ftl->driver->block_markbad(ftl->instance, block);

	return ftl->block_markbad(ftl, block);
}

static ufprog_status generic_ftl_read_pages(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					    uint32_t page, uint32_t count, void *buf, bool raw, uint32_t flags,
					    uint32_t *retcount, struct ufprog_ftl_callback *cb)
{
	ufprog_status ret = UFP_OK;
	uint32_t rdcnt = 0;
	uint8_t *p = buf;
	void *rdbuf;

	while (count) {
		if (cb && cb->buffer)
			rdbuf = cb->buffer;
		else
			rdbuf = p;

		ret = ftl_read_page(ftl, part, page, rdbuf, raw);
		if (ret) {
			if (ret == UFP_ECC_UNCORRECTABLE) {
				if (!(flags & NAND_READ_F_IGNORE_ECC_ERROR))
					break;
			} else if (!(flags & NAND_READ_F_IGNORE_IO_ERROR)) {
				logm_err("Failed to read page %u at 0x%" PRIx64 "\n", page + rdcnt,
					 (uint64_t)(page + rdcnt) << ftl->nand->maux.page_shift);
				break;
			}

			ret = UFP_OK;
		}

		page++;
		count--;
		rdcnt++;
		p += ftl->nand->maux.oob_page_size;

		if (cb) {
			ret = cb->post(cb, 1);
			if (ret)
				break;
		}
	}

	if (retcount)
		*retcount = rdcnt;

	return ret;
}

static ufprog_status generic_ftl_write_pages(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					     uint32_t page, uint32_t count, const void *buf, bool raw,
					     ufprog_bool ignore_error, uint32_t *retcount,
					     struct ufprog_ftl_callback *cb)
{
	ufprog_status ret = UFP_OK;
	const uint8_t *p = buf;
	uint32_t wrcnt = 0;
	const void *wrbuf;

	while (count) {
		if (cb && cb->buffer)
			wrbuf = cb->buffer;
		else
			wrbuf = p;

		if (cb) {
			ret = cb->pre(cb, 1);
			if (ret)
				break;
		}

		ret = ftl_write_page(ftl, part, page, wrbuf, raw);
		if (ret) {
			if (!ignore_error) {
				logm_err("Failed to write page %u at 0x%" PRIx64 "\n", page + wrcnt,
					 (uint64_t)(page + wrcnt) << ftl->nand->maux.page_shift);
				break;
			}

			ret = UFP_OK;
		}

		page++;
		count--;
		wrcnt++;
		p += ftl->nand->maux.oob_page_size;

		if (cb) {
			ret = cb->post(cb, 1);
			if (ret)
				break;
		}
	}

	if (retcount)
		*retcount = wrcnt;

	return ret;
}

static ufprog_status generic_ftl_erase_blocks(struct ufprog_nand_ftl *ftl, const struct ufprog_ftl_part *part,
					      uint32_t block, uint32_t count, ufprog_bool spread, uint32_t *retcount,
					      struct ufprog_ftl_callback *cb)
{
	ufprog_status ret = UFP_OK;
	uint32_t page, ecnt = 0;

	page = block << ftl->nand->maux.pages_per_block_shift;

	while (count) {
		ret = ftl_erase_block(ftl, part, page, spread);
		if (ret)
			break;

		count--;
		ecnt++;
		page += ftl->nand->memorg.pages_per_block;

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
