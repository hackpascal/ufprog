// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR external flash table processing
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
	const struct spi_nor_vendor *builtin_vendor;
	struct spi_nor_vendor *vendor;
	struct spi_nor_flash_part *parts;
	uint32_t maxparts;
	uint32_t nparts;
	const char *path;
	ufprog_status ret;
};

struct part_erase_info_item {
	const char *name;
	const struct spi_nor_erase_info *info;
};

struct part_io_opcodes_item {
	const char *name;
	const struct spi_nor_io_opcode *opcodes;
};

struct part_wp_item {
	const char *name;
	const struct spi_nor_wp_info *info;
};

static const struct spi_nor_part_flag_enum_info part_flags[] = {
	{ 0, "meta" },
	{ 1, "no-sfdp" },
	{ 2, "4k-sector" },
	{ 3, "32k-sector" },
	{ 4, "64k-block" },
	{ 5, "256k-block" },
	{ 6, "non-volatile-sr" },
	{ 7, "volatile-sr" },
	{ 8, "volatile-sr-wren-50h" },
	{ 9, "unique-id" },
	{ 10, "full-dpi-opcodes" },
	{ 11, "full-qpi-opcodes" },
	{ 12, "sfdp-4b-mode" },
	{ 13, "global-block-unlock" },
	{ 14, "aai-word-program" },
	{ 15, "no-op" },
};

static const struct spi_nor_part_flag_enum_info part_qe_types[] = {
	{ QE_DONT_CARE, "dont-care" },
	{ QE_SR1_BIT6, "sr1-bit6" },
	{ QE_SR2_BIT1, "sr2-bit1" },
	{ QE_SR2_BIT1_WR_SR1, "sr2-bit1-wr-sr1" },
	{ QE_SR2_BIT7, "sr2-bit7" },
};

static const struct spi_nor_part_flag_enum_info part_qpi_en_types[] = {
	{ QPI_EN_NONE, "none" },
	{ QPI_EN_QER_38H, "qer-38h" },
	{ QPI_EN_38H, "38h" },
	{ QPI_EN_35H, "35h" },
};

static const struct spi_nor_part_flag_enum_info part_qpi_dis_types[] = {
	{ QPI_DIS_NONE, "none" },
	{ QPI_DIS_FFH, "ffh" },
	{ QPI_DIS_F5H, "f5h" },
	{ QPI_DIS_66H_99H, "66h-99h" },
};

static const struct spi_nor_part_flag_enum_info part_4b_en_types[] = {
	{ A4B_EN_NONE, "none" },
	{ A4B_EN_B7H, "b7h" },
	{ A4B_EN_WREN_B7H, "wren-b7h" },
	{ A4B_EN_EAR, "ear" },
	{ A4B_EN_BANK, "bank" },
	{ A4B_EN_NVCR, "nvcr" },
	{ A4B_EN_4B_OPCODE, "4b-opcodes" },
	{ A4B_EN_ALWAYS, "always" },
};

static const struct spi_nor_part_flag_enum_info part_4b_dis_types[] = {
	{ A4B_DIS_NONE, "none" },
	{ A4B_DIS_E9H, "e9h" },
	{ A4B_DIS_WREN_E9H, "wren-e9h" },
	{ A4B_DIS_EAR, "ear" },
	{ A4B_DIS_BANK, "bank" },
	{ A4B_DIS_NVCR, "nvcr" },
	{ A4B_DIS_66H_99H, "66h-99h" },
};

static const struct spi_nor_part_flag_enum_info part_4b_flags[] = {
	{ 0, "b7h-e9h" },
	{ 1, "wren-b7h-e9h" },
	{ 2, "ear" },
	{ 3, "bank" },
	{ 4, "opcode" },
	{ 5, "always" },
};

static const struct spi_nor_part_flag_enum_info part_soft_reset_flags[] = {
	{ 0, "drive-4io-fh-8-clocks" },
	{ 1, "drive-4io-fh-10-clocks-4b-mode" },
	{ 2, "drive-4io-fh-16-clocks" },
	{ 3, "f0h" },
	{ 4, "66h-99h" },
};

static const struct part_erase_info_item builtin_erase_info[] = {
	{ "default-3b", &default_erase_opcodes_3b },
	{ "default-4b", &default_erase_opcodes_4b },
};

static const struct part_io_opcodes_item builtin_io_opcodes[] = {
	{ "default-read-3b", default_read_opcodes_3b },
	{ "default-read-4b", default_read_opcodes_4b },
	{ "default-pp-3b", default_pp_opcodes_3b },
	{ "default-pp-4b", default_pp_opcodes_4b },
};

static const struct part_wp_item builtin_wp_items[] = {
	{ "2bp", &wpr_2bp },
	{ "2bp-tb", &wpr_2bp_tb },
	{ "3bp-tb", &wpr_3bp_tb },
	{ "3bp-tb-ratio", &wpr_3bp_tb_ratio },
	{ "3bp-tb-sec", &wpr_3bp_tb_sec },
	{ "3bp-tb-sec-ratio", &wpr_3bp_tb_sec_ratio },
	{ "3bp-tb-sec-cmp", &wpr_3bp_tb_sec_cmp },
	{ "3bp-tb-sec-cmp-ratio", &wpr_3bp_tb_sec_cmp_ratio },
	{ "4bp-tb", &wpr_4bp_tb },
	{ "4bp-tb-cmp", &wpr_4bp_tb_cmp },
};

static struct ufprog_lookup_table *ext_erase_info_list;
static struct ufprog_lookup_table *ext_io_opcodes_list;

static ufprog_status spi_nor_parse_ext_erase_group(struct json_object *jei, struct spi_nor_erase_info *retei,
						   const char *path);

static ufprog_status spi_nor_parse_ext_io_opcodes(struct json_object *jopcode, struct spi_nor_io_opcode *ret_opcodes,
						  uint32_t *out_iocaps, const char *path);

static const struct spi_nor_erase_info *spi_nor_erase_info_find(const char *name)
{
	const struct spi_nor_erase_info *ei;
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(builtin_erase_info); i++) {
		if (!strcmp(builtin_erase_info[i].name, name))
			return builtin_erase_info[i].info;
	}

	if (lookup_table_find(ext_erase_info_list, name, (void **)&ei))
		return ei;

	return NULL;
}

static const struct spi_nor_io_opcode *spi_nor_io_opcodes_find(const char *name)
{
	const struct spi_nor_io_opcode *opcodes;
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(builtin_io_opcodes); i++) {
		if (!strcmp(builtin_io_opcodes[i].name, name))
			return builtin_io_opcodes[i].opcodes;
	}

	if (lookup_table_find(ext_io_opcodes_list, name, (void **)&opcodes))
		return opcodes;

	return NULL;
}

static const struct spi_nor_wp_info *spi_nor_wp_find(const char *name)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(builtin_wp_items); i++) {
		if (!strcmp(builtin_wp_items[i].name, name))
			return builtin_wp_items[i].info;
	}

	return NULL;
}

static ufprog_status spi_nor_ext_part_read_id(struct json_object *jpart, struct spi_nor_id *id, const char *path)
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

	id->len = (uint32_t)json_array_len(jid);
	if (id->len > SPI_NOR_MAX_ID_LEN) {
		logm_err("Length of %s/%s is bigger than %u\n", path, "id", SPI_NOR_MAX_ID_LEN);
		return UFP_JSON_DATA_INVALID;
	}

	for (i = 0; i < id->len; i++) {
		ret = json_array_read_hex32(jid, i, &val, 0);
		if (ret) {
			logm_err("Invalid value of %s/%s/%u\n", path, "id", i);
			return ret;
		}

		if (val > 0xff) {
			logm_err("The value of %s/%s/%u is bigger than FFh\n", path, "id", i);
			return UFP_JSON_DATA_INVALID;
		}

		id->id[i] = (uint8_t)val;
	}

	return UFP_OK;
}

static bool spi_nor_ext_part_get_flag_enum_val_from_name(const struct spi_nor_part_flag_enum_info *info, uint32_t count,
							 const char *name, uint32_t *retval)
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

static ufprog_status spi_nor_ext_part_read_bit_flags(struct json_object *jflags,
						     const struct spi_nor_part_flag_enum_info *info, uint32_t count,
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

		if (!spi_nor_ext_part_get_flag_enum_val_from_name(info, count, flag_name, &bit)) {
			logm_err("%s/%s/%zu is not a valid flag name\n", path, field, i);
			return UFP_NOT_EXIST;
		}

		*retflags |= BIT(bit);
	}

	return UFP_OK;
}

static ufprog_status spi_nor_ext_part_read_flags(struct json_object *jobj, const char *field,
						 const struct spi_nor_part_flag_enum_info *info, uint32_t count,
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

	return spi_nor_ext_part_read_bit_flags(jflags, info, count, retflags, path, field);
}

static ufprog_status spi_nor_ext_part_read_enum(struct json_object *jobj, const char *field,
						const struct spi_nor_part_flag_enum_info *info, uint32_t count,
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

static ufprog_status spi_nor_ext_part_read_u32(struct json_object *jobj, const char *field, uint32_t *retval,
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

static ufprog_status spi_nor_ext_part_read_size(struct json_object *jobj, const char *field, uint64_t *retval,
						const char *path)
{
	const char *str;
	uint64_t val;
	char *end;

	if (!json_node_exists(jobj, field)) {
		logm_err("%s/%s not defined\n", path, field);
		return UFP_NOT_EXIST;
	}

	if (json_is_int(jobj, field)) {
		json_read_uint64(jobj, field, retval, 0);
	} else if (json_is_str(jobj, field)) {
		json_read_str(jobj, field, &str, NULL);

		val = strtoull(str, &end, 0);
		if (end == str || val == ULLONG_MAX) {
			logm_err("Invalid data of %s/%s\n", path, field);
			return UFP_JSON_TYPE_INVALID;
		}

		if (*end) {
			if (*end == 'k' || *end == 'K') {
				val <<= 10;
				end++;
			} else if (*end == 'm' || *end == 'M') {
				val <<= 20;
				end++;
			} else if (*end == 'g' || *end == 'G') {
				val <<= 30;
				end++;
			}

			if (*end) {
				logm_err("Invalid data of %s/%s\n", path, field);
				return UFP_JSON_TYPE_INVALID;
			}
		}

		*retval = val;
	} else {
		logm_err("Invalid type of %s/%s\n", path, field);
		return UFP_JSON_TYPE_INVALID;
	}

	if (is_power_of_2(*retval))
		return UFP_OK;

	logm_err("Invalid value of %s/%s\n", path, field);

	return UFP_JSON_DATA_INVALID;
}

static ufprog_status spi_nor_ext_part_read_io_caps(struct json_object *jpart, const char *field, uint32_t *retval,
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

static ufprog_status spi_nor_ext_part_read_erase_info(struct json_object *jpart, const char *field,
						      struct spi_nor_erase_info **outei, const char *path)
{
	const struct spi_nor_erase_info *cei;
	struct spi_nor_erase_info tmpei;
	struct json_object *jei;
	const char *ei_name;
	ufprog_status ret;
	char *npath;

	if (!json_node_exists(jpart, field)) {
		*outei = NULL;
		return UFP_OK;
	}

	if (asprintf(&npath, "%s/%s", path, field) < 0) {
		logm_err("No memory for JSON pointer string\n");
		return UFP_NOMEM;
	}

	if (json_is_str(jpart, field)) {
		json_read_str(jpart, field, &ei_name, NULL);

		cei = spi_nor_erase_info_find(ei_name);
		if (!cei) {
			logm_err("Erase group named '%s' not found for %s\n", ei_name, npath);
			ret = UFP_NOT_EXIST;
			goto cleanup;
		}
	} else if (json_is_array(jpart, field)) {
		json_read_array(jpart, field, &jei);

		ret = spi_nor_parse_ext_erase_group(jei, &tmpei, npath);
		if (ret) {
			logm_err("Failed to parse erase group %s\n", npath);
			ret = UFP_JSON_DATA_INVALID;
			goto cleanup;
		}

		cei = &tmpei;
	} else {
		logm_err("Invalid type of %s\n", npath);
		ret = UFP_JSON_TYPE_INVALID;
		goto cleanup;
	}

	*outei = malloc(sizeof(struct spi_nor_erase_info));
	if (!*outei) {
		logm_err("No memory for erase group %s\n", npath);
		ret = UFP_NOMEM;
		goto cleanup;
	}

	memcpy(*outei, cei, sizeof(struct spi_nor_erase_info));

	free(npath);

	return UFP_OK;

cleanup:
	free(npath);

	return ret;
}

static ufprog_status spi_nor_ext_part_read_io_opcodes(struct json_object *jpart, const char *field,
						      struct spi_nor_io_opcode **out_opcodes, uint32_t *out_iocaps,
						      const char *path)
{
	struct spi_nor_io_opcode tmp_opcodes[__SPI_MEM_IO_MAX];
	const struct spi_nor_io_opcode *copcodes;
	struct json_object *jopcode;
	const char *opcode_name;
	ufprog_status ret;
	char *npath;

	if (!json_node_exists(jpart, field)) {
		*out_opcodes = NULL;
		return UFP_OK;
	}

	if (asprintf(&npath, "%s/%s", path, field) < 0) {
		logm_err("No memory for JSON pointer string\n");
		return UFP_NOMEM;
	}

	if (json_is_str(jpart, field)) {
		json_read_str(jpart, field, &opcode_name, NULL);

		copcodes = spi_nor_io_opcodes_find(opcode_name);
		if (!copcodes) {
			logm_err("I/O opcode group named '%s' not found for %s\n", opcode_name, npath);
			ret = UFP_NOT_EXIST;
			goto cleanup;
		}

		*out_iocaps = 0;
	} else if (json_is_obj(jpart, field)) {
		json_read_obj(jpart, field, &jopcode);

		ret = spi_nor_parse_ext_io_opcodes(jopcode, tmp_opcodes, out_iocaps, npath);
		if (ret) {
			logm_err("Failed to parse I/O opcode group %s\n", npath);
			ret = UFP_JSON_DATA_INVALID;
			goto cleanup;
		}

		copcodes = tmp_opcodes;
	} else {
		logm_err("Invalid type of %s\n", npath);
		ret = UFP_JSON_TYPE_INVALID;
		goto cleanup;
	}

	*out_opcodes = malloc(sizeof(struct spi_nor_io_opcode) * __SPI_MEM_IO_MAX);
	if (!*out_opcodes) {
		logm_err("No memory for I/O opcode group %s\n", npath);
		ret = UFP_NOMEM;
		goto cleanup;
	}

	memcpy(*out_opcodes, copcodes, sizeof(struct spi_nor_io_opcode) * __SPI_MEM_IO_MAX);

	free(npath);

	return UFP_OK;

cleanup:
	free(npath);

	return ret;
}

static ufprog_status spi_nor_ext_part_read_otp(struct json_object *jpart, struct spi_nor_otp_info **outotp,
					       const char *path)
{
	struct spi_nor_otp_info tmpotp;
	struct json_object *jotp;
	ufprog_status ret;
	uint64_t size;
	char *npath;

	if (!json_node_exists(jpart, "otp")) {
		*outotp = NULL;
		return UFP_OK;
	}

	if (!json_is_obj(jpart, "otp")) {
		logm_err("Invalid type of %s/%s\n", path, "otp");
		return UFP_JSON_TYPE_INVALID;
	}

	json_read_obj(jpart, "otp", &jotp);

	memset(&tmpotp, 0, sizeof(tmpotp));

	if (asprintf(&npath, "%s/%s", path, "otp") < 0) {
		logm_err("No memory for JSON pointer string\n");
		return UFP_NOMEM;
	}

	ret = spi_nor_ext_part_read_size(jotp, "size", &size, npath);
	if (ret)
		goto cleanup;

	if (size > UINT32_MAX) {
		logm_err("OTP size in %s is too big\n", npath);
		ret = UFP_NOMEM;
		goto cleanup;
	}

	tmpotp.size = (uint32_t)size;

	ret = spi_nor_ext_part_read_u32(jotp, "start-index", &tmpotp.start_index, 0, npath);
	if (ret)
		goto cleanup;

	ret = spi_nor_ext_part_read_u32(jotp, "count", &tmpotp.count, 0, npath);
	if (ret)
		goto cleanup;

	if (!tmpotp.count) {
		logm_err("OTP region count is zero in %s\n", npath);
		ret = UFP_JSON_DATA_INVALID;
		goto cleanup;
	}

	*outotp = malloc(sizeof(tmpotp));
	if (!*outotp) {
		logm_err("No memory OTP region %s\n", npath);
		ret = UFP_NOMEM;
		goto cleanup;
	}

	memcpy(*outotp, &tmpotp, sizeof(tmpotp));

	free(npath);

	return UFP_OK;

cleanup:
	free(npath);

	return ret;
}

static ufprog_status spi_nor_ext_part_read_wp_info(struct json_object *jpart, struct spi_nor_wp_info **outwp,
						   const char *path)
{
	const struct spi_nor_wp_info *cwp;
	const char *wp_name;
	ufprog_status ret;
	uint32_t wplen;
	char *npath;

	if (!json_node_exists(jpart, "wp")) {
		*outwp = NULL;
		return UFP_OK;
	}

	if (asprintf(&npath, "%s/%s", path, "wp") < 0) {
		logm_err("No memory for JSON pointer string\n");
		return UFP_NOMEM;
	}

	if (json_is_str(jpart, "wp")) {
		json_read_str(jpart, "wp", &wp_name, NULL);

		cwp = spi_nor_wp_find(wp_name);
		if (!cwp) {
			logm_err("Write-protect region named '%s' not found for %s\n", wp_name, npath);
			ret = UFP_NOT_EXIST;
			goto cleanup;
		}
	} else {
		logm_err("Invalid type of %s\n", npath);
		ret = UFP_JSON_TYPE_INVALID;
		goto cleanup;
	}

	wplen = sizeof(struct spi_nor_wp_info) + cwp->num * sizeof(struct spi_nor_wp_range);

	*outwp = malloc(wplen);
	if (!*outwp) {
		logm_err("No memory for Write-protect region %s\n", npath);
		ret = UFP_NOMEM;
		goto cleanup;
	}

	memcpy(*outwp, cwp, wplen);

	free(npath);

	return UFP_OK;

cleanup:
	free(npath);

	return ret;
}

static ufprog_status spi_nor_ext_part_read_alias(struct json_object *jpart, struct spi_nor_flash_part_alias **outalias,
						 const char *path)
{
	struct spi_nor_flash_part_alias *alias;
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

	alias = calloc(1, sizeof(*alias) + n * sizeof(struct spi_nor_flash_part_alias_item));
	if (!alias) {
		logm_err("No memory for flash part alias\n");
		return UFP_NOMEM;
	}

	alias->num = (uint32_t)n;

	for (i = 0; i < n; i++) {
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
			alias->items[i].vendor = spi_nor_find_vendor_by_id(vendor);
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

		alias->items[i].model = os_strdup(model);
		if (alias->items[i].model) {
			logm_err("No memory for flash part alias\n");
			return UFP_NOMEM;
		}
	}

	*outalias = alias;

	return UFP_OK;
}

static void spi_nor_reset_ext_part(struct spi_nor_flash_part *part)
{
	uint32_t i;

	if (part->model) {
		free((void *)part->model);
		part->model = NULL;
	}

	if (part->erase_info_3b) {
		free((void *)part->erase_info_3b);
		part->erase_info_3b = NULL;
	}

	if (part->erase_info_4b) {
		free((void *)part->erase_info_4b);
		part->erase_info_4b = NULL;
	}

	if (part->read_opcodes_3b) {
		free((void *)part->read_opcodes_3b);
		part->read_opcodes_3b = NULL;
	}

	if (part->read_opcodes_4b) {
		free((void *)part->read_opcodes_4b);
		part->read_opcodes_4b = NULL;
	}

	if (part->pp_opcodes_3b) {
		free((void *)part->pp_opcodes_3b);
		part->pp_opcodes_3b = NULL;
	}

	if (part->pp_opcodes_4b) {
		free((void *)part->pp_opcodes_4b);
		part->pp_opcodes_4b = NULL;
	}

	if (part->otp) {
		free((void *)part->otp);
		part->otp = NULL;
	}

	if (part->wp_ranges) {
		free((void *)part->wp_ranges);
		part->wp_ranges = NULL;
	}

	if (part->alias) {
		for (i = 0; i < part->alias->num; i++) {
			if (part->alias->items[i].model)
				free((void *)part->alias->items[i].model);
		}

		free((void *)part->alias);
		part->alias = NULL;
	}
}

static int UFPROG_API spi_nor_ext_vendor_parts_cb(void *priv, const char *key, struct json_object *jpart)
{
	struct ext_parts_info *pi = priv;
	struct spi_nor_flash_part *part = &pi->parts[pi->nparts];
	const struct spi_nor_flash_part *chkpart;
	struct spi_nor_flash_part_alias *alias;
	struct spi_nor_wp_info *wp_ranges;
	struct spi_nor_io_opcode *opcodes;
	struct spi_nor_erase_info *ei;
	struct spi_nor_otp_info *otp;
	uint32_t val, io_caps;
	char *path;

	if (!*key) {
		logm_err("Part name must not be empty\n");
		pi->ret = UFP_FAIL;
		return 1;
	}

	if (pi->builtin_vendor) {
		chkpart = spi_nor_vendor_find_part_by_name(key, pi->builtin_vendor);
		if (chkpart) {
			logm_err("Part '%s' already exists in built-in part list\n", key);
			pi->ret = UFP_ALREADY_EXIST;
			return 1;
		}
	}

	chkpart = spi_nor_vendor_find_part_by_name(key, pi->vendor);
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

	pi->ret = spi_nor_ext_part_read_id(jpart, &part->id, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_flags(jpart, "flags", part_flags, ARRAY_SIZE(part_flags), &part->flags, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_flags(jpart, "vendor-flags", pi->vendor->vendor_flag_names,
					      pi->vendor->num_vendor_flag_names, &part->vendor_flags, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_flags(jpart, "4b-flags", part_4b_flags, ARRAY_SIZE(part_4b_flags),
					      &part->a4b_flags, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_flags(jpart, "soft-reset-flags", part_soft_reset_flags,
					      ARRAY_SIZE(part_soft_reset_flags), &part->soft_reset_flags, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_enum(jpart, "qe-type", part_qe_types, ARRAY_SIZE(part_qe_types), &val, path);
	if (pi->ret)
		goto cleanup;
	part->qe_type = val;

	pi->ret = spi_nor_ext_part_read_enum(jpart, "qpi-en-type", part_qpi_en_types, ARRAY_SIZE(part_qpi_en_types),
					     &val, path);
	if (pi->ret)
		goto cleanup;
	part->qpi_en_type = val;

	pi->ret = spi_nor_ext_part_read_enum(jpart, "qpi-dis-type", part_qpi_dis_types, ARRAY_SIZE(part_qpi_dis_types),
					     &val, path);
	if (pi->ret)
		goto cleanup;
	part->qpi_dis_type = val;

	pi->ret = spi_nor_ext_part_read_enum(jpart, "4b-en-type", part_4b_en_types, ARRAY_SIZE(part_4b_en_types),
					     &val, path);
	if (pi->ret)
		goto cleanup;
	part->a4b_en_type = val;

	pi->ret = spi_nor_ext_part_read_enum(jpart, "4b-dis-type", part_4b_dis_types, ARRAY_SIZE(part_4b_dis_types),
					     &val, path);
	if (pi->ret)
		goto cleanup;
	part->a4b_dis_type = val;

	pi->ret = spi_nor_ext_part_read_u32(jpart, "max-speed-spi-mhz", &part->max_speed_spi_mhz, 0, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_u32(jpart, "max-speed-dual-mhz", &part->max_speed_dual_mhz, 0, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_u32(jpart, "max-speed-quad-mhz", &part->max_speed_quad_mhz, 0, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_u32(jpart, "page-size", &part->page_size, 0, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_u32(jpart, "max-pp-time-us", &part->max_pp_time_us, 0, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_size(jpart, "size", &part->size, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_u32(jpart, "num-dies", &part->ndies, 1, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_io_caps(jpart, "read-io-caps", &part->read_io_caps, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_io_caps(jpart, "pp-io-caps", &part->pp_io_caps, path);
	if (pi->ret)
		goto cleanup;

	pi->ret = spi_nor_ext_part_read_erase_info(jpart, "erase-info-3b", &ei, path);
	if (pi->ret)
		goto cleanup;
	part->erase_info_3b = ei;

	pi->ret = spi_nor_ext_part_read_erase_info(jpart, "erase-info-4b", &ei, path);
	if (pi->ret)
		goto cleanup;
	part->erase_info_4b = ei;

	pi->ret = spi_nor_ext_part_read_io_opcodes(jpart, "read-opcodes-3b", &opcodes, &io_caps, path);
	if (pi->ret)
		goto cleanup;
	part->read_opcodes_3b = opcodes;
	part->read_io_caps |= io_caps;

	pi->ret = spi_nor_ext_part_read_io_opcodes(jpart, "read-opcodes-4b", &opcodes, &io_caps, path);
	if (pi->ret)
		goto cleanup;
	part->read_opcodes_4b = opcodes;
	part->read_io_caps |= io_caps;

	pi->ret = spi_nor_ext_part_read_io_opcodes(jpart, "pp-opcodes-3b", &opcodes, &io_caps, path);
	if (pi->ret)
		goto cleanup;
	part->pp_opcodes_3b = opcodes;
	part->pp_io_caps |= io_caps;

	pi->ret = spi_nor_ext_part_read_io_opcodes(jpart, "pp-opcodes-4b", &opcodes, &io_caps, path);
	if (pi->ret)
		goto cleanup;
	part->pp_opcodes_4b = opcodes;
	part->pp_io_caps |= io_caps;

	pi->ret = spi_nor_ext_part_read_otp(jpart, &otp, path);
	if (pi->ret)
		goto cleanup;
	part->otp = otp;

	pi->ret = spi_nor_ext_part_read_wp_info(jpart, &wp_ranges, path);
	if (pi->ret)
		goto cleanup;
	part->wp_ranges = wp_ranges;

	pi->ret = spi_nor_ext_part_read_alias(jpart, &alias, path);
	if (pi->ret)
		goto cleanup;
	part->alias = alias;

	pi->nparts++;
	pi->ret = UFP_OK;

	free(path);

	return 0;

cleanup:
	spi_nor_reset_ext_part(part);

	free(path);

	return 1;
}

static ufprog_status spi_nor_load_flash_parts(struct json_object *jparts, struct ext_parts_info *pi)
{
	ufprog_status ret;

	ret = json_obj_foreach(jparts, NULL, spi_nor_ext_vendor_parts_cb, pi, NULL);
	if (ret) {
		if (ret == UFP_NOT_EXIST)
			return UFP_OK;

		return ret;
	}

	if (!pi->ret)
		return UFP_OK;

	return UFP_FAIL;
}

static int UFPROG_API spi_nor_ext_vendors_cb(void *priv, const char *key, struct json_object *jobj)
{
	const struct spi_nor_vendor *vendor;
	struct spi_nor_vendor *new_vendor;
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

	vendor = spi_nor_find_vendor_by_id(key);
	if (vendor) {
		if (spi_nor_is_ext_vendor(vendor)) {
			logm_err("Vendor ID '%s' already exists\n", key);
			*cbret = UFP_ALREADY_EXIST;
			return 1;
		}
	}

	new_vendor = spi_nor_alloc_ext_vendor();
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

	ret = spi_nor_load_flash_parts(jparts, &pi);
	if (ret)
		goto cleanup_pi;

	new_vendor->parts = pi.parts;
	new_vendor->nparts = pi.nparts;

out:
	free(path);

	*cbret = UFP_OK;
	return 0;

cleanup_pi:
	free(pi.parts);
	pi.parts = NULL;

cleanup:
	if (new_vendor->id) {
		free((void *)new_vendor->id);
		new_vendor->id = NULL;
	}

	if (new_vendor->name) {
		free((void *)new_vendor->name);
		new_vendor->name = NULL;
	}

	free(path);

	return 1;
}

static ufprog_status spi_nor_load_ext_vendors(struct json_object *jroot)
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

	if (!spi_nor_set_ext_vendor_capacity(count)) {
		logm_err("No memory for external vendor list\n");
		return UFP_NOMEM;
	}

	ret = json_obj_foreach(jvendors, NULL, spi_nor_ext_vendors_cb, &cbret, NULL);
	if (ret) {
		logm_err("Invalid type of /%s\n", "vendors");
		return ret;
	}

	if (!cbret)
		return UFP_OK;

	return UFP_FAIL;
}

static void spi_nor_reset_ext_vendor(struct spi_nor_vendor *vendor)
{
	uint32_t i;

	for (i = 0; i < vendor->nparts; i++)
		spi_nor_reset_ext_part((struct spi_nor_flash_part *)&vendor->parts[i]);

	free((void *)vendor->parts);
}

static ufprog_status spi_nor_parse_ext_erase_group(struct json_object *jei, struct spi_nor_erase_info *retei,
						   const char *path)
{
	struct json_object *jeiitem;
	uint32_t val, num = 0;
	ufprog_status ret;
	size_t count, i;
	uint64_t size;
	char *npath;

	memset(retei, 0, sizeof(*retei));

	count = json_array_len(jei);
	if (count > SPI_NOR_MAX_ERASE_INFO) {
		logm_err("%s has more than %u items\n", path, SPI_NOR_MAX_ERASE_INFO);
		return UFP_FAIL;
	}

	for (i = 0; i < count; i++) {
		ret = json_array_read_obj(jei, i, &jeiitem);
		if (ret) {
			logm_err("%s/%zu is not an object\n", path, i);
			return UFP_JSON_TYPE_INVALID;
		}

		if (!json_node_exists(jeiitem, "opcode")) {
			logm_err("%s/%zu/%s not defined\n", path, i, "opcode");
			return UFP_NOT_EXIST;
		}

		ret = json_read_hex32(jeiitem, "opcode", &val, 0);
		if (ret) {
			logm_err("Invalid type of %s/%zu/%s\n", path, i, "opcode");
			return ret;
		}

		if (!val || val > 0xff) {
			logm_err("%s/%zu/%s is invalid\n", path, i, "opcode");
			return UFP_JSON_DATA_INVALID;
		}

		retei->info[num].opcode = (uint8_t)val;

		if (asprintf(&npath, "%s/%zu", path, i) < 0) {
			logm_err("No memory for JSON pointer string\n");
			return UFP_NOMEM;
		}

		ret = spi_nor_ext_part_read_size(jeiitem, "size", &size, npath);

		free(npath);

		if (ret)
			return ret;

		if (!is_power_of_2(size) || size > UINT32_MAX) {
			logm_err("Invalid value of %s/%zu/%s\n", path, i, "size");
			return UFP_JSON_DATA_INVALID;
		}

		retei->info[num].size = (uint32_t)size;

		ret = json_read_uint32(jeiitem, "max-erase-time-ms", &retei->info[num].max_erase_time_ms, 0);
		if (ret) {
			logm_err("Invalid type of %s/%zu/%s\n", path, i, "max-erase-time-ms");
			return ret;
		}

		num++;
	}

	return UFP_OK;
}

static int UFPROG_API spi_nor_ext_erase_group_cb(void *priv, const char *key, struct json_object *jei)
{
	struct spi_nor_erase_info *ei = NULL, tmpei;
	ufprog_status ret, *cbret = priv;
	char *path;

	if (spi_nor_erase_info_find(key)) {
		logm_err("Erase group named '%s' is already defined\n", key);
		*cbret = UFP_ALREADY_EXIST;
		return 1;
	}

	if (asprintf(&path, "/%s/%s", "erase-groups", key) < 0) {
		logm_err("No memory for JSON pointer string\n");
		*cbret = UFP_NOMEM;
		return 1;
	}

	if (!json_is_array(jei, NULL)) {
		logm_err("%s is not an array\n", path);
		*cbret = UFP_JSON_TYPE_INVALID;
		goto cleanup;
	}

	ret = spi_nor_parse_ext_erase_group(jei, &tmpei, path);
	if (ret) {
		logm_err("Failed to parse %s'\n", path);
		*cbret = ret;
		goto cleanup;
	}

	ei = malloc(sizeof(*ei));
	if (!ei) {
		logm_err("No memory for erase group '%s'\n", key);
		*cbret = UFP_NOMEM;
		goto cleanup;
	}

	memcpy(ei, &tmpei, sizeof(tmpei));

	ret = lookup_table_insert(ext_erase_info_list, key, ei);
	if (ret) {
		logm_err("No memory for inserting erase group '%s'\n", key);
		*cbret = UFP_NOMEM;
		goto cleanup;
	}

	free(path);

	return 0;

cleanup:
	if (ei)
		free(ei);

	free(path);

	return 1;
}

static ufprog_status spi_nor_load_ext_erase_groups(struct json_object *jroot)
{
	ufprog_status ret, cbret = UFP_OK;
	struct json_object *jeg;

	ret = json_read_obj(jroot, "erase-groups", &jeg);
	if (ret) {
		if (ret == UFP_NOT_EXIST)
			return UFP_OK;

		logm_err("Invalid type of erase group list\n");
		return ret;
	}

	ret = json_obj_foreach(jeg, NULL, spi_nor_ext_erase_group_cb, &cbret, NULL);
	if (ret) {
		logm_err("Invalid type of /%s\n", "erase-groups");
		return ret;
	}

	if (!cbret)
		return UFP_OK;

	return UFP_FAIL;
}

static int UFPROG_API spi_nor_reset_ext_erase_group(void *priv, struct ufprog_lookup_table *tbl, const char *key,
						    void *ptr)
{
	lookup_table_delete(tbl, key);

	free(ptr);

	return 0;
}

static void spi_nor_reset_ext_erase_groups(void)
{
	lookup_table_enum(ext_erase_info_list, spi_nor_reset_ext_erase_group, NULL);
}

static ufprog_status spi_nor_parse_ext_io_opcodes(struct json_object *jopcode, struct spi_nor_io_opcode *ret_opcodes,
						  uint32_t *out_iocaps, const char *path)
{
	uint32_t opcode, ndummy, nmode, io_type, io_caps = 0;
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

		ret = json_read_uint32(jitem, "mode-cycles", &nmode, 0);
		if (ret) {
			logm_err("Invalid type of %s/%s/%s\n", path, name, "mode-cycles");
			return ret;
		}

		if (ndummy + nmode > 0xff) {
			logm_err("The sum of dummy-cycles + mode-cycles in %s/%s is too big\n", path, name);
			return ret;
		}

		ret_opcodes[io_type].ndummy = (uint8_t)ndummy;
		ret_opcodes[io_type].nmode = (uint8_t)nmode;

		io_caps |= BIT(io_type);
	}

	if (out_iocaps)
		*out_iocaps = io_caps;

	return UFP_OK;
}

static int UFPROG_API spi_nor_ext_io_opcodes_cb(void *priv, const char *key, struct json_object *jopcode)
{
	struct spi_nor_io_opcode *opcodes = NULL, tmp_opcodes[__SPI_MEM_IO_MAX];
	ufprog_status ret, *cbret = priv;
	char *path;

	if (spi_nor_io_opcodes_find(key)) {
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

	ret = spi_nor_parse_ext_io_opcodes(jopcode, tmp_opcodes, NULL, path);
	if (ret) {
		logm_err("Failed to parse %s'\n", path);
		*cbret = ret;
		goto cleanup;
	}

	opcodes = malloc(sizeof(*opcodes) * __SPI_MEM_IO_MAX);
	if (!opcodes) {
		logm_err("No memory for I/O opcode group '%s'\n", key);
		*cbret = UFP_NOMEM;
		goto cleanup;
	}

	memcpy(opcodes, tmp_opcodes, sizeof(*opcodes) * __SPI_MEM_IO_MAX);

	ret = lookup_table_insert(ext_io_opcodes_list, key, opcodes);
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

static ufprog_status spi_nor_load_ext_io_opcodes(struct json_object *jroot)
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

	ret = json_obj_foreach(jopcodes, NULL, spi_nor_ext_io_opcodes_cb, &cbret, NULL);
	if (ret) {
		logm_err("Invalid type of /%s\n", "io-opcodes");
		return ret;
	}

	if (!cbret)
		return UFP_OK;

	return UFP_FAIL;
}

static int UFPROG_API spi_nor_reset_ext_io_opcode(void *priv, struct ufprog_lookup_table *tbl, const char *key,
						  void *ptr)
{
	lookup_table_delete(tbl, key);

	free(ptr);

	return 0;
}

static void spi_nor_reset_ext_io_opcodes(void)
{
	lookup_table_enum(ext_io_opcodes_list, spi_nor_reset_ext_io_opcode, NULL);
}

static ufprog_status spi_nor_ext_ids_init_lists(void)
{
	ufprog_status ret;

	if (!ext_erase_info_list) {
		ret = lookup_table_create(&ext_erase_info_list, 0);
		if (ret) {
			logm_err("No memory for external erase information list\n");
			return ret;
		}
	}

	if (!ext_io_opcodes_list) {
		ret = lookup_table_create(&ext_io_opcodes_list, 0);
		if (ret) {
			logm_err("No memory for external I/O opcodes list\n");
			return ret;
		}
	}

	return UFP_OK;
}

static void spi_nor_reset_ext_id_list(void)
{
	spi_nor_reset_ext_vendors(spi_nor_reset_ext_vendor);
	spi_nor_reset_ext_io_opcodes();
	spi_nor_reset_ext_erase_groups();
}

ufprog_status spi_nor_load_ext_id_list(void)
{
	struct json_object *jroot;
	ufprog_status ret;

	spi_nor_reset_ext_id_list();

	STATUS_CHECK_RET(spi_nor_ext_ids_init_lists());

	ret = json_open_config("spi-nor-ids", &jroot);
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

	STATUS_CHECK_GOTO_RET(spi_nor_load_ext_io_opcodes(jroot), ret, cleanup);
	STATUS_CHECK_GOTO_RET(spi_nor_load_ext_erase_groups(jroot), ret, cleanup);
	STATUS_CHECK_GOTO_RET(spi_nor_load_ext_vendors(jroot), ret, cleanup);

	logm_notice("Successfully loaded external flash table\n");

	ret = UFP_OK;

cleanup:
	json_free(jroot);

	if (ret)
		spi_nor_reset_ext_id_list();

	return ret;
}
