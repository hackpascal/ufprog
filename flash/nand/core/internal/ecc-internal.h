/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ECC internal definitions
 */
#pragma once

#ifndef _UFPROG_NAND_ECC_INTERNAL_H_
#define _UFPROG_NAND_ECC_INTERNAL_H_

#include <stdbool.h>
#include <ufprog/ecc-driver.h>
#include <ufprog/ecc.h>
#include <plugin-common.h>

#define ECC_PAGE_BYTES(_type, _num)		{ .num = (_num), .type = (_type) }
#define ECC_PAGE_UNUSED_BYTES(_num)		ECC_PAGE_BYTES(NAND_PAGE_BYTE_UNUSED, _num)
#define ECC_PAGE_DATA_BYTES(_num)		ECC_PAGE_BYTES(NAND_PAGE_BYTE_DATA, _num)
#define ECC_PAGE_OOB_DATA_BYTES(_num)		ECC_PAGE_BYTES(NAND_PAGE_BYTE_OOB_DATA, _num)
#define ECC_PAGE_OOB_FREE_BYTES(_num)		ECC_PAGE_BYTES(NAND_PAGE_BYTE_OOB_FREE, _num)
#define ECC_PAGE_PARITY_BYTES(_num)		ECC_PAGE_BYTES(NAND_PAGE_BYTE_ECC_PARITY, _num)
#define ECC_PAGE_MARKER_BYTES(_num)		ECC_PAGE_BYTES(NAND_PAGE_BYTE_MARKER, _num)

#define ECC_PAGE_LAYOUT(...)	\
	{ .count = sizeof((struct nand_page_layout_entry[]){ __VA_ARGS__ }) / sizeof(struct nand_page_layout_entry), \
	  .entries = { __VA_ARGS__ } }

struct ufprog_ecc_driver {
	struct plugin plugin;

	api_ecc_create_instance create_instance;
	api_ecc_free_instance free_instance;

	api_ecc_get_config get_config;
	api_ecc_get_bbm_config get_bbm_config;

	api_ecc_encode_page encode_page;
	api_ecc_decode_page decode_page;
	api_ecc_get_status get_status;

	api_ecc_get_page_layout get_page_layout;
	api_ecc_convert_page_layout convert_page_layout;

	struct ufprog_lookup_table *instances;
};

struct ufprog_nand_ecc_chip {
	enum nand_ecc_type type;
	char *name;

	struct ufprog_ecc_driver *driver;
	struct ufprog_ecc_instance *instance;

	struct nand_ecc_config config;
	struct nand_bbm_config bbm_config;

	const struct nand_page_layout *page_layout;
	const struct nand_page_layout *page_layout_canonical;

	ufprog_status (*free_ni)(struct ufprog_nand_ecc_chip *ecc);

	ufprog_status (*encode_page)(struct ufprog_nand_ecc_chip *ecc, void *page);
	ufprog_status (*decode_page)(struct ufprog_nand_ecc_chip *ecc, void *page);
	const struct nand_ecc_status *(*get_status)(struct ufprog_nand_ecc_chip *ecc);

	ufprog_status (*convert_page_layout)(struct ufprog_nand_ecc_chip *ecc, const void *src, void *out,
					     ufprog_bool from_canonical);

	ufprog_status (*set_enable)(struct ufprog_nand_ecc_chip *ecc, bool enable);
};

ufprog_status ufprog_ecc_add_instance(struct ufprog_ecc_driver *drv, const struct ufprog_ecc_instance *inst);
ufprog_status ufprog_ecc_remove_instance(struct ufprog_ecc_driver *drv, const struct ufprog_ecc_instance *inst);

ufprog_status ufprog_ecc_set_enable(struct ufprog_nand_ecc_chip *ecc, bool enable);

#endif /* _UFPROG_NAND_ECC_INTERNAL_H_ */
