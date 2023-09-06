/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ECC driver interface definitions
 */
#pragma once

#ifndef _UFPROG_API_ECC_H_
#define _UFPROG_API_ECC_H_

#include <ufprog/bits.h>
#include <ufprog/config.h>
#include <ufprog/api_plugin.h>

EXTERN_C_BEGIN

#define ECC_DRIVER_API_VERSION_MAJOR		1
#define ECC_DRIVER_API_VERSION_MINOR		0

#define NAND_BBM_MAX_NUM			4
#define NAND_BBM_MAX_PAGES			8

/* Merge the BBM page config from NAND chip */
#define ECC_F_BBM_MERGE_PAGE			BIT(0)

/* Mark whole bad page with 00h */
#define ECC_F_BBM_MARK_WHOLE_PAGE		BIT(1)

/* Use raw read/write on the page */
#define ECC_F_BBM_RAW				BIT(2)

/* Use canonical layout on the page */
#define ECC_F_BBM_CANONICAL_LAYOUT		BIT(3)

struct ufprog_ecc_instance;

enum nand_ecc_type {
	NAND_ECC_NONE,
	NAND_ECC_ON_DIE,
	NAND_ECC_EXTERNAL,

	__NAND_ECC_TYPE_MAX
};

enum nand_ecc_page_byte_type {
	NAND_PAGE_BYTE_UNUSED,
	NAND_PAGE_BYTE_DATA,
	NAND_PAGE_BYTE_OOB_DATA,
	NAND_PAGE_BYTE_OOB_FREE,
	NAND_PAGE_BYTE_ECC_PARITY,
	NAND_PAGE_BYTE_MARKER,

	__MAX_NAND_PAGE_BYTE_TYPE
};

struct nand_page_layout_entry {
	uint32_t num;
	uint32_t /* enum nand_ecc_page_byte_type */ type;
};

struct nand_page_layout {
	uint32_t count;
	struct nand_page_layout_entry entries[];
};

struct nand_ecc_config {
	uint16_t step_size; /* Not including OOB */
	uint16_t strength_per_step;
};

struct nand_ecc_status {
	ufprog_bool per_step;
	int step_bitflips[];
};

struct nand_bbm_page_cfg {
	uint32_t idx[NAND_BBM_MAX_PAGES];
	uint32_t num;
};

struct nand_bbm_check_cfg {
	uint32_t pos[NAND_BBM_MAX_NUM];
	uint16_t num;
	uint16_t width;
};

struct nand_bbm_mark_cfg {
	uint32_t pos[NAND_BBM_MAX_NUM];
	uint16_t num;
	uint16_t bytes;
};

struct nand_bbm_config {
	struct nand_bbm_page_cfg pages;
	struct nand_bbm_check_cfg check;
	struct nand_bbm_mark_cfg mark;
	uint32_t flags;
};

#define API_NAME_ECC_CREATE_INSTANCE		"ufprog_ecc_create_instance"
typedef ufprog_status (UFPROG_API *api_ecc_create_instance)(uint32_t page_size, uint32_t spare_size,
							    struct json_object *config,
							    struct ufprog_ecc_instance **outinst);

#define API_NAME_ECC_FREE_INSTANCE		"ufprog_ecc_free_instance"
typedef ufprog_status (UFPROG_API *api_ecc_free_instance)(struct ufprog_ecc_instance *inst);

#define API_NAME_ECC_GET_CONFIG			"ufprog_ecc_chip_get_config"
typedef ufprog_status (UFPROG_API *api_ecc_get_config)(struct ufprog_ecc_instance *inst,
						       struct nand_ecc_config *ret_ecccfg);

#define API_NAME_ECC_GET_BBM_CONFIG		"ufprog_ecc_chip_get_bbm_config"
typedef ufprog_status (UFPROG_API *api_ecc_get_bbm_config)(struct ufprog_ecc_instance *inst,
							   struct nand_bbm_config *ret_bbmcfg);

#define API_NAME_ECC_ENCODE_PAGE		"ufprog_ecc_chip_encode_page"
typedef ufprog_status (UFPROG_API *api_ecc_encode_page)(struct ufprog_ecc_instance *inst, void *page);

#define API_NAME_ECC_DECODE_PAGE		"ufprog_ecc_chip_decode_page"
typedef ufprog_status (UFPROG_API *api_ecc_decode_page)(struct ufprog_ecc_instance *inst, void *page);

#define API_NAME_ECC_GET_STATUS			"ufprog_ecc_chip_get_status"
typedef const struct nand_ecc_status *(UFPROG_API *api_ecc_get_status)(struct ufprog_ecc_instance *inst);

#define API_NAME_ECC_GET_PAGE_LAYOUT		"ufprog_ecc_chip_get_page_layout"
typedef const struct nand_page_layout *(UFPROG_API *api_ecc_get_page_layout)(struct ufprog_ecc_instance *inst,
									     ufprog_bool canonical);

#define API_NAME_ECC_CONVERT_PAGE_LAYOUT	"ufprog_ecc_chip_convert_page_layout"
typedef ufprog_status (UFPROG_API *api_ecc_convert_page_layout)(struct ufprog_ecc_instance *inst, const void *src,
								void *out, ufprog_bool from_canonical);

EXTERN_C_END

#endif /* _UFPROG_API_ECC_H_ */
