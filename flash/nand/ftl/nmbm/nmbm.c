/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * FTL driver using NAND Mapped-block Management (NMBM)
 */

#include <malloc.h>
#include <ufprog/api_ftl.h>
#include <ufprog/config.h>
#include <ufprog/log.h>
#include <ufprog/nand.h>
#include <nmbm/nmbm.h>

#define NMBM_DRV_API_VER_MAJOR			1
#define NMBM_DRV_API_VER_MINOR			0

#define NMBM_DEFAULT_MAX_RATIO			1
#define NMBM_DEFAULT_MAX_RESERVED_BLOCKS	256

struct ufprog_ftl_instance {
	struct nand_chip *nand;
	struct nmbm_instance *ni;
	struct nand_info info;
	uint8_t *page_cache;
};

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
	return MAKE_VERSION(NMBM_DRV_API_VER_MAJOR, NMBM_DRV_API_VER_MINOR);
}

const char *UFPROG_API ufprog_plugin_desc(void)
{
	return "NAND Mapped-block Management (NMBM)";
}

static void nmbm_lower_log(void *arg, enum nmbm_log_category level, const char *fmt, va_list ap)
{
	log_level ll;

	switch (level) {
	case NMBM_LOG_DEBUG:
		ll = LOG_DEBUG;
		break;

	case NMBM_LOG_INFO:
		ll = LOG_INFO;
		break;

	case NMBM_LOG_WARN:
		ll = LOG_WARN;
		break;

	case NMBM_LOG_ERR:
	case NMBM_LOG_EMERG:
		ll = LOG_ERR;
		break;

	default:
		ll = LOG_NOTICE;
	}

	log_vprintf(ll, UFP_MODULE_NAME, fmt, ap);
}

static int nmbm_lower_read_page(void *arg, uint64_t addr, void *buf, void *oob, enum nmbm_oob_mode mode)
{
	struct ufprog_ftl_instance *ftl = arg;
	ufprog_status ret;

	ret = ufprog_nand_read_page(ftl->nand, (uint32_t)(addr >> ftl->info.maux.page_shift), ftl->page_cache,
				    mode == NMBM_MODE_RAW);
	if (!ret) {
		if (buf)
			memcpy(buf, ftl->page_cache, ftl->info.memorg.page_size);

		if (oob)
			memcpy(oob, ftl->page_cache + ftl->info.memorg.page_size, ftl->info.memorg.oob_size);

		return 0;
	}

	if (ret == UFP_ECC_UNCORRECTABLE)
		return -EBADMSG;

	return -EIO;
}

static int nmbm_lower_write_page(void *arg, uint64_t addr, const void *buf, const void *oob, enum nmbm_oob_mode mode)
{
	struct ufprog_ftl_instance *ftl = arg;
	ufprog_status ret;

	if (buf)
		memcpy(ftl->page_cache, buf, ftl->info.memorg.page_size);
	else
		memset(ftl->page_cache, 0xff, ftl->info.memorg.page_size);

	if (oob)
		memcpy(ftl->page_cache + ftl->info.memorg.page_size, oob, ftl->info.memorg.oob_size);
	else
		memset(ftl->page_cache + ftl->info.memorg.page_size, 0xff, ftl->info.memorg.oob_size);

	ret = ufprog_nand_write_page(ftl->nand, (uint32_t)(addr >> ftl->info.maux.page_shift), ftl->page_cache,
				     mode == NMBM_MODE_RAW);
	if (!ret)
		return 0;

	return -EIO;
}

static int nmbm_lower_erase_block(void *arg, uint64_t addr)
{
	struct ufprog_ftl_instance *ftl = arg;
	ufprog_status ret;

	ret = ufprog_nand_erase_block(ftl->nand, (uint32_t)(addr >> ftl->info.maux.page_shift));
	if (!ret)
		return 0;

	return -EIO;
}

static int nmbm_lower_is_bad_block(void *arg, uint64_t addr)
{
	struct ufprog_ftl_instance *ftl = arg;
	ufprog_status ret;

	ret = ufprog_nand_checkbad(ftl->nand, NULL, (uint32_t)(addr >> ftl->info.maux.block_shift));
	if (!ret)
		return 0;

	if (ret == UFP_FAIL)
		return 1;

	return -EIO;
}

static int nmbm_lower_mark_bad_block(void *arg, uint64_t addr)
{
	struct ufprog_ftl_instance *ftl = arg;
	ufprog_status ret;

	ret = ufprog_nand_markbad(ftl->nand, NULL, (uint32_t)(addr >> ftl->info.maux.block_shift));
	if (!ret)
		return 0;

	return -EIO;
}

ufprog_status UFPROG_API ufprog_ftl_create_instance(struct nand_chip *nand, struct json_object *config,
						    struct ufprog_ftl_instance **outinst)
{
	struct ufprog_ftl_instance *ftl;
	struct nmbm_lower_device nld;
	struct nand_info ninfo;
	ufprog_bool val;
	size_t ni_size;
	int rc;

	if (!nand || !outinst)
		return UFP_INVALID_PARAMETER;

	STATUS_CHECK_RET(ufprog_nand_info(nand, &ninfo));

	memset(&nld, 0, sizeof(nld));

	if (config) {
		STATUS_CHECK_RET(json_read_bool(config, "writable", &val));
		if (!val)
			nld.flags |= NMBM_F_READ_ONLY;

		STATUS_CHECK_RET(json_read_bool(config, "forced-create", &val));
		if (val)
			nld.flags |= NMBM_F_CREATE;

		STATUS_CHECK_RET(json_read_bool(config, "empty-page-ecc-protected", &val));
		if (val)
			nld.flags |= NMBM_F_EMPTY_PAGE_ECC_OK;

		STATUS_CHECK_RET(json_read_uint32(config, "max-ratio", &nld.max_ratio, NMBM_DEFAULT_MAX_RATIO));
		STATUS_CHECK_RET(json_read_uint32(config, "max-reserved-blocks", &nld.max_reserved_blocks,
						  NMBM_DEFAULT_MAX_RESERVED_BLOCKS));
	} else {
		nld.flags = NMBM_F_CREATE | NMBM_F_READ_ONLY;
		nld.max_ratio = NMBM_DEFAULT_MAX_RATIO;
		nld.max_reserved_blocks = NMBM_DEFAULT_MAX_RESERVED_BLOCKS;
	}

	nld.size = ninfo.maux.size;
	nld.erasesize = ninfo.maux.block_size;
	nld.writesize = ninfo.memorg.page_size;
	nld.oobsize = ninfo.memorg.oob_size;

	nld.read_page = nmbm_lower_read_page;
	nld.write_page = nmbm_lower_write_page;
	nld.erase_block = nmbm_lower_erase_block;
	nld.is_bad_block = nmbm_lower_is_bad_block;
	nld.mark_bad_block = nmbm_lower_mark_bad_block;

	nld.logprint = nmbm_lower_log;

	ni_size = nmbm_calc_structure_size(&nld);

	ftl = calloc(sizeof(*ftl) + ni_size + ninfo.maux.oob_page_size, 1);
	if (!ftl) {
		logm_err("No memory for NMBM FTL driver instance\n");
		return UFP_NOMEM;
	}

	ftl->nand = nand;
	ftl->ni = (struct nmbm_instance *)((uintptr_t)ftl + sizeof(*ftl));
	ftl->page_cache = (uint8_t *)((uintptr_t)ftl->ni + ni_size);

	memcpy(&ftl->info, &ninfo, sizeof(ninfo));

	nld.arg = ftl;

	rc = nmbm_attach(&nld, ftl->ni);
	if (rc) {
		free(ftl);
		return UFP_FAIL;
	}

	*outinst = ftl;

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_ftl_free_instance(struct ufprog_ftl_instance *inst)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	nmbm_detach(inst->ni);

	free(inst);

	return UFP_OK;
}

uint64_t UFPROG_API ufprog_ftl_get_size(struct ufprog_ftl_instance *inst)
{
	if (!inst)
		return UFP_INVALID_PARAMETER;

	return nmbm_get_avail_size(inst->ni);
}

ufprog_status UFPROG_API ufprog_ftl_read_page(struct ufprog_ftl_instance *inst, const struct ufprog_ftl_part *part,
					      uint32_t page, void *buf, ufprog_bool raw)
{
	uint64_t addr;
	int rc;

	if (!inst)
		return UFP_INVALID_PARAMETER;

	addr = (uint64_t)page << inst->info.maux.page_shift;
	if (part)
		addr += (uint64_t)part->base_block << inst->info.maux.block_shift;

	rc = nmbm_read_single_page(inst->ni, addr, buf, (uint8_t *)buf + inst->info.memorg.page_size,
				   raw ? NMBM_MODE_RAW : NMBM_MODE_PLACE_OOB);
	if (!rc)
		return 0;

	if (rc == -EBADMSG)
		return UFP_ECC_UNCORRECTABLE;

	return UFP_DEVICE_IO_ERROR;
}

ufprog_status UFPROG_API ufprog_ftl_write_page(struct ufprog_ftl_instance *inst, const struct ufprog_ftl_part *part,
					       uint32_t page, const void *buf, ufprog_bool raw)
{
	uint64_t addr;
	int rc;

	if (!inst)
		return UFP_INVALID_PARAMETER;

	addr = (uint64_t)page << inst->info.maux.page_shift;
	if (part)
		addr += (uint64_t)part->base_block << inst->info.maux.block_shift;

	rc = nmbm_write_single_page(inst->ni, addr, buf, (uint8_t *)buf + inst->info.memorg.page_size,
				    raw ? NMBM_MODE_RAW : NMBM_MODE_PLACE_OOB);
	if (!rc)
		return 0;

	return UFP_DEVICE_IO_ERROR;
}

ufprog_status UFPROG_API ufprog_ftl_erase_block(struct ufprog_ftl_instance *inst, const struct ufprog_ftl_part *part,
						uint32_t page, ufprog_bool spread)
{
	uint64_t addr;
	int rc;

	if (!inst)
		return UFP_INVALID_PARAMETER;

	addr = (uint64_t)page << inst->info.maux.page_shift;
	if (part)
		addr += (uint64_t)part->base_block << inst->info.maux.block_shift;

	rc = nmbm_erase_block_range(inst->ni, addr, inst->info.maux.block_size, NULL);
	if (!rc)
		return 0;

	return UFP_DEVICE_IO_ERROR;
}

ufprog_status UFPROG_API ufprog_ftl_block_checkbad(struct ufprog_ftl_instance *inst, uint32_t block)
{
	int rc;

	if (!inst)
		return UFP_INVALID_PARAMETER;

	rc = nmbm_check_bad_block(inst->ni, (uint64_t)block << inst->info.maux.block_shift);
	if (!rc)
		return UFP_OK;

	if (rc > 0)
		return UFP_FAIL;

	return UFP_DEVICE_IO_ERROR;
}

ufprog_status UFPROG_API ufprog_ftl_block_markbad(struct ufprog_ftl_instance *inst, uint32_t block)
{
	int rc;

	if (!inst)
		return UFP_INVALID_PARAMETER;

	rc = nmbm_mark_bad_block(inst->ni, (uint64_t)block << inst->info.maux.block_shift);
	if (!rc)
		return UFP_OK;

	return UFP_DEVICE_IO_ERROR;
}
