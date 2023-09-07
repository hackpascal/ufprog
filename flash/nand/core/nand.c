// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Generic NAND flash support
 */

#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <ufprog/log.h>
#include <ufprog/ecc.h>
#include "internal/nand-internal.h"
#include "internal/ecc-internal.h"

#define TORTURE_TEST_PAT			0x5a
#define TORTURE_TEST_CMP_PAT			((uint8_t)~TORTURE_TEST_PAT)

int UFPROG_API ufprog_nand_check_buf_bitflips(const void *buf, size_t len, uint32_t bitflips,
					      uint32_t bitflips_threshold)
{
	const uint8_t *buf8 = buf;
	const uint32_t *buf32;
	uint32_t d, weight;

	if (!buf || !len)
		return 0;

	while (len && ((size_t)buf8) % sizeof(uint32_t)) {
		weight = hweight8(*buf8);
		bitflips += 8 - weight;
		buf8++;
		len--;

		if (bitflips > bitflips_threshold)
			return -1;
	}

	buf32 = (const uint32_t *)buf8;
	while (len >= sizeof(uint32_t)) {
		d = *buf32;

		if (d != ~0) {
			weight = hweight32(d);
			bitflips += sizeof(uint32_t) * 8 - weight;
		}

		buf32++;
		len -= sizeof(uint32_t);

		if (bitflips > bitflips_threshold)
			return -1;
	}

	buf8 = (const uint8_t *)buf32;
	while (len) {
		weight = hweight8(*buf8);
		bitflips += 8 - weight;
		buf8++;
		len--;

		if (bitflips > bitflips_threshold)
			return -1;
	}

	return bitflips;
}

int UFPROG_API ufprog_nand_check_buf_bitflips_by_bits(const void *buf, size_t bits, uint32_t bitflips,
						      uint32_t bitflips_threshold)
{
	size_t len;
	uint32_t i;
	uint8_t b;
	int rc;

	len = bits >> 3;
	bits &= 7;

	rc = ufprog_nand_check_buf_bitflips(buf, len, bitflips, bitflips_threshold);
	if (!bits || rc < 0)
		return rc;

	bitflips = rc;

	/* We want a precise count of bits */
	b = ((const uint8_t *)buf)[len];
	for (i = 0; i < bits; i++) {
		if (!(b & BIT(i)))
			bitflips++;
	}

	if (bitflips > bitflips_threshold)
		return -1;

	return bitflips;
}

static bool nand_id_has_period(const struct nand_id *id, uint32_t period)
{
	uint32_t i, j;

	for (i = 0; i < period; i++) {
		for (j = i + period; j < id->len; j += period) {
			if (id->id[i] != id->id[j])
				return false;
		}
	}

	return true;
}

void UFPROG_API ufprog_nand_compute_id_len(struct nand_id *id)
{
	uint32_t last_nonzero, last_repeated, period;

	id->len = sizeof(id->id);

	for (last_nonzero = id->len; last_nonzero > 0; last_nonzero--)
		if (id->id[last_nonzero - 1])
			break;

	if (last_nonzero == 0) {
		id->len = 0;
		return;
	}

	for (period = 1; period < id->len; period++) {
		if (nand_id_has_period(id, period))
			break;
	}

	if (period < id->len) {
		id->len = period;
		return;
	}

	last_repeated = id->len - 1;
	while (last_repeated > 0) {
		if (id->id[id->len - 1] != id->id[last_repeated - 1])
			break;
		last_repeated--;
	}

	if (last_repeated && last_repeated < id->len - 1 && id->id[id->len - 1] == id->id[last_repeated]) {
		id->len = last_repeated;
		return;
	}

	if (last_nonzero < id->len) {
		id->len = last_nonzero;
		return;
	}
}

const char *UFPROG_API ufprog_nand_cell_type(struct nand_chip *nand)
{
	switch (nand->bits_per_cell) {
	case 1:
		return "SLC";
	case 2:
		return "MLC";
	case 3:
		return "TLC";
	case 4:
		return "QLC";
	default:
		return "Unknown";
	}
}

ufprog_status UFPROG_API ufprog_nand_info(struct nand_chip *nand, struct nand_info *retinfo)
{
	ufprog_status ret;

	if (!nand || !retinfo)
		return UFP_INVALID_PARAMETER;

	memset(retinfo, 0, sizeof(*retinfo));

	memcpy(&retinfo->id, &nand->id, sizeof(nand->id));
	memcpy(&retinfo->ecc_req, &nand->ecc_req, sizeof(nand->ecc_req));
	memcpy(&retinfo->maux, &nand->maux, sizeof(nand->maux));
	memcpy(&retinfo->memorg, &nand->memorg, sizeof(nand->memorg));

	retinfo->model = nand->model;
	retinfo->vendor = nand->vendor;
	retinfo->bus_width = nand->bus_width;
	retinfo->bits_per_cell = nand->bits_per_cell;
	retinfo->nops = nand->nops;
	retinfo->random_page_write = nand->random_page_write;

	if (nand->read_uid) {
		ret = nand->read_uid(nand, NULL, &retinfo->uid_length);
		if (ret)
			retinfo->uid_length = 0;
	}

	if (nand->otp)
		retinfo->otp_pages = nand->otp->count;

	return UFP_OK;
}

void UFPROG_API ufprog_nand_update_param(struct nand_chip *nand)
{
	uint64_t lun_size;

	if (!nand)
		return;

	nand->maux.size = nand_flash_compute_chip_size(&nand->memorg);

	nand->maux.chip_shift = ffs64(nand->maux.size) - 1;
	nand->maux.chip_mask = (1ULL << nand->maux.chip_shift) - 1;

	nand->maux.oob_page_size = nand->memorg.page_size + nand->memorg.oob_size;
	nand->maux.page_shift = ffs(nand->memorg.page_size) - 1;
	nand->maux.page_mask = (1U << nand->maux.page_shift) - 1;
	nand->maux.page_count = (uint32_t)(nand->maux.size >> nand->maux.page_shift);

	nand->maux.pages_per_block_shift = ffs(nand->memorg.pages_per_block) - 1;
	nand->maux.pages_per_block_mask = (1U << nand->maux.pages_per_block_shift) - 1;

	nand->maux.block_size = nand->memorg.page_size << nand->maux.pages_per_block_shift;
	nand->maux.oob_block_size = nand->maux.oob_page_size << nand->maux.pages_per_block_shift;
	nand->maux.block_shift = nand->maux.page_shift + nand->maux.pages_per_block_shift;
	nand->maux.block_mask = (1U << nand->maux.block_shift) - 1;
	nand->maux.block_count = (uint32_t)(nand->maux.size >> nand->maux.block_shift);

	lun_size = (uint64_t)nand->memorg.blocks_per_lun * nand->maux.block_size;
	nand->maux.lun_shift = ffs64(lun_size) - 1;
	nand->maux.lun_mask = (1ULL << nand->maux.lun_shift) - 1;
}

ufprog_status UFPROG_API ufprog_nand_select_die(struct nand_chip *nand, uint32_t ce, uint32_t lun)
{
	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (ce >= nand->memorg.num_chips || lun >= nand->memorg.luns_per_cs)
		return UFP_INVALID_PARAMETER;

	return nand->select_die(nand, ce, lun);
}

void UFPROG_API ufprog_nand_print_ecc_result(struct nand_chip *nand, uint32_t page)
{
	const struct nand_ecc_status *status = ufprog_ecc_get_status(nand->ecc);
	uint32_t i;

	if (!status->per_step) {
		if (status->step_bitflips[0] == 1)
			logm_dbg("1 bitflip corrected in page %u\n", page);
		else if (status->step_bitflips[0] > 1)
			logm_dbg("%u bitflips corrected in page %u\n", status->step_bitflips[0], page);
		else if (status->step_bitflips[0] < 0)
			logm_err("Uncorrectable bitflips detected in page %u\n", page);

		return;
	}

	for (i = 0; i < nand->ecc_steps; i++) {
		if (status->step_bitflips[i] == 1)
			logm_dbg("1 bitflip corrected in page %u step %u\n", page, i);
		else if (status->step_bitflips[i] > 1)
			logm_dbg("%u bitflips corrected in page %u step %u\n", status->step_bitflips[i], page, i);
		else if (status->step_bitflips[i] < 0)
			logm_err("Uncorrectable bitflips detected in page %u step %u\n", page, i);
	}
}

ufprog_status UFPROG_API ufprog_nand_read_page(struct nand_chip *nand, uint32_t page, void *buf, ufprog_bool raw)
{
	ufprog_status ret;

	if (!nand || !buf)
		return UFP_INVALID_PARAMETER;

	if (page >= nand->maux.size >> nand->maux.page_shift)
		return UFP_FLASH_ADDRESS_OUT_OF_RANGE;

	if (nand->ecc)
		STATUS_CHECK_RET(ufprog_ecc_set_enable(nand->ecc, !raw));

	ret = nand->read_page(nand, page, 0, nand->maux.oob_page_size, buf);
	if (ret)
		return ret;

	if (!raw && nand->ecc) {
		ret = ufprog_ecc_decode_page(nand->ecc, buf);
		if (ret == UFP_ECC_CORRECTED || ret == UFP_ECC_UNCORRECTABLE) {
			ufprog_nand_print_ecc_result(nand, page);

			if (ret == UFP_ECC_CORRECTED)
				ret = UFP_OK;
		}
	}

	return ret;
}

ufprog_status UFPROG_API ufprog_nand_read_pages(struct nand_chip *nand, uint32_t page, uint32_t count, void *buf,
						ufprog_bool raw, uint32_t flags, uint32_t *retcount)
{
	ufprog_status ret = UFP_OK;
	uint32_t rdcnt = 0;
	uint8_t *p = buf;

	if (retcount)
		*retcount = 0;

	if (!nand || !buf)
		return UFP_INVALID_PARAMETER;

	if (page >= nand->maux.page_count || (page + count) > nand->maux.page_count)
		return UFP_FLASH_ADDRESS_OUT_OF_RANGE;

	if (!count)
		return UFP_OK;

	if (nand->ecc)
		STATUS_CHECK_RET(ufprog_ecc_set_enable(nand->ecc, !raw));

	if (nand->read_pages)
		return nand->read_pages(nand, page, count, buf, flags, retcount);

	while (count) {
		ret = nand->read_page(nand, page, 0, nand->maux.oob_page_size, p);
		if (ret) {
			if (!(flags & NAND_READ_F_IGNORE_IO_ERROR))
				break;

			ret = UFP_OK;
		}

		if (!raw && nand->ecc) {
			ret = ufprog_ecc_decode_page(nand->ecc, p);
			if (ret) {
				if (ret == UFP_ECC_CORRECTED || ret == UFP_ECC_UNCORRECTABLE) {
					ufprog_nand_print_ecc_result(nand, page);

					if (ret == UFP_ECC_UNCORRECTABLE) {
						if (!(flags & NAND_READ_F_IGNORE_ECC_ERROR))
							break;
					}
				} else if (!(flags & NAND_READ_F_IGNORE_IO_ERROR)) {
					break;
				}

				ret = UFP_OK;
			}
		}

		page++;
		count--;
		rdcnt++;
		p += nand->maux.oob_page_size;
	}

	if (retcount)
		*retcount = rdcnt;

	return ret;
}

ufprog_status UFPROG_API ufprog_nand_write_page(struct nand_chip *nand, uint32_t page, const void *buf,
						ufprog_bool raw)
{
	ufprog_status ret;
	const void *data;
	void *tmp;

	if (!nand || !buf)
		return UFP_INVALID_PARAMETER;

	if (page >= nand->maux.size >> nand->maux.page_shift)
		return UFP_FLASH_ADDRESS_OUT_OF_RANGE;

	if (nand->ecc)
		STATUS_CHECK_RET(ufprog_ecc_set_enable(nand->ecc, !raw));

	if (!raw && nand->ecc) {
		tmp = nand->page_cache[0];
		memcpy(tmp, buf, nand->maux.oob_page_size);

		ret = ufprog_ecc_encode_page(nand->ecc, tmp);
		if (ret)
			return ret;

		data = tmp;
	} else {
		data = buf;
	}

	return nand->write_page(nand, page, 0, nand->maux.oob_page_size, data);
}

ufprog_status UFPROG_API ufprog_nand_write_pages(struct nand_chip *nand, uint32_t page, uint32_t count, const void *buf,
						 ufprog_bool raw, ufprog_bool ignore_error, uint32_t *retcount)
{
	ufprog_status ret = UFP_OK;
	const uint8_t *p = buf;
	uint32_t wrcnt = 0;
	const void *data;
	void *tmp;

	if (retcount)
		*retcount = 0;

	if (!nand || !buf)
		return UFP_INVALID_PARAMETER;

	if (page >= nand->maux.page_count || (page + count) > nand->maux.page_count)
		return UFP_FLASH_ADDRESS_OUT_OF_RANGE;

	if (!count)
		return UFP_OK;

	if (nand->ecc)
		STATUS_CHECK_RET(ufprog_ecc_set_enable(nand->ecc, !raw));

	if (nand->write_pages)
		return nand->write_pages(nand, page, count, buf, ignore_error, retcount);

	while (count) {
		if (!raw && nand->ecc) {
			tmp = nand->page_cache[0];
			memcpy(tmp, p, nand->maux.oob_page_size);

			ret = ufprog_ecc_encode_page(nand->ecc, tmp);
			if (ret) {
				if (!ignore_error)
					break;

				ret = UFP_OK;
			}

			data = tmp;
		} else {
			data = p;
		}

		ret = nand->write_page(nand, page, 0, nand->maux.oob_page_size, data);
		if (ret) {
			if (!ignore_error)
				break;

			ret = UFP_OK;
		}

		page++;
		count--;
		wrcnt++;
		p += nand->maux.oob_page_size;
	}

	if (retcount)
		*retcount = wrcnt;

	return ret;
}

ufprog_status UFPROG_API ufprog_nand_erase_block(struct nand_chip *nand, uint32_t page)
{
	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (page >= nand->maux.size >> nand->maux.page_shift)
		return UFP_FLASH_ADDRESS_OUT_OF_RANGE;

	return nand->erase_block(nand, page);
}

ufprog_status UFPROG_API ufprog_nand_read_uid(struct nand_chip *nand, void *data, uint32_t *retlen)
{
	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (!nand->read_uid)
		return UFP_UNSUPPORTED;

	return nand->read_uid(nand, data, retlen);
}

ufprog_status UFPROG_API ufprog_nand_otp_read(struct nand_chip *nand, uint32_t index, void *buf, ufprog_bool raw)
{
	ufprog_status ret;

	if (!nand || !buf)
		return UFP_INVALID_PARAMETER;

	if (!nand->otp || !nand->otp_ops)
		return UFP_UNSUPPORTED;

	if (index >= nand->otp->count)
		return UFP_INVALID_PARAMETER;

	if (nand->ecc)
		STATUS_CHECK_RET(ufprog_ecc_set_enable(nand->ecc, !raw));

	ret = nand->otp_ops->read(nand, index + nand->otp->start_index, 0, nand->maux.oob_page_size, buf);
	if (ret)
		return ret;

	if (!raw && nand->ecc) {
		ret = ufprog_ecc_decode_page(nand->ecc, buf);
		if (ret == UFP_ECC_CORRECTED || ret == UFP_ECC_UNCORRECTABLE) {
			ufprog_nand_print_ecc_result(nand, index);

			if (ret == UFP_ECC_CORRECTED)
				ret = UFP_OK;
		}
	}

	return ret;
}

ufprog_status UFPROG_API ufprog_nand_otp_write(struct nand_chip *nand, uint32_t index, const void *buf,
					       ufprog_bool raw)
{
	ufprog_status ret;
	const void *data;
	void *tmp;

	if (!nand || !buf)
		return UFP_INVALID_PARAMETER;

	if (!nand->otp || !nand->otp_ops)
		return UFP_UNSUPPORTED;

	if (index >= nand->otp->count)
		return UFP_INVALID_PARAMETER;

	if (nand->ecc)
		STATUS_CHECK_RET(ufprog_ecc_set_enable(nand->ecc, !raw));

	if (!raw && nand->ecc) {
		tmp = nand->page_cache[0];
		memcpy(tmp, buf, nand->maux.oob_page_size);

		ret = ufprog_ecc_encode_page(nand->ecc, tmp);
		if (ret)
			return ret;

		data = tmp;
	} else {
		data = buf;
	}

	return nand->otp_ops->write(nand, index + nand->otp->start_index, 0, nand->maux.oob_page_size, data);
}

ufprog_status UFPROG_API ufprog_nand_otp_lock(struct nand_chip *nand)
{
	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (!nand->otp || !nand->otp_ops)
		return UFP_UNSUPPORTED;

	return nand->otp_ops->lock(nand);
}

ufprog_status UFPROG_API ufprog_nand_otp_locked(struct nand_chip *nand, ufprog_bool *retlocked)
{
	if (!nand || !retlocked)
		return UFP_INVALID_PARAMETER;

	if (!nand->otp || !nand->otp_ops)
		return UFP_UNSUPPORTED;

	return nand->otp_ops->locked(nand, retlocked);
}

ufprog_status UFPROG_API ufprog_nand_convert_page_format(struct nand_chip *nand, const void *buf, void *out,
							 ufprog_bool from_canonical)
{
	if (!nand || !buf || !out)
		return UFP_INVALID_PARAMETER;

	if (!nand->ecc || !ufprog_ecc_support_convert_page_layout(nand->ecc)) {
		memcpy(out, buf, nand->maux.oob_page_size);
		return UFP_OK;
	}

	return ufprog_ecc_convert_page_layout(nand->ecc, buf, out, from_canonical);
}

ufprog_status UFPROG_API ufprog_nand_check_bbm(struct nand_chip *nand, const struct nand_bbm_config *bbmcfg,
					       uint32_t page)
{
	uint8_t *buf = nand->page_cache[0], *bad;
	uint32_t i, bbm_check_width;
	ufprog_status ret;

	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (!bbmcfg)
		bbmcfg = &nand->bbm_config;

	if ((bbmcfg->flags & ECC_F_BBM_RAW) && !(bbmcfg->flags & ECC_F_BBM_CANONICAL_LAYOUT)) {
		if (nand->ecc)
			STATUS_CHECK_RET(ufprog_ecc_set_enable(nand->ecc, false));

		for (i = 0; i < bbmcfg->check.num; i++) {
			ret = nand->read_page(nand, page, bbmcfg->check.pos[i], (bbmcfg->check.width + 7) / 8,
					      &buf[bbmcfg->check.pos[i]]);
			if (ret)
				return ret;
		}
	} else {
		ret = ufprog_nand_read_page(nand, page, buf, !!(bbmcfg->flags & ECC_F_BBM_RAW));
		if (ret)
			return ret;
	}


	if ((bbmcfg->flags & ECC_F_BBM_CANONICAL_LAYOUT) && nand->ecc &&
	    ufprog_ecc_support_convert_page_layout(nand->ecc)) {
		ret = ufprog_ecc_convert_page_layout(nand->ecc, nand->page_cache[0], nand->page_cache[1], false);
		if (ret)
			return ret;
		buf = nand->page_cache[1];
	}

	for (i = 0; i < bbmcfg->check.num; i++) {
		bad = &buf[bbmcfg->check.pos[i]];

		bbm_check_width = bbmcfg->check.width;
		if (!bbm_check_width)
			bbm_check_width = 8;

		while (bbm_check_width) {
			if (bbm_check_width >= 8) {
				if (*bad != 0xff)
					return UFP_FAIL;

				bad++;
				bbm_check_width -= 8;
			} else {
				if (hweight8(*bad) < bbm_check_width)
					return UFP_FAIL;

				break;
			}
		}
	}

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_nand_write_bbm(struct nand_chip *nand, const struct nand_bbm_config *bbmcfg,
					       uint32_t page)
{
	uint32_t i, j, bbm_mark_bytes, bbm_pos;
	uint8_t *buf = nand->page_cache[0];
	ufprog_status ret;

	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (!bbmcfg)
		bbmcfg = &nand->bbm_config;

	if (bbmcfg->flags & ECC_F_BBM_MARK_WHOLE_PAGE) {
		memset(buf, 0, nand->maux.oob_page_size);
	} else {
		memset(buf, 0xff, nand->maux.oob_page_size);

		for (i = 0; i < bbmcfg->mark.num; i++) {
			bbm_pos = bbmcfg->mark.pos[i];

			if (nand->bus_width == 16)
				bbm_pos &= ~0x01;

			bbm_mark_bytes = bbmcfg->mark.bytes;
			if (!bbm_mark_bytes)
				bbm_mark_bytes = nand->bus_width == 16 ? 2 : 1;

			for (j = 0; j < bbm_mark_bytes; j++)
				buf[bbm_pos + j] = 0;
		}
	}

	if ((bbmcfg->flags & ECC_F_BBM_CANONICAL_LAYOUT) && nand->ecc &&
	    ufprog_ecc_support_convert_page_layout(nand->ecc)) {
		ret = ufprog_ecc_convert_page_layout(nand->ecc, nand->page_cache[0], nand->page_cache[1], true);
		if (ret)
			return ret;
		buf = nand->page_cache[1];
	}

	return ufprog_nand_write_page(nand, page, buf, !!(bbmcfg->flags & ECC_F_BBM_RAW));
}

ufprog_status UFPROG_API ufprog_nand_checkbad(struct nand_chip *nand, const struct nand_bbm_config *bbmcfg,
					      uint32_t block)
{
	ufprog_status ret = UFP_FAIL;
	uint32_t i, page, fails = 0;

	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (block >= nand->maux.block_count)
		return UFP_INVALID_PARAMETER;

	if (!bbmcfg)
		bbmcfg = &nand->bbm_config;

	page = block << nand->maux.pages_per_block_shift;

	for (i = 0; i < bbmcfg->pages.num; i++) {
		ret = ufprog_nand_check_bbm(nand, bbmcfg, page + bbmcfg->pages.idx[i]);
		if (!ret)
			continue;

		if (ret == UFP_FAIL)
			return UFP_FAIL;

		logm_warn("Failed to check bad marker of block %u at page +%u\n", block, bbmcfg->pages.idx[i]);
		fails++;
	}

	if (fails < bbmcfg->pages.num)
		return UFP_OK;

	logm_warn("Failed to check whether block %u is bad\n", block);

	return UFP_DEVICE_IO_ERROR;
}

ufprog_status UFPROG_API ufprog_nand_markbad(struct nand_chip *nand, const struct nand_bbm_config *bbmcfg,
					     uint32_t block)
{
	ufprog_status ret = UFP_FAIL;
	uint32_t i, page, ok = 0;

	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (block >= nand->maux.block_count)
		return UFP_INVALID_PARAMETER;

	if (!bbmcfg)
		bbmcfg = &nand->bbm_config;

	page = block << nand->maux.pages_per_block_shift;

	for (i = 0; i < bbmcfg->pages.num; i++) {
		ret = ufprog_nand_write_bbm(nand, bbmcfg, page + bbmcfg->pages.idx[i]);
		if (ret) {
			logm_warn("Failed to write bad marker to block %u at page +%u\n", block, bbmcfg->pages.idx[i]);
			continue;
		}

		ret = ufprog_nand_check_bbm(nand, bbmcfg, page + bbmcfg->pages.idx[i]);
		if (!ret || ret != UFP_FAIL) {
			logm_warn("Failed to verify bad marker of block %u at page +%u\n", block, bbmcfg->pages.idx[i]);

			if (!ret)
				ret = UFP_DATA_VERIFICATION_FAIL;

			continue;
		}

		ok++;
	}

	if (ok) /* At least one page has been marked successfully */
		return UFP_OK;

	logm_err("Failed to mark block %u as bad at page +%u\n", block, bbmcfg->pages.idx[i]);

	return ret;
}

ufprog_bool UFPROG_API ufprog_nand_bbm_add_page(struct nand_chip *nand, struct nand_bbm_page_cfg *cfg,
						uint32_t page)
{
	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (page >= nand->memorg.pages_per_block)
		return UFP_INVALID_PARAMETER;

	return ufprog_ecc_bbm_add_page(cfg, page);
}

ufprog_bool UFPROG_API ufprog_nand_bbm_add_check_pos(struct nand_chip *nand, struct nand_bbm_check_cfg *cfg,
						     uint32_t pos)
{
	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (pos >= nand->maux.oob_page_size)
		return UFP_INVALID_PARAMETER;

	return ufprog_ecc_bbm_add_check_pos(cfg, pos);
}

ufprog_bool UFPROG_API ufprog_nand_bbm_add_mark_pos(struct nand_chip *nand, struct nand_bbm_mark_cfg *cfg,
						    uint32_t pos)
{
	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (pos >= nand->maux.oob_page_size)
		return UFP_INVALID_PARAMETER;

	return ufprog_ecc_bbm_add_mark_pos(cfg, pos);
}

static ufprog_status nand_setup_bbm_config(struct nand_chip *nand)
{
	ufprog_status ret;
	uint32_t i;

	if (nand->ecc) {
		ret = ufprog_ecc_get_bbm_config(nand->ecc, &nand->bbm_config);
		if (ret)
			return ret;
	} else {
		memcpy(&nand->bbm_config, &nand->default_bbm_config, sizeof(nand->bbm_config));
	}

	if (nand->bbm_config.flags & ECC_F_BBM_MERGE_PAGE || !nand->bbm_config.pages.num) {
		for (i = 0; i < nand->default_bbm_config.pages.num; i++) {
			ret = ufprog_nand_bbm_add_page(nand, &nand->bbm_config.pages,
						       nand->default_bbm_config.pages.idx[i]);
			if (ret) {
				if (ret != UFP_FAIL)
					return ret;

				logm_warn("Too many pages for checking bad marker\n");
				break;
			}
		}
	}

	if (!nand->bbm_config.pages.num) {
		/* Use default config */
		nand->bbm_config.pages.num = 1;
		nand->bbm_config.pages.idx[0] = 0;
	}

	if (!nand->bbm_config.mark.num) {
		memcpy(&nand->bbm_config.mark, &nand->default_bbm_config.mark, sizeof(nand->bbm_config.mark));

		if (!nand->bbm_config.mark.num) {
			/* Use default config */
			nand->bbm_config.mark.num = 1;
			nand->bbm_config.mark.pos[0] = nand->memorg.page_size;
		}

		if (!nand->bbm_config.mark.bytes) {
			nand->bbm_config.mark.bytes = nand->default_bbm_config.mark.bytes;

			if (!nand->bbm_config.mark.bytes)
				nand->bbm_config.mark.bytes = (nand->bus_width + 7) / 8;
		}
	}

	if (!nand->bbm_config.check.num) {
		memcpy(&nand->bbm_config.check, &nand->default_bbm_config.check, sizeof(nand->bbm_config.check));

		if (!nand->bbm_config.check.num) {
			/* Use default config */
			nand->bbm_config.check.num = 1;
			nand->bbm_config.check.pos[0] = nand->memorg.page_size;
		}

		if (!nand->bbm_config.check.width) {
			nand->bbm_config.check.width = nand->default_bbm_config.check.width;

			if (!nand->bbm_config.check.width)
				nand->bbm_config.check.width = nand->bbm_config.mark.bytes * 8;
		}
	}

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_nand_set_ecc(struct nand_chip *nand, struct ufprog_nand_ecc_chip *ecc)
{
	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (nand->ecc)
		STATUS_CHECK_RET(ufprog_ecc_set_enable(nand->ecc, false));

	nand->ecc = ecc;

	if (ecc)
		nand->ecc_steps = nand->memorg.page_size / ecc->config.step_size;
	else
		nand->ecc_steps = 0;

	return nand_setup_bbm_config(nand);
}

struct ufprog_nand_ecc_chip *UFPROG_API ufprog_nand_get_ecc(struct nand_chip *nand)
{
	if (!nand)
		return NULL;

	if (nand->ecc && nand->ecc->type != NAND_ECC_NONE)
		return nand->ecc;

	return NULL;
}

struct ufprog_nand_ecc_chip *UFPROG_API ufprog_nand_default_ecc(struct nand_chip *nand)
{
	if (!nand)
		return NULL;

	return nand->default_ecc;
}

ufprog_status UFPROG_API ufprog_nand_get_bbm_config(struct nand_chip *nand, struct nand_bbm_config *ret_bbmcfg)
{
	if (!nand || !ret_bbmcfg)
		return UFP_INVALID_PARAMETER;

	memcpy(ret_bbmcfg, &nand->bbm_config, sizeof(nand->bbm_config));

	return UFP_OK;
}

static int nand_bbm_mark_pos_cmp(void const *a, void const *b)
{
	const uint32_t *pa = a, *pb = b;

	return *pa - *pb;
}

ufprog_status UFPROG_API ufprog_nand_generate_page_layout(struct nand_chip *nand, struct nand_page_layout **out_layout)
{
	struct nand_page_layout *layout;
	uint32_t i, count, base;
	struct nand_bbm_mark_cfg mark;

	if (!nand || !out_layout)
		return UFP_INVALID_PARAMETER;

	memcpy(&mark, &nand->bbm_config.mark, sizeof(mark));
	qsort(mark.pos, mark.num, sizeof(mark.pos[0]), nand_bbm_mark_pos_cmp);

	for (i = 0, base = 0, count = 0; i < mark.num; i++) {
		if (base < mark.pos[i]) {
			count++;
			base = mark.pos[i];
		}

		if (base == mark.pos[i]) {
			count++;
			base += mark.bytes;
		}
	}

	if (base < nand->maux.oob_page_size)
		count++;

	layout = malloc(sizeof(*layout) + count * sizeof(layout->entries[0]));
	if (!layout)
		return UFP_NOMEM;

	layout->count = count;

	for (i = 0, base = 0, count = 0; i < mark.num; i++) {
		if (base < mark.pos[i]) {
			layout->entries[count].num = mark.pos[i] - base;
			layout->entries[count].type = NAND_PAGE_BYTE_DATA;
			count++;
			base = mark.pos[i];
		}

		if (base == mark.pos[i]) {
			layout->entries[count].num = mark.bytes;
			layout->entries[count].type = NAND_PAGE_BYTE_MARKER;
			count++;
			base += mark.bytes;
		}
	}

	if (base < nand->maux.oob_page_size) {
		layout->entries[count].num = nand->maux.oob_page_size - base;
		layout->entries[count].type = NAND_PAGE_BYTE_DATA;
	}

	*out_layout = layout;

	return UFP_OK;
}

void UFPROG_API ufprog_nand_free_page_layout(struct nand_page_layout *layout)
{
	if (layout)
		free(layout);
}

uint32_t UFPROG_API ufprog_nand_page_layout_to_map(const struct nand_page_layout *layout, uint8_t *map)
{
	uint32_t i, n = 0;

	if (!layout || !map)
		return 0;

	for (i = 0; i < layout->count; i++) {
		memset(map + n, layout->entries[i].type, layout->entries[i].num);
		n += layout->entries[i].num;
	}

	return n;
}

uint32_t UFPROG_API ufprog_nand_fill_page_by_layout(const struct nand_page_layout *layout, void *dst, const void *src,
						    uint32_t count, uint32_t flags)
{
	uint32_t i, cpycnt, n = 0;
	const uint8_t *s = src;
	uint8_t *p = dst;
	bool fill;

	if (!layout || !dst || !src)
		return 0;

	for (i = 0; i < layout->count; i++) {
		if (n < count) {
			cpycnt = count - n;
			if (cpycnt > layout->entries[i].num)
				cpycnt = layout->entries[i].num;
		} else {
			cpycnt = 0;
		}

		switch (layout->entries[i].type) {
		case NAND_PAGE_BYTE_DATA:
		case NAND_PAGE_BYTE_OOB_DATA:
		case NAND_PAGE_BYTE_OOB_FREE:
		case NAND_PAGE_BYTE_UNUSED:
		case NAND_PAGE_BYTE_ECC_PARITY:
			fill = false;

			if (layout->entries[i].type == NAND_PAGE_BYTE_DATA) {
				fill = true;
			} else if (layout->entries[i].type == NAND_PAGE_BYTE_OOB_DATA) {
				if (flags & PAGE_FILL_F_FILL_OOB)
					fill = true;
			} else if (layout->entries[i].type == NAND_PAGE_BYTE_OOB_FREE) {
				if (flags & PAGE_FILL_F_FILL_UNPROTECTED_OOB)
					fill = true;
			} else if (layout->entries[i].type == NAND_PAGE_BYTE_UNUSED) {
				if (flags & PAGE_FILL_F_FILL_UNUSED)
					fill = true;
			} else if (layout->entries[i].type == NAND_PAGE_BYTE_ECC_PARITY) {
				if (flags & PAGE_FILL_F_FILL_ECC_PARITY)
					fill = true;
			}

			if (fill) {
				if (cpycnt)
					memcpy(p, s, cpycnt);

				if (layout->entries[i].num > cpycnt)
					memset(p + cpycnt, 0xff, layout->entries[i].num - cpycnt);

				s += layout->entries[i].num;
				p += layout->entries[i].num;
				break;
			}

		default:
			if (flags & PAGE_FILL_F_FILL_NON_DATA_FF)
				memset(p, 0xff, layout->entries[i].num);

			if (flags & PAGE_FILL_F_SRC_SKIP_NON_DATA)
				s += layout->entries[i].num;

			p += layout->entries[i].num;
		}

		n += layout->entries[i].num;
	}

	return n;
}

static ufprog_status nand_torture_check_pattern(const uint8_t *buf, uint32_t count, uint8_t pat)
{
	uint32_t i;

	for (i = 0; i < count; i++) {
		if (buf[i] != pat)
			return UFP_FAIL;
	}

	return UFP_OK;
}

static ufprog_status nand_torture_test_pattern(struct nand_chip *nand, uint32_t block, uint8_t pat, uint8_t check,
					       bool erase)
{
	void *buf = nand->page_cache[0];
	ufprog_status ret;

	if (erase) {
		ret = ufprog_nand_erase_block(nand, block << nand->maux.pages_per_block_shift);
		if (ret) {
			logm_err("Failed to erase block %u at 0x%" PRIx64 "\n", block,
				 (uint64_t)block << nand->maux.block_shift);
			return ret;
		}

		ret = ufprog_nand_read_pages(nand, block << nand->maux.pages_per_block_shift,
					     nand->memorg.pages_per_block, buf, true, 0, NULL);
		if (ret) {
			logm_err("Failed to read block %u at 0x%" PRIx64 " after erase\n", block,
				 (uint64_t)block << nand->maux.block_shift);

			return ret;
		}

		ret = nand_torture_check_pattern(buf, nand->maux.oob_block_size, 0xff); {
			logm_err("Non-0xFF byte found in erased block %u at 0x%" PRIx64 "\n", block,
				 (uint64_t)block << nand->maux.block_shift);
			return ret;
		}
	}

	memset(buf, pat, nand->maux.oob_block_size);

	ret = ufprog_nand_write_pages(nand, block << nand->maux.pages_per_block_shift,
				      nand->memorg.pages_per_block, buf, true, false, NULL);
	if (ret) {
		logm_err("Failed to write test pattern to block %u at 0x%" PRIx64 "\n", block,
			 (uint64_t)block << nand->maux.block_shift);
		return ret;
	}

	memset(buf, ~pat, nand->maux.oob_block_size);

	ret = ufprog_nand_read_pages(nand, block << nand->maux.pages_per_block_shift,
				     nand->memorg.pages_per_block, buf, true, 0, NULL);
	if (ret) {
		logm_err("Failed to read block %u at 0x%" PRIx64 " after writting test pattern 0x%02x\n", block,
			 (uint64_t)block << nand->maux.block_shift, pat);
		return ret;
	}

	ret = nand_torture_check_pattern(buf, nand->maux.oob_block_size, pat);
	if (ret) {
		logm_err("Non-0x%02X byte found in erased block %u at 0x%" PRIx64 "\n", pat, block,
			 (uint64_t)block << nand->maux.block_shift);
		return ret;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_nand_torture_block(struct nand_chip *nand, uint32_t block)
{
	ufprog_status ret;

	if (!nand)
		return UFP_INVALID_PARAMETER;

	if (block >= nand->maux.block_count)
		return UFP_INVALID_PARAMETER;

	STATUS_CHECK_RET(nand_torture_test_pattern(nand, block, TORTURE_TEST_PAT, TORTURE_TEST_PAT, true));
	STATUS_CHECK_RET(nand_torture_test_pattern(nand, block, TORTURE_TEST_CMP_PAT, TORTURE_TEST_CMP_PAT, true));

	if (nand->random_page_write)
		STATUS_CHECK_RET(nand_torture_test_pattern(nand, block, TORTURE_TEST_PAT, 0, false));

	/* Test passed. Erase this block. */
	ret = ufprog_nand_erase_block(nand, block << nand->maux.pages_per_block_shift);
	if (ret) {
		logm_err("Failed to erase block %u at 0x%" PRIx64 "\n", block,
			 (uint64_t)block << nand->maux.block_shift);
	}

	return ret;
}
