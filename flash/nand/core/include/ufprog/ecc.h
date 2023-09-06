/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ECC chip management
 */
#pragma once

#ifndef _UFPROG_ECC_H_
#define _UFPROG_ECC_H_

#include <ufprog/api_ecc.h>
#include <ufprog/config.h>

EXTERN_C_BEGIN

struct ufprog_nand_ecc_chip;

ufprog_status UFPROG_API ufprog_ecc_open_chip(const char *drvname, const char *name, uint32_t page_size,
					      uint32_t spare_size, struct json_object *config,
					      struct ufprog_nand_ecc_chip **outecc);
ufprog_status UFPROG_API ufprog_ecc_free_chip(struct ufprog_nand_ecc_chip *ecc);

const char *UFPROG_API ufprog_ecc_chip_name(struct ufprog_nand_ecc_chip *ecc);
uint32_t /* enum nand_ecc_type */ UFPROG_API ufprog_ecc_chip_type(struct ufprog_nand_ecc_chip *ecc);
const char *UFPROG_API ufprog_ecc_chip_type_name(struct ufprog_nand_ecc_chip *ecc);

ufprog_status UFPROG_API ufprog_ecc_get_config(struct ufprog_nand_ecc_chip *ecc, struct nand_ecc_config *ret_ecccfg);
ufprog_status UFPROG_API ufprog_ecc_get_bbm_config(struct ufprog_nand_ecc_chip *ecc,
						   struct nand_bbm_config *ret_bbmcfg);

ufprog_bool UFPROG_API ufprog_ecc_support_convert_page_layout(struct ufprog_nand_ecc_chip *ecc);
const struct nand_page_layout *UFPROG_API ufprog_ecc_get_page_layout(struct ufprog_nand_ecc_chip *ecc,
								     ufprog_bool canonical);
ufprog_status UFPROG_API ufprog_ecc_convert_page_layout(struct ufprog_nand_ecc_chip *ecc, const void *src, void *out,
							ufprog_bool from_canonical);

ufprog_status UFPROG_API ufprog_ecc_encode_page(struct ufprog_nand_ecc_chip *ecc, void *page);
ufprog_status UFPROG_API ufprog_ecc_decode_page(struct ufprog_nand_ecc_chip *ecc, void *page);
const struct nand_ecc_status *UFPROG_API ufprog_ecc_get_status(struct ufprog_nand_ecc_chip *ecc);

ufprog_bool UFPROG_API ufprog_ecc_bbm_add_page(struct nand_bbm_page_cfg *cfg, uint32_t page);
ufprog_bool UFPROG_API ufprog_ecc_bbm_add_check_pos(struct nand_bbm_check_cfg *cfg, uint32_t pos);
ufprog_bool UFPROG_API ufprog_ecc_bbm_add_mark_pos(struct nand_bbm_mark_cfg *cfg, uint32_t pos);

EXTERN_C_END

#endif /* _UFPROG_ECC_DRIVER_H_ */
