// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NAND external flash table processing
 */

#define _GNU_SOURCE
#include <limits.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ufprog/log.h>
#include <ufprog/dirs.h>
#include <ufprog/config.h>
#include <ufprog/lookup_table.h>
#include "vendor.h"
#include "ext_id.h"

struct ext_parts_info {
	const struct spi_nand_vendor *builtin_vendor;
	struct spi_nand_vendor *vendor;
	struct spi_nand_flash_part *parts;
	uint32_t maxparts;
	uint32_t nparts;
	const char *path;
	ufprog_status ret;
};

struct part_io_opcodes_item {
	const char *name;
	const struct spi_nand_io_opcode *opcodes;
};

struct part_page_layouts_item {
	const char *name;
	const struct nand_page_layout *page_layouts;
};

struct part_memorgs_item {
	const char *name;
	const struct nand_memorg *memorg;
};

static const struct spi_nand_part_flag_enum_info part_flags[] = {
	{ 0, "meta" },
	{ 1, "no-pp" },
	{ 2, "generic-uid" },
	{ 3, "extended-ecc-bfr-max-8-bits" },
	{ 4, "read-cache-random" },
	{ 5, "read-cache-seq" },
	{ 6, "spi-nor-read-cap" },
	{ 7, "continuous-read" },
	{ 8, "bbm-check-2nd-page" },
	{ 9, "no-op" },
	{ 10, "random-page-write" },
};

static const struct spi_nand_part_flag_enum_info part_id_types[] = {
	{ SNAND_ID_DUMMY, "with-dummy-byte" },
	{ SNAND_ID_ADDR, "with-address-byte" },
	{ SNAND_ID_DIRECT, "direct" },
};

static const struct spi_nand_part_flag_enum_info part_qe_types[] = {
	{ QE_DONT_CARE, "dont-care" },
	{ QE_CR_BIT0, "cr-bit0" },
};

static const struct spi_nand_part_flag_enum_info part_ecc_types[] = {
	{ ECC_UNSUPPORTED, "unsupported" },
	{ ECC_CR_BIT4, "cr-bit4" },
};

static const struct spi_nand_part_flag_enum_info part_otp_ctrl_types[] = {
	{ OTP_UNSUPPORTED, "unsupported" },
	{ OTP_CR_BIT6, "cr-bit6" },
};

static const struct part_io_opcodes_item builtin_io_opcodes[] = {
	{ "default-read-4d", default_rd_opcodes_4d },
	{ "default-read-q2d", default_rd_opcodes_q2d },
	{ "default-pl", default_pl_opcodes },
};

static const struct part_memorgs_item builtin_memorgs[] = {
	{ "512m:2k+64", &snand_memorg_512m_2k_64 },
	{ "512m:2k+128", &snand_memorg_512m_2k_128 },
	{ "1g:2k+64", &snand_memorg_1g_2k_64 },
	{ "2g:2k+64", &snand_memorg_2g_2k_64 },
	{ "2g:2k+120", &snand_memorg_2g_2k_120 },
	{ "4g:2k+64", &snand_memorg_4g_2k_64 },
	{ "1g:2k+120", &snand_memorg_1g_2k_120 },
	{ "1g:2k+128", &snand_memorg_1g_2k_128 },
	{ "2g:2k+128", &snand_memorg_2g_2k_128 },
	{ "4g:2k+128", &snand_memorg_4g_2k_128 },
	{ "4g:4k+240", &snand_memorg_4g_4k_240 },
	{ "4g:4k+256", &snand_memorg_4g_4k_256 },
	{ "8g:2k+128", &snand_memorg_8g_2k_128 },
	{ "8g:4k+256", &snand_memorg_8g_4k_256 },
	{ "1g:2k+64/2p", &snand_memorg_1g_2k_64_2p },
	{ "2g:2k+64/2p", &snand_memorg_2g_2k_64_2p },
	{ "2x1g:2k+64", &snand_memorg_2g_2k_64_2d },
	{ "2g:2k+128/2p", &snand_memorg_2g_2k_128_2p },
	{ "4g:2k+64/2p", &snand_memorg_4g_2k_64_2p },
	{ "2x2g:2k+128/2p", &snand_memorg_4g_2k_128_2p_2d },
	{ "2x4g:4k+256", &snand_memorg_8g_4k_256_2d },
	{ "4x2g:2k+128/2p", &snand_memorg_8g_2k_128_2p_4d },
};

static const struct spi_nand_part_flag_enum_info page_layout_entry_types[] = {
	{ NAND_PAGE_BYTE_UNUSED, "unused" },
	{ NAND_PAGE_BYTE_DATA, "data" },
	{ NAND_PAGE_BYTE_OOB_DATA, "oob" },
	{ NAND_PAGE_BYTE_OOB_FREE, "oob-raw" },
	{ NAND_PAGE_BYTE_ECC_PARITY, "ecc-parity-code" },
	{ NAND_PAGE_BYTE_MARKER, "bad-block-marker" },
};

static struct ufprog_lookup_table *ext_io_opcodes_list;
static struct ufprog_lookup_table *ext_page_layout_list;
static struct ufprog_lookup_table *ext_memorg_list;

static ufprog_status spi_nand_parse_ext_io_opcodes(struct json_object *jopcode, struct spi_nand_io_opcode *ret_opcodes,
						   uint32_t *out_iocaps, const char *path);

static ufprog_status spi_nand_parse_ext_page_layout(struct json_object *jpglyt, struct nand_page_layout **out_pglyt,
						    const char *path);

static ufprog_status spi_nand_parse_ext_memorg(struct json_object *jmemorg, struct nand_memorg *ret_memorg,
					       const char *path);

static const struct spi_nand_io_opcode *spi_nand_io_opcodes_find(const char *name)
{
	const struct spi_nand_io_opcode *opcodes;
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(builtin_io_opcodes); i++) {
		if (!strcmp(builtin_io_opcodes[i].name, name))
			return builtin_io_opcodes[i].opcodes;
	}

	if (lookup_table_find(ext_io_opcodes_list, name, (void **)&opcodes))
		return opcodes;

	return NULL;
}

static const struct nand_page_layout *spi_nand_page_layout_find(const char *name)
{
	const struct nand_page_layout *pglyt;

	if (lookup_table_find(ext_page_layout_list, name, (void **)&pglyt))
		return pglyt;

	return NULL;
}

static const struct nand_memorg *spi_nand_memorg_find(const char *name)
{
	const struct nand_memorg *memorg;
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(builtin_memorgs); i++) {
		if (!strcmp(builtin_memorgs[i].name, name))
			return builtin_memorgs[i].memorg;
	}

	if (lookup_table_find(ext_memorg_list, name, (void **)&memorg))
		return memorg;

	return NULL;
}

static ufprog_status spi_nand_ext_part_read_id(struct json_object *jpart, struct spi_nand_id *id, const char *path)
{
	struct json_object *jid;
	ufprog_status ret;
	uint32_t i, val;

	ret = json_read_array(jpart, "id", &jid);
	if (ret) {
		if (ret == UFP_NOT_EXIST)
			logm_err("%s/%s not defined\n", path, "id");
		else
			logm_err("Invalid type of %s/%s\n", path, "id");

		return ret;
	}

	id->val.len = (uint32_t)json_array_len(jid);
	if (id->val.len > NAND_ID_MAX_LEN) {
		logm_err("Length of %s/%s is bigger than %u\n", path, "id", NAND_ID_MAX_LEN);
		return UFP_JSON_DATA_INVALID;
	}

	for (i = 0; i < id->val.len; i++) {
		ret = json_array_read_hex32(jid, i, &val, 0);
		if (ret) {
			logm_err("Invalid value of %s/%s/%u\n", path, "id", i);
			return ret;
		}

		if (val > 0xff) {
			logm_err("The value of %s/%s/%u is bigger than FFh\n", path, "id", i);
			return UFP_JSON_DATA_INVALID;
		}

		id->val.id[i] = (uint8_t)val;
	}

	return UFP_OK;
}

static bool spi_nand_ext_part_get_flag_enum_val_from_name(const struct spi_nand_part_flag_enum_info *info,
							  uint32_t count, const char *name, uint32_t *retval)
{
	uint32_t i;

	for (i = 0; i < count; i++) {
		if (!strcmp(info[i].name, name)) {
			*retval = info[i].val;
			return true;
		}
	}

	return false;
}

static ufprog_status spi_nand_ext_part_read_bit_flags(struct json_object *jflags,
						      const struct spi_nand_part_flag_enum_info *info, uint32_t count,
						      uint32_t *retflags, const char *path, const char *field)
{
	const char *flag_name;
	ufprog_status ret;
	uint32_t bit;
	size_t i, n;

	n = json_array_len(jflags);

	for (i = 0; i < n; i++) {
		ret = json_array_read_str(jflags, i, &flag_name, NULL);
		if (ret) {
			logm_dbg("Invalid type of %s/%s/%zu\n", path, field, i);
			return ret;
		}

		if (!spi_nand_ext_part_get_flag_enum_val_from_name(info, count, flag_name, &bit)) {
			logm_err("%s/%s/%zu is not a valid flag name\n", path, field, i);
			return UFP_NOT_EXIST;
		}

		*retflags |= BIT(bit);
	}

	return UFP_OK;
}

static ufprog_status spi_nand_ext_part_read_flags(struct json_object *jobj, const char *field,
						  const struct spi_nand_part_flag_enum_info *info, uint32_t count,
						  uint32_t *retflags, const char *path)
{
	struct json_object *jflags;
	ufprog_status ret;

	*retflags = 0;

	if (!info || !count)
		return UFP_OK;

	ret = json_read_array(jobj, field, &jflags);
	if (ret) {
		if (ret == UFP_NOT_EXIST)
			return UFP_OK;

		logm_err("Invalid type of %s/%s\n", path, field);
		return ret;
	}

	return spi_nand_ext_part_read_bit_flags(jflags, info, count, retflags, path, field);
}

static ufprog_status spi_nand_ext_part_read_enum(struct json_object *jobj, const char *field,
						 const struct spi_nand_part_flag_enum_info *info, uint32_t count,
						 uint32_t *retval, const char *path)
{
	const char *enum_name;
	ufprog_status ret;
	uint32_t i;

	*retval = 0;

	if (!info || !count)
		return UFP_OK;

	ret = json_read_str(jobj, field, &enum_name, NULL);
	if (ret) {
		if (ret == UFP_NOT_EXIST)
			return UFP_OK;

		logm_err("Invalid type of %s/%s\n", path, field);
		return ret;
	}

	for (i = 0; i < count; i++) {
		if (!strcmp(info[i].name, enum_name)) {
			*retval = info[i].val;
			return UFP_OK;
		}
	}

	logm_err("'%s' is not a valid name for %s/%s\n", enum_name, path, field);

	return UFP_NOT_EXIST;
}

static ufprog_status spi_nand_ext_part_read_u32(struct json_object *jobj, const char *field, uint32_t *retval,
						uint32_t dflval, const char *path)
{
	ufprog_status ret;

	ret = json_read_uint32(jobj, field, retval, dflval);
	if (ret == UFP_JSON_TYPE_INVALID) {
		logm_err("Invalid type of %s/%s\n", path, field);
		return ret;
	}

	return UFP_OK;
}

static ufprog_status spi_nand_ext_part_read_io_caps(struct json_object *jpart, const char *field, uint32_t *retval,
						    const char *path)
{
	struct json_object *jiocaps;
	const char *io_name;
	ufprog_status ret;
	uint32_t io_type;
	size_t i, n;

	*retval = BIT_SPI_MEM_IO_1_1_1;

	ret = json_read_array(jpart, field, &jiocaps);
	if (ret) {
		if (ret == UFP_NOT_EXIST)
			return UFP_OK;

		logm_err("Invalid type of %s/%s\n", path, field);
		return ret;
	}

	n = json_array_len(jiocaps);

	for (i = 0; i < n; i++) {
		ret = json_array_read_str(jiocaps, i, &io_name, NULL);
		if (ret == UFP_JSON_TYPE_INVALID) {
			logm_err("Invalid type of %s/%s/%zu\n", path, field, i);
			return ret;
		}

		io_type = ufprog_spi_mem_io_name_to_type(io_name);
		if (io_type >= __SPI_MEM_IO_MAX) {
			logm_err("'%s' is not a valid I/O type name of %s/%s/%zu\n", io_name, path, field, i);
			return UFP_JSON_DATA_INVALID;
		}

		*retval |= BIT(io_type);
	}

	return UFP_OK;
}

static ufprog_status spi_nand_ext_part_read_io_opcodes(struct json_object *jpart, const char *field,
						       const struct spi_nand_io_opcode **out_opcodes,
						       uint32_t *out_iocaps, bool *ret_needs_free, const char *path)
{
	struct spi_nand_io_opcode *tmp_opcodes;
	const struct spi_nand_io_opcode *copcodes;
	struct json_object *jopcode;
	const char *opcode_name;
	ufprog_status ret;
	char *npath;

	if (!json_node_exists(jpart, field)) {
		*(out_opcodes) = NULL;
		*ret_needs_free = false;
		return UFP_OK;
	}

	if (asprintf(&npath, "%s/%s", path, field) < 0) {
		logm_err("No memory for JSON pointer string\n");
		return UFP_NOMEM;
	}

	if (json_is_str(jpart, field)) {
		json_read_str(jpart, field, &opcode_name, NULL);

		copcodes = spi_nand_io_opcodes_find(opcode_name);
		if (!copcodes) {
			logm_err("I/O opcode group named '%s' not found for %s\n", opcode_name, npath);
			ret = UFP_NOT_EXIST;
			goto cleanup;
		}

		*out_iocaps = 0;
		*(out_opcodes) = copcodes;
		*ret_needs_free = false;

		ret = UFP_OK;
	} else if (json_is_obj(jpart, field)) {
		json_read_obj(jpart, field, &jopcode);

		tmp_opcodes = malloc(sizeof(struct spi_nand_io_opcode) * __SPI_MEM_IO_MAX);
		if (!tmp_opcodes) {
			logm_err("No memory for I/O opcode group %s\n", npath);
			ret = UFP_NOMEM;
			goto cleanup;
		}

		ret = spi_nand_parse_ext_io_opcodes(jopcode, tmp_opcodes, out_iocaps, npath);
		if (ret) {
			logm_err("Failed to parse I/O opcode group %s\n", npath);
			ret = UFP_JSON_DATA_INVALID;
			free(tmp_opcodes);
			goto cleanup;
		}

		*(out_opcodes) = tmp_opcodes;
		*ret_needs_free = true;
	} else {
		logm_err("Invalid type of %s\n", npath);
		ret = UFP_JSON_TYPE_INVALID;
	}

cleanup:
	free(npath);

	return ret;
}

static ufprog_status spi_nand_ext_part_read_page_layout(struct json_object *jpart,
							const struct nand_page_layout **out_pglyt, bool *ret_needs_free,
							const char *path)
{
	const struct nand_page_layout *cpglyt;
	struct nand_page_layout *tmp_pglyt;
	struct json_object *jpglyt;
	const char *pglyt_name;
	ufprog_status ret;
	char *npath;

	if (!json_node_exists(jpart, "page-layout")) {
		*(out_pglyt) = NULL;
		*ret_needs_free = false;
		return UFP_OK;
	}

	if (asprintf(&npath, "%s/%s", path, "page-layout") < 0) {
		logm_err("No memory for JSON pointer string\n");
		return UFP_NOMEM;
	}

	if (json_is_str(jpart, "page-layout")) {
		json_read_str(jpart, "page-layout", &pglyt_name, NULL);

		cpglyt = spi_nand_page_layout_find(pglyt_name);
		if (!cpglyt) {
			logm_err("Page layout named '%s' not found for %s\n", pglyt_name, npath);
			ret = UFP_NOT_EXIST;
			goto cleanup;
		}

		*(out_pglyt) = cpglyt;
		*ret_needs_free = false;

		ret = UFP_OK;
	} else if (json_is_array(jpart, "page-layout")) {
		json_read_array(jpart, "page-layout", &jpglyt);

		ret = spi_nand_parse_ext_page_layout(jpglyt, &tmp_pglyt, npath);
		if (ret) {
			logm_err("Failed to parse page layout %s\n", npath);
			ret = UFP_JSON_DATA_INVALID;
			goto cleanup;
		}

		*(out_pglyt) = tmp_pglyt;
		*ret_needs_free = true;
	} else {
		logm_err("Invalid type of %s\n", npath);
		ret = UFP_JSON_TYPE_INVALID;
	}

cleanup:
	free(npath);

	return ret;
}

static ufprog_status spi_nand_ext_part_read_memorg(struct json_object *jpart,
						   const struct nand_memorg **out_memorg, bool *ret_needs_free,
						   const char *path)
{
	const struct nand_memorg *cmemorg;
	struct nand_memorg *tmp_memorg;
	struct json_object *jmemorg;
	const char *memorg_name;
	ufprog_status ret;
	char *npath;

	if (!json_node_exists(jpart, "memory-organization")) {
		*(out_memorg) = NULL;
		*ret_needs_free = false;
		return UFP_OK;
	}

	if (asprintf(&npath, "%s/%s", path, "memory-organization") < 0) {
		logm_err("No memory for JSON pointer string\n");
		return UFP_NOMEM;
	}

	if (json_is_str(jpart, "memory-organization")) {
		json_read_str(jpart, "memory-organization", &memorg_name, NULL);

		cmemorg = spi_nand_memorg_find(memorg_name);
		if (!cmemorg) {
			logm_err("Memory organization named '%s' not found for %s\n", memorg_name, npath);
			ret = UFP_NOT_EXIST;
			goto cleanup;
		}

		*(out_memorg) = cmemorg;
		*ret_needs_free = false;

		ret = UFP_OK;
	} else if (json_is_obj(jpart, "memory-organization")) {
		json_read_obj(jpart, "memory-organization", &jmemorg);

		tmp_memorg = malloc(sizeof(*tmp_memorg));
		if (!tmp_memorg) {
			logm_err("No memory for memory organization %s\n", npath);
			ret = UFP_NOMEM;
			goto cleanup;
		}

		ret = spi_nand_parse_ext_memorg(jmemorg, tmp_memorg, npath);
		if (ret) {
			logm_err("Failed to parse memory organization %s\n", npath);
			ret = UFP_JSON_DATA_INVALID;
			free(tmp_memorg);
			goto cleanup;
		}

		*(out_memorg) = tmp_memorg;
		*ret_needs_free = true;
	} else {
		logm_err("Invalid type of %s\n", npath);
		ret = UFP_JSON_TYPE_INVALID;
	}

cleanup:
	free(npath);

	return ret;
}

static ufprog_status spi_nand_ext_part_read_ecc_req(struct json_object *jpart, struct nand_ecc_config *ret_eccreq,
						    const char *path)
{
	struct json_object *jeccreq;
	ufprog_status ret;
	uint32_t val;
	char *npath;

	if (!json_node_exists(jpart, "ecc-requirement")) {
		ret_eccreq->step_size = 0;
		ret_eccreq->strength_per_step = 0;
		return UFP_OK;
	}

	if (!json_is_obj(jpart, "ecc-requirement")) {
		logm_err("Invalid type of %s/%s\n", path, "ecc-requirement");
		return UFP_JSON_TYPE_INVALID;
	}

	json_read_obj(jpart, "ecc-requirement", &jeccreq);

	if (asprintf(&npath, "%s/%s", path, "ecc-requirement") < 0) {
		logm_err("No memory for JSON pointer string\n");
		return UFP_NOMEM;
	}

	ret = spi_nand_ext_part_read_u32(jeccreq, "step-size", &val, 0, npath);
	if (ret)
		goto cleanup;

	if (val > UINT16_MAX) {
		logm_err("Step size is too big in %s\n", npath);
		ret = UFP_JSON_DATA_INVALID;
		goto cleanup;
	}

	if (!val) {
		logm_err("Step size is zero in %s\n", npath);
		ret = UFP_JSON_DATA_INVALID;
		goto cleanup;
	}

	ret_eccreq->step_size = (uint16_t)val;

	ret = spi_nand_ext_part_read_u32(jeccreq, "strength-per-step", &val, 0, npath);
	if (ret)
		goto cleanup;

	if (val > UINT16_MAX) {
		logm_err("Strength per step is too big in %s\n", npath);
		ret = UFP_JSON_DATA_INVALID;
		goto cleanup;
	}

	if (!val) {
		logm_err("Strength per step is zero in %s\n", npath);
		ret = UFP_JSON_DATA_INVALID;
		goto cleanup;
	}

	ret_eccreq->strength_per_step = (uint16_t)val;

cleanup:
	free(npath);

	return ret;
}

static ufprog_status spi_nand_ext_part_read_otp(struct json_object *jpart, const struct nand_otp_info **outotp,
						const char *path)
{
	struct nand_otp_info *tmpotp;
	uint32_t start_index, count;
	struct json_object *jotp;
	ufprog_status ret;
	char *npath;

	if (!json_node_exists(jpart, "otp")) {
		*(outotp) = NULL;
		return UFP_OK;
	}

	if (!json_is_obj(jpart, "otp")) {
		logm_err("Invalid type of %s/%s\n", path, "otp");
		return UFP_JSON_TYPE_INVALID;
	}

	json_read_obj(jpart, "otp", &jotp);

	if (asprintf(&npath, "%s/%s", path, "otp") < 0) {
		logm_err("No memory for JSON pointer string\n");
		return UFP_NOMEM;
	}

	ret = spi_nand_ext_part_read_u32(jotp, "start-index", &start_index, 0, npath);
	if (ret)
		goto cleanup;

	ret = spi_nand_ext_part_read_u32(jotp, "count", &count, 0, npath);
	if (ret)
		goto cleanup;

	if (!count) {
		logm_err("OTP region count is zero in %s\n", npath);
		ret = UFP_JSON_DATA_INVALID;
		goto cleanup;
	}

	tmpotp = malloc(sizeof(*tmpotp));
	if (!tmpotp) {
		logm_err("No memory OTP region %s\n", npath);
		ret = UFP_NOMEM;
		goto cleanup;
	}

	tmpotp->start_index = start_index;
	tmpotp->count = count;

	*(outotp) = tmpotp;

	ret = UFP_OK;

cleanup:
	free(npath);

	return ret;
}

static ufprog_status spi_nand_ext_part_read_alias(struct json_object *jpart,
						  struct spi_nand_flash_part_alias **outalias, const char *path)
{
	struct spi_nand_flash_part_alias *alias;
	struct json_object *jalias, *jitem;
	const char *vendor, *model;
	ufprog_status ret;
	size_t i, n;

	ret = json_read_array(jpart, "alias", &jalias);
	if (ret) {
		if (ret == UFP_NOT_EXIST) {
			*outalias = NULL;
			return UFP_OK;
		}

		logm_err("Invalid type of %s/%s\n", path, "alias");
		return ret;
	}

	n = json_array_len(jalias);

	alias = calloc(1, sizeof(*alias) + n * sizeof(struct spi_nand_flash_part_alias_item));
	if (!alias) {
		logm_err("No memory for flash part alias\n");
		return UFP_NOMEM;
	}

	alias->num = (uint32_t)n;

	for (i = 0; i < n; i++) {
		ret = json_array_read_str(jalias, i, &model, NULL);
		if (!ret)
			goto done;

		ret = json_array_read_obj(jalias, i, &jitem);
		if (ret) {
			logm_err("Invalid type of %s/%s/%zu\n", path, "alias", i);
			return ret;
		}

		ret = json_read_str(jitem, "vendor", &vendor, NULL);
		if (ret) {
			if (ret != UFP_NOT_EXIST) {
				logm_err("Invalid type of %s/%s/%zu/%s\n", path, "alias", i, "vendor");
				return ret;
			}
		} else {
			alias->items[i].vendor = spi_nand_find_vendor_by_id(vendor);
			if (!alias->items[i].vendor) {
				logm_err("Vendor named '%s' does not exist\n", vendor);
				return UFP_JSON_DATA_INVALID;
			}
		}

		ret = json_read_str(jitem, "model", &model, NULL);
		if (ret) {
			if (ret == UFP_NOT_EXIST)
				logm_err("Alias model name from %s/%s/%zu must not be empty\n", path, "alias", i);
			else
				logm_err("Invalid type of %s/%s/%zu/%s\n", path, "alias", i, "model");

			return UFP_JSON_DATA_INVALID;
		}

	done:
		alias->items[i].model = os_strdup(model);
		if (!alias->items[i].model) {
			logm_err("No memory for flash part alias\n");
			return UFP_NOMEM;
		}
	}

	*outalias = alias;

	return UFP_OK;
}

static void spi_nand_reset_ext_part(struct spi_nand_flash_part *part)
{
	uint32_t i;

	if (part->model)
		free((void *)part->model);

	if (part->rd_opcodes && (part->ext_id_flags & SPI_NAND_EXT_PART_FREE_RD_OPCODES))
		free((void *)part->rd_opcodes);

	if (part->pl_opcodes && (part->ext_id_flags & SPI_NAND_EXT_PART_FREE_PL_OPCODES))
		free((void *)part->pl_opcodes);

	if (part->page_layout && (part->ext_id_flags & SPI_NAND_EXT_PART_FREE_PAGE_LAYOUT))
		free((void *)part->page_layout);

	if (part->memorg && (part->ext_id_flags & SPI_NAND_EXT_PART_FREE_MEMORG))
		free((void *)part->memorg);

	if (part->otp)
		free((void *)part->otp);

	if (part->alias) {
		for (i = 0; i < part->alias->num; i++) {
			if (part->alias->items[i].model)
				free((void *)part->alias->items[i].model);
		}

		free((void *)part->alias);
	}

	memset(part, 0, sizeof(*part));
}

static int UFPROG_API spi_nand_ext_vendor_parts_cb(void *priv, const char *key, struct json_object *jpart)
{
	struct ext_parts_info *pi = priv;
	struct spi_nand_flash_part *part = &pi->parts[pi->nparts];
	const struct nand_page_layout *page_layout;
	const struct spi_nand_flash_part *chkpart;
	const struct spi_nand_io_opcode *opcodes;
	struct spi_nand_flash_part_alias *alias;
	const struct nand_memorg *memorg;
	const struct nand_otp_info *otp;
	uint32_t val, io_caps;
	bool needs_free;
	char *path;

	if (!*key) {
		logm_err("Part name must not be empty\n");
		pi->ret = UFP_FAIL;
		return 1;
	}

	if (pi->builtin_vendor) {
		chkpart = spi_nand_vendor_find_part_by_name(key, pi->builtin_vendor);
		if (chkpart) {
			logm_err("Part '%s' already exists in built-in part list\n", key);
			pi->ret = UFP_ALREADY_EXIST;
			return 1;
		}
	}

	chkpart = spi_nand_vendor_find_part_by_name(key, pi->vendor);
	if (chkpart) {
		logm_err("Part '%s' already exists in part list\n", key);
		pi->ret = UFP_ALREADY_EXIST;
		return 1;
	}

	if (asprintf(&path, "%s/%s/%s", pi->path, "parts", key) < 0) {
		logm_err("No memory for JSON pointer string\n");
		pi->ret = UFP_NOMEM;
		return 1;
	}

	part->model = os_strdup(key);
	if (!part->model) {
		logm_err("No memory for name of new part '%s'\n", key);
		pi->ret = UFP_NOMEM;
		goto cleanup;
	}

	pi->ret = spi_nand_ext_part_read_id(jpart, &part->id, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nand_ext_part_read_flags(jpart, "flags", part_flags, ARRAY_SIZE(part_flags), &part->flags, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nand_ext_part_read_flags(jpart, "vendor-flags", pi->vendor->vendor_flag_names,
					      pi->vendor->num_vendor_flag_names, &part->vendor_flags, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nand_ext_part_read_enum(jpart, "id-type", part_id_types, ARRAY_SIZE(part_id_types), &val, path);
	if (pi->ret)
		goto cleanup;
	part->id.type = val;

	pi->ret = spi_nand_ext_part_read_enum(jpart, "qe-type", part_qe_types, ARRAY_SIZE(part_qe_types), &val, path);
	if (pi->ret)
		goto cleanup;
	part->qe_type = val;

	pi->ret = spi_nand_ext_part_read_enum(jpart, "ecc-en-type", part_ecc_types, ARRAY_SIZE(part_ecc_types), &val,
					      path);
	if (pi->ret)
		goto cleanup;
	part->ecc_type = val;

	pi->ret = spi_nand_ext_part_read_enum(jpart, "otp-ctrl-type", part_otp_ctrl_types,
					      ARRAY_SIZE(part_otp_ctrl_types), &val, path);
	if (pi->ret)
		goto cleanup;
	part->otp_en_type = val;

	pi->ret = spi_nand_ext_part_read_u32(jpart, "number-of-programs", &part->nops, 0, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nand_ext_part_read_u32(jpart, "max-speed-spi-mhz", &part->max_speed_spi_mhz, 0, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nand_ext_part_read_u32(jpart, "max-speed-dual-mhz", &part->max_speed_dual_mhz, 0, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nand_ext_part_read_u32(jpart, "max-speed-quad-mhz", &part->max_speed_quad_mhz, 0, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nand_ext_part_read_io_caps(jpart, "read-io-caps", &part->rd_io_caps, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nand_ext_part_read_io_caps(jpart, "pl-io-caps", &part->pl_io_caps, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nand_ext_part_read_io_opcodes(jpart, "read-opcodes", &opcodes, &io_caps, &needs_free, path);
	if (pi->ret)
		goto cleanup;
	if (needs_free)
		part->ext_id_flags |= SPI_NAND_EXT_PART_FREE_RD_OPCODES;
	part->rd_opcodes = opcodes;
	part->rd_io_caps |= io_caps;

	pi->ret = spi_nand_ext_part_read_io_opcodes(jpart, "pl-opcodes", &opcodes, &io_caps, &needs_free, path);
	if (pi->ret)
		goto cleanup;
	if (needs_free)
		part->ext_id_flags |= SPI_NAND_EXT_PART_FREE_PL_OPCODES;
	part->pl_opcodes = opcodes;
	part->pl_io_caps |= io_caps;

	pi->ret = spi_nand_ext_part_read_ecc_req(jpart, &part->ecc_req, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nand_ext_part_read_page_layout(jpart, &page_layout, &needs_free, path);
	if (pi->ret)
		goto cleanup;
	if (needs_free)
		part->ext_id_flags |= SPI_NAND_EXT_PART_FREE_PAGE_LAYOUT;
	part->page_layout = page_layout;

	pi->ret = spi_nand_ext_part_read_memorg(jpart, &memorg, &needs_free, path);
	if (pi->ret)
		goto cleanup;
	if (needs_free)
		part->ext_id_flags |= SPI_NAND_EXT_PART_FREE_MEMORG;
	part->memorg = memorg;

	pi->ret = spi_nand_ext_part_read_otp(jpart, &otp, path);
	if (pi->ret)
		goto cleanup;
	part->otp = otp;

	pi->ret = spi_nand_ext_part_read_alias(jpart, &alias, path);
	if (pi->ret)
		goto cleanup;
	part->alias = alias;

	pi->nparts++;
	pi->ret = UFP_OK;

	free(path);

	return 0;

cleanup:
	spi_nand_reset_ext_part(part);

	free(path);

	return 1;
}

static ufprog_status spi_nand_load_flash_parts(struct json_object *jparts, struct ext_parts_info *pi)
{
	ufprog_status ret;

	ret = json_obj_foreach(jparts, NULL, spi_nand_ext_vendor_parts_cb, pi, NULL);
	if (ret) {
		if (ret == UFP_NOT_EXIST)
			return UFP_OK;

		return ret;
	}

	if (!pi->ret)
		return UFP_OK;

	return UFP_FAIL;
}

static int UFPROG_API spi_nand_ext_vendors_cb(void *priv, const char *key, struct json_object *jobj)
{
	const struct spi_nand_vendor *vendor;
	struct spi_nand_vendor *new_vendor;
	ufprog_status ret, *cbret = priv;
	struct json_object *jparts;
	struct ext_parts_info pi;
	const char *name;
	uint32_t mfr_id;
	char *path;

	if (!*key) {
		logm_err("Vendor ID must not be empty\n");
		*cbret = UFP_FAIL;
		return 1;
	}

	vendor = spi_nand_find_vendor_by_id(key);
	if (vendor) {
		if (spi_nand_is_ext_vendor(vendor)) {
			logm_err("Vendor ID '%s' already exists\n", key);
			*cbret = UFP_ALREADY_EXIST;
			return 1;
		}
	}

	new_vendor = spi_nand_alloc_ext_vendor();
	if (!new_vendor) {
		logm_err("No memory for new vendor '%s'\n", key);
		*cbret = UFP_NOMEM;
		return 1;
	}

	if (asprintf(&path, "/%s/%s", "vendors", key) < 0) {
		logm_err("No memory for JSON pointer string\n");
		*cbret = UFP_NOMEM;
		return 1;
	}

	new_vendor->id = os_strdup(key);
	if (!new_vendor->id) {
		logm_err("No memory for ID of new vendor '%s'\n", key);
		*cbret = UFP_NOMEM;
		goto cleanup;
	}

	if (vendor) {
		new_vendor->name = os_strdup(vendor->name);
		new_vendor->mfr_id = vendor->mfr_id;
		new_vendor->default_part_otp_ops = vendor->default_part_otp_ops;
		new_vendor->default_part_fixups = vendor->default_part_fixups;
		new_vendor->default_part_ops = vendor->default_part_ops;
		new_vendor->ops = vendor->ops;
	} else {
		ret = json_read_hex32(jobj, "mfr-id", &mfr_id, 0);
		if (ret) {
			if (ret == UFP_JSON_TYPE_INVALID)
				logm_err("Invalid type of %s/%s\n", path, "mfr-id");
			else
				logm_err("Invalid data of %s/%s\n", path, "mfr-id");

			*cbret = UFP_FAIL;
			goto cleanup;
		}

		if (!mfr_id || mfr_id > 0xff) {
			logm_err("Invalid value of %s/%s\n", path, "mfr-id");
			*cbret = UFP_FAIL;
			goto cleanup;
		}

		ret = json_read_str(jobj, "name", &name, NULL);
		if (ret == UFP_JSON_TYPE_INVALID) {
			logm_err("Invalid type of %s/%s\n", path, "name");
			*cbret = UFP_FAIL;
			goto cleanup;
		}

		if (!name || !*name)
			name = key;

		new_vendor->name = os_strdup(name);
		new_vendor->mfr_id = (uint8_t)mfr_id;
	}

	if (!new_vendor->name) {
		logm_err("No memory for name of new vendor '%s'\n", key);
		*cbret = UFP_NOMEM;
		goto cleanup;
	}

	if (!vendor)
		logm_dbg("Added new external vendor '%s' (%s)\n", key, new_vendor->name);
	else
		logm_dbg("Copied built-in vendor '%s' (%s)\n", key, new_vendor->name);

	ret = json_read_obj(jobj, "parts", &jparts);
	if (ret) {
		if (ret == UFP_NOT_EXIST) {
			logm_dbg("No parts defined for vendor '%s'\n", key);
			goto out;
		}

		logm_err("Invalid type of %s/%s\n", path, "parts");
		*cbret = ret;
		goto cleanup;
	}

	pi.path = path;
	pi.vendor = new_vendor;
	pi.builtin_vendor = vendor;
	pi.maxparts = (uint32_t)json_obj_len(jparts);
	pi.nparts = 0;
	pi.parts = calloc(pi.maxparts, sizeof(*pi.parts));
	if (!pi.parts) {
		logm_err("No memory for parts of new vendor '%s'\n", key);
		*cbret = UFP_NOMEM;
		goto cleanup;
	}

	ret = spi_nand_load_flash_parts(jparts, &pi);
	if (ret) {
		*cbret = ret;
		goto cleanup;
	}

	new_vendor->parts = pi.parts;
	new_vendor->nparts = pi.nparts;

out:
	free(path);

	*cbret = UFP_OK;
	return 0;

cleanup:
	free(path);

	return 1;
}

static ufprog_status spi_nand_load_ext_vendors(struct json_object *jroot)
{
	struct json_object *jvendors;
	ufprog_status ret, cbret;
	uint32_t count;

	ret = json_read_obj(jroot, "vendors", &jvendors);
	if (ret) {
		if (ret == UFP_NOT_EXIST)
			return UFP_OK;

		logm_err("Invalid type of vendors\n");
		return ret;
	}

	count = (uint32_t)json_obj_len(jvendors);
	if (!count) {
		logm_dbg("Empty vendor list\n");
		return UFP_OK;
	}

	if (!spi_nand_set_ext_vendor_capacity(count)) {
		logm_err("No memory for external vendor list\n");
		return UFP_NOMEM;
	}

	ret = json_obj_foreach(jvendors, NULL, spi_nand_ext_vendors_cb, &cbret, NULL);
	if (ret) {
		logm_err("Invalid type of /%s\n", "vendors");
		return ret;
	}

	if (!cbret)
		return UFP_OK;

	return UFP_FAIL;
}

static void spi_nand_reset_ext_vendor(struct spi_nand_vendor *vendor)
{
	uint32_t i;

	for (i = 0; i < vendor->nparts; i++)
		spi_nand_reset_ext_part((struct spi_nand_flash_part *)&vendor->parts[i]);

	free((void *)vendor->parts);
}

static ufprog_status spi_nand_parse_ext_io_opcodes(struct json_object *jopcode, struct spi_nand_io_opcode *ret_opcodes,
						   uint32_t *out_iocaps, const char *path)
{
	uint32_t opcode, ndummy, naddrs, io_type, io_caps = 0;
	struct json_object *jitem;
	ufprog_status ret;
	const char *name;

	memset(ret_opcodes, 0, sizeof(*ret_opcodes) * __SPI_MEM_IO_MAX);

	for (io_type = 0; io_type < __SPI_MEM_IO_MAX; io_type++) {
		name = ufprog_spi_mem_io_name(io_type);

		if (!json_node_exists(jopcode, name))
			continue;

		if (!json_is_obj(jopcode, name)) {
			logm_err("%s/%s is not an object\n", path, name);
			return UFP_JSON_TYPE_INVALID;
		}

		json_read_obj(jopcode, name, &jitem);

		if (!json_node_exists(jitem, "opcode")) {
			logm_err("%s/%s/%s not defined\n", path, name, "opcode");
			return UFP_NOT_EXIST;
		}

		ret = json_read_hex32(jitem, "opcode", &opcode, 0);
		if (ret) {
			logm_err("Invalid type of %s/%s/%s\n", path, name, "opcode");
			return ret;
		}

		if (!opcode || opcode > 0xff) {
			logm_err("%s/%s/%s is invalid\n", path, name, "opcode");
			return UFP_JSON_DATA_INVALID;
		}

		ret_opcodes[io_type].opcode = (uint8_t)opcode;

		ret = json_read_uint32(jitem, "dummy-cycles", &ndummy, 0);
		if (ret) {
			logm_err("Invalid type of %s/%s/%s\n", path, name, "dummy-cycles");
			return ret;
		}

		ret = json_read_uint32(jitem, "address-bytes", &naddrs, 0);
		if (ret) {
			logm_err("Invalid type of %s/%s/%s\n", path, name, "address-bytes");
			return ret;
		}

		ret_opcodes[io_type].ndummy = (uint8_t)ndummy;
		ret_opcodes[io_type].naddrs = (uint8_t)naddrs;

		io_caps |= BIT(io_type);
	}

	if (out_iocaps)
		*out_iocaps = io_caps;

	return UFP_OK;
}

static int UFPROG_API spi_nand_ext_io_opcodes_cb(void *priv, const char *key, struct json_object *jopcode)
{
	struct spi_nand_io_opcode *opcodes = NULL, tmp_opcodes[__SPI_MEM_IO_MAX];
	ufprog_status ret, *cbret = priv;
	char *path, *nkey;
	size_t keylen;

	if (spi_nand_io_opcodes_find(key)) {
		logm_err("I/O opcode group named '%s' is already defined\n", key);
		*cbret = UFP_ALREADY_EXIST;
		return 1;
	}

	if (asprintf(&path, "/%s/%s", "io-opcodes", key) < 0) {
		logm_err("No memory for JSON pointer string\n");
		*cbret = UFP_NOMEM;
		return 1;
	}

	if (!json_is_obj(jopcode, NULL)) {
		logm_err("%s is not an object\n", path);
		*cbret = UFP_JSON_TYPE_INVALID;
		goto cleanup;
	}

	ret = spi_nand_parse_ext_io_opcodes(jopcode, tmp_opcodes, NULL, path);
	if (ret) {
		logm_err("Failed to parse %s'\n", path);
		*cbret = ret;
		goto cleanup;
	}

	keylen = strlen(key);

	opcodes = malloc(sizeof(*opcodes) * __SPI_MEM_IO_MAX + keylen + 1);
	if (!opcodes) {
		logm_err("No memory for I/O opcode group '%s'\n", key);
		*cbret = UFP_NOMEM;
		goto cleanup;
	}

	memcpy(opcodes, tmp_opcodes, sizeof(*opcodes) * __SPI_MEM_IO_MAX);

	nkey = (char *)opcodes + sizeof(*opcodes) * __SPI_MEM_IO_MAX;
	memcpy(nkey, key, keylen + 1);

	ret = lookup_table_insert(ext_io_opcodes_list, nkey, opcodes);
	if (ret) {
		logm_err("No memory for inserting I/O opcode group '%s'\n", key);
		*cbret = UFP_NOMEM;
		goto cleanup;
	}

	free(path);

	return 0;

cleanup:
	if (opcodes)
		free(opcodes);

	free(path);

	return 1;
}

static ufprog_status spi_nand_load_ext_io_opcodes(struct json_object *jroot)
{
	ufprog_status ret, cbret = UFP_OK;
	struct json_object *jopcodes;

	ret = json_read_obj(jroot, "io-opcodes", &jopcodes);
	if (ret) {
		if (ret == UFP_NOT_EXIST)
			return UFP_OK;

		logm_err("Invalid type of /%s\n", "io-opcodes");
		return ret;
	}

	ret = json_obj_foreach(jopcodes, NULL, spi_nand_ext_io_opcodes_cb, &cbret, NULL);
	if (ret) {
		logm_err("Invalid type of /%s\n", "io-opcodes");
		return ret;
	}

	if (!cbret)
		return UFP_OK;

	return UFP_FAIL;
}

static int UFPROG_API spi_nand_reset_ext_io_opcode(void *priv, struct ufprog_lookup_table *tbl, const char *key,
						   void *ptr)
{
	lookup_table_delete(tbl, key);

	free(ptr);

	return 0;
}

static void spi_nand_reset_ext_io_opcodes(void)
{
	lookup_table_enum(ext_io_opcodes_list, spi_nand_reset_ext_io_opcode, NULL);
}

static ufprog_status spi_nand_parse_ext_page_layout(struct json_object *jpglyt, struct nand_page_layout **out_pglyt,
						    const char *path)
{
	struct nand_page_layout *pglyt = NULL;
	ufprog_status ret = UFP_OK;
	struct json_object *jitem;
	uint32_t i, count;

	count = (uint32_t)json_array_len(jpglyt);
	if (!count) {
		logm_err("No entry defined for page layout '%s'\n", path);
		return UFP_JSON_DATA_INVALID;
	}

	pglyt = malloc(sizeof(*pglyt) + count * sizeof(pglyt->entries[0]));
	if (!pglyt) {
		logm_err("No memory for page layout '%s'\n", path);
		return UFP_NOMEM;
	}

	pglyt->count = count;

	for (i = 0; i < count; i++) {
		ret = json_array_read_obj(jpglyt, i, &jitem);
		if (ret) {
			logm_err("%s/%u is not an object\n", path, i);
			goto cleanup;
		}

		ret = spi_nand_ext_part_read_enum(jitem, "type", page_layout_entry_types,
						  ARRAY_SIZE(page_layout_entry_types), &pglyt->entries[i].type, path);
		if (ret)
			goto cleanup;

		if (!json_node_exists(jitem, "count")) {
			logm_err("Byte count not specified for %s/%u\n", path, i);
			ret = UFP_JSON_DATA_INVALID;
			goto cleanup;
		}

		ret = spi_nand_ext_part_read_u32(jitem, "count", &pglyt->entries[i].num, 0, path);
		if (ret)
			goto cleanup;

		if (!pglyt->entries[i].num) {
			logm_err("Byte count must not be zero for %s/%u\n", path, i);
			ret = UFP_JSON_DATA_INVALID;
			goto cleanup;
		}
	}

	*out_pglyt = pglyt;

cleanup:
	if (ret)
		free(pglyt);

	return ret;
}

static int UFPROG_API spi_nand_ext_page_layout_cb(void *priv, const char *key, struct json_object *jpglyt)
{
	struct nand_page_layout *page_layout = NULL;
	ufprog_status ret, *cbret = priv;
	char *path, *nkey;

	if (spi_nand_page_layout_find(key)) {
		logm_err("Page layout named '%s' is already defined\n", key);
		*cbret = UFP_ALREADY_EXIST;
		return 1;
	}

	if (asprintf(&path, "/%s/%s", "page-layouts", key) < 0) {
		logm_err("No memory for JSON pointer string\n");
		*cbret = UFP_NOMEM;
		return 1;
	}

	if (!json_is_array(jpglyt, NULL)) {
		logm_err("%s is not an array\n", path);
		*cbret = UFP_JSON_TYPE_INVALID;
		goto cleanup;
	}

	ret = spi_nand_parse_ext_page_layout(jpglyt, &page_layout, path);
	if (ret) {
		logm_err("Failed to parse %s'\n", path);
		*cbret = ret;
		goto cleanup;
	}

	nkey = os_strdup(key);
	if (!key) {
		logm_err("No memory for page layout key name '%s'\n", key);
		*cbret = UFP_NOMEM;
		goto cleanup;
	}

	ret = lookup_table_insert(ext_page_layout_list, nkey, page_layout);
	if (ret) {
		logm_err("No memory for inserting page layout '%s'\n", key);
		*cbret = UFP_NOMEM;
		goto cleanup;
	}

	free(path);

	return 0;

cleanup:
	if (page_layout)
		free(page_layout);

	free(path);

	return 1;
}

static ufprog_status spi_nand_load_ext_page_layout(struct json_object *jroot)
{
	ufprog_status ret, cbret = UFP_OK;
	struct json_object *jpglyts;

	ret = json_read_obj(jroot, "page-layouts", &jpglyts);
	if (ret) {
		if (ret == UFP_NOT_EXIST)
			return UFP_OK;

		logm_err("Invalid type of /%s\n", "page-layouts");
		return ret;
	}

	ret = json_obj_foreach(jpglyts, NULL, spi_nand_ext_page_layout_cb, &cbret, NULL);
	if (ret) {
		logm_err("Invalid type of /%s\n", "page-layouts");
		return ret;
	}

	if (!cbret)
		return UFP_OK;

	return UFP_FAIL;
}

static int UFPROG_API spi_nand_reset_ext_page_layout(void *priv, struct ufprog_lookup_table *tbl, const char *key,
						      void *ptr)
{
	lookup_table_delete(tbl, key);

	free(ptr);
	free((void *)key);

	return 0;
}

static void spi_nand_reset_ext_page_layouts(void)
{
	lookup_table_enum(ext_page_layout_list, spi_nand_reset_ext_page_layout, NULL);
}

static ufprog_status memorg_read_power_of_2(struct json_object *jmemorg, const char *field, uint32_t *retval,
					    const char *path)
{
	if (!json_node_exists(jmemorg, field)) {
		logm_err("%s/%s not specified\n", path, field);
		return UFP_JSON_DATA_INVALID;
	}

	STATUS_CHECK_RET(spi_nand_ext_part_read_u32(jmemorg, field, retval, 0, path));

	if (!*retval) {
		logm_err("%s/%s must not be zero\n", path, field);
		return UFP_JSON_DATA_INVALID;
	}

	if (!is_power_of_2(*retval)) {
		logm_err("%s/%s must be power of 2\n", path, field);
		return UFP_JSON_DATA_INVALID;
	}

	return UFP_OK;
}

static ufprog_status spi_nand_parse_ext_memorg(struct json_object *jmemorg, struct nand_memorg *ret_memorg,
					       const char *path)
{
	STATUS_CHECK_RET(memorg_read_power_of_2(jmemorg, "page-size", &ret_memorg->page_size, path));
	STATUS_CHECK_RET(memorg_read_power_of_2(jmemorg, "pages-per-block", &ret_memorg->pages_per_block, path));
	STATUS_CHECK_RET(memorg_read_power_of_2(jmemorg, "blocks-per-lun", &ret_memorg->blocks_per_lun, path));
	STATUS_CHECK_RET(memorg_read_power_of_2(jmemorg, "luns-per-cs", &ret_memorg->luns_per_cs, path));
	STATUS_CHECK_RET(memorg_read_power_of_2(jmemorg, "planes-per-lun", &ret_memorg->planes_per_lun, path));

	if (!json_node_exists(jmemorg, "oob-size")) {
		logm_err("%s/%s not specified\n", path, "oob-size");
		return UFP_JSON_DATA_INVALID;
	}

	STATUS_CHECK_RET(spi_nand_ext_part_read_u32(jmemorg, "oob-size", &ret_memorg->oob_size, 0, path));

	if (!ret_memorg->oob_size) {
		logm_err("%s/%s must not be zero\n", path, "oob-size");
		return UFP_JSON_DATA_INVALID;
	}

	ret_memorg->num_chips = 1;

	return UFP_OK;
}

static int UFPROG_API spi_nand_ext_memorg_cb(void *priv, const char *key, struct json_object *jmemorg)
{
	struct nand_memorg tmp_memorg, *memorg = NULL;
	ufprog_status ret, *cbret = priv;
	char *path, *nkey;
	size_t keylen;

	if (spi_nand_memorg_find(key)) {
		logm_err("Memory organization named '%s' is already defined\n", key);
		*cbret = UFP_ALREADY_EXIST;
		return 1;
	}

	if (asprintf(&path, "/%s/%s", "memory-organizations", key) < 0) {
		logm_err("No memory for JSON pointer string\n");
		*cbret = UFP_NOMEM;
		return 1;
	}

	if (!json_is_obj(jmemorg, NULL)) {
		logm_err("%s is not an object\n", path);
		*cbret = UFP_JSON_TYPE_INVALID;
		goto cleanup;
	}

	ret = spi_nand_parse_ext_memorg(jmemorg, &tmp_memorg, path);
	if (ret) {
		logm_err("Failed to parse %s'\n", path);
		*cbret = ret;
		goto cleanup;
	}

	keylen = strlen(key);

	memorg = malloc(sizeof(*memorg) + keylen + 1);
	if (!memorg) {
		logm_err("No memory for memory organization '%s'\n", key);
		*cbret = UFP_NOMEM;
		goto cleanup;
	}

	memcpy(memorg, &tmp_memorg, sizeof(*memorg));

	nkey = (char *)memorg + sizeof(*memorg);
	memcpy(nkey, key, keylen + 1);

	ret = lookup_table_insert(ext_memorg_list, nkey, memorg);
	if (ret) {
		logm_err("No memory for inserting memory organization '%s'\n", key);
		*cbret = UFP_NOMEM;
		goto cleanup;
	}

	free(path);

	return 0;

cleanup:
	if (memorg)
		free(memorg);

	free(path);

	return 1;
}

static ufprog_status spi_nand_load_ext_memorg(struct json_object *jroot)
{
	ufprog_status ret, cbret = UFP_OK;
	struct json_object *jmemorgs;

	ret = json_read_obj(jroot, "memory-organizations", &jmemorgs);
	if (ret) {
		if (ret == UFP_NOT_EXIST)
			return UFP_OK;

		logm_err("Invalid type of /%s\n", "memory-organizations");
		return ret;
	}

	ret = json_obj_foreach(jmemorgs, NULL, spi_nand_ext_memorg_cb, &cbret, NULL);
	if (ret) {
		logm_err("Invalid type of /%s\n", "memory-organizations");
		return ret;
	}

	if (!cbret)
		return UFP_OK;

	return UFP_FAIL;
}

static int UFPROG_API spi_nand_reset_ext_memorg(void *priv, struct ufprog_lookup_table *tbl, const char *key, void *ptr)
{
	lookup_table_delete(tbl, key);

	free(ptr);

	return 0;
}

static void spi_nand_reset_ext_memorgs(void)
{
	lookup_table_enum(ext_memorg_list, spi_nand_reset_ext_memorg, NULL);
}

static ufprog_status spi_nand_ext_ids_init_lists(void)
{
	ufprog_status ret;

	if (!ext_io_opcodes_list) {
		ret = lookup_table_create(&ext_io_opcodes_list, 0);
		if (ret) {
			logm_err("No memory for external I/O opcodes list\n");
			return ret;
		}
	}

	if (!ext_page_layout_list) {
		ret = lookup_table_create(&ext_page_layout_list, 0);
		if (ret) {
			logm_err("No memory for external page layout list\n");
			return ret;
		}
	}

	if (!ext_memorg_list) {
		ret = lookup_table_create(&ext_memorg_list, 0);
		if (ret) {
			logm_err("No memory for external memory organization list\n");
			return ret;
		}
	}

	return UFP_OK;
}

static void spi_nand_reset_ext_id_list(void)
{
	spi_nand_reset_ext_vendors(spi_nand_reset_ext_vendor);
	spi_nand_reset_ext_io_opcodes();
	spi_nand_reset_ext_page_layouts();
	spi_nand_reset_ext_memorgs();
}

ufprog_status spi_nand_load_ext_id_list(void)
{
	struct json_object *jroot;
	ufprog_status ret;

	spi_nand_reset_ext_id_list();

	STATUS_CHECK_RET(spi_nand_ext_ids_init_lists());

	ret = json_open_config("spi-nand-ids", &jroot);
	if (ret) {
		if (ret == UFP_FILE_NOT_EXIST) {
			logm_dbg("External flash table file does not exist\n");
			return UFP_OK;
		}

		if (ret == UFP_FILE_READ_FAILURE)
			logm_err("Unable to read external flash table file\n");
		else if (ret == UFP_JSON_DATA_INVALID)
			logm_err("External flash table file has invalid format\n");
		else
			logm_err("Unable to process external flash table file\n");

		return ret;
	}

	STATUS_CHECK_GOTO_RET(spi_nand_load_ext_io_opcodes(jroot), ret, cleanup);
	STATUS_CHECK_GOTO_RET(spi_nand_load_ext_page_layout(jroot), ret, cleanup);
	STATUS_CHECK_GOTO_RET(spi_nand_load_ext_memorg(jroot), ret, cleanup);
	STATUS_CHECK_GOTO_RET(spi_nand_load_ext_vendors(jroot), ret, cleanup);

	logm_notice("Successfully loaded external flash table\n");

	ret = UFP_OK;

cleanup:
	json_free(jroot);

	if (ret)
		spi_nand_reset_ext_id_list();

	return ret;
}
