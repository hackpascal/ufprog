// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * ECC chip management
 */

#include <string.h>
#include <ufprog/log.h>
#include "internal/ecc-internal.h"

ufprog_status UFPROG_API ufprog_ecc_open_chip(const char *drvname, const char *name, uint32_t page_size,
					      uint32_t spare_size, struct json_object *config,
					      struct ufprog_nand_ecc_chip **outecc)
{
	struct ufprog_ecc_instance *eccinst;
	struct ufprog_nand_ecc_chip *ecc;
	struct ufprog_ecc_driver *drv;
	ufprog_status ret;
	size_t namelen;

	if (!drvname || !outecc || !page_size || !spare_size)
		return UFP_INVALID_PARAMETER;

	ret = ufprog_load_ecc_driver(drvname, &drv);
	if (ret)
		return ret;

	ret = drv->create_instance(page_size, spare_size, config, &eccinst);
	if (ret)
		goto cleanup_unload_driver;

	ret = ufprog_ecc_add_instance(drv, eccinst);
	if (ret)
		goto cleanup_free_instance;

	if (!name)
		name = drvname;

	namelen = strlen(name);

	ecc = calloc(1, sizeof(*ecc) + namelen + 1);
	if (!ecc) {
		log_err("No memory for ECC chip\n");
		ret = UFP_NOMEM;
		goto cleanup_remove_instance;
	}

	ecc->type = NAND_ECC_EXTERNAL;
	ecc->name = (char *)ecc + sizeof(*ecc);
	memcpy(ecc->name, name, namelen + 1);

	ecc->driver = drv;
	ecc->instance = eccinst;

	ret = drv->get_config(eccinst, &ecc->config);
	if (ret) {
		log_err("Failed to get ECC configuration\n");
		goto cleanup_free_ecc_chip;
	}

	*outecc = ecc;
	return UFP_OK;

cleanup_free_ecc_chip:
	free(ecc);

cleanup_remove_instance:
	ufprog_ecc_remove_instance(drv, eccinst);

cleanup_free_instance:
	drv->free_instance(eccinst);

cleanup_unload_driver:
	ufprog_unload_ecc_driver(drv);

	return ret;
}

ufprog_status UFPROG_API ufprog_ecc_free_chip(struct ufprog_nand_ecc_chip *ecc)
{
	if (!ecc)
		return UFP_INVALID_PARAMETER;

	if (ecc->driver && ecc->instance) {
		ufprog_ecc_remove_instance(ecc->driver, ecc->instance);
		ecc->driver->free_instance(ecc->instance);
		ufprog_unload_ecc_driver(ecc->driver);
	} else {
		if (ecc->free_ni)
			return ecc->free_ni(ecc);
	}

	free(ecc);

	return UFP_OK;
}

const char *UFPROG_API ufprog_ecc_chip_name(struct ufprog_nand_ecc_chip *ecc)
{
	if (!ecc)
		return NULL;

	return ecc->name;
}

uint32_t UFPROG_API ufprog_ecc_chip_type(struct ufprog_nand_ecc_chip *ecc)
{
	if (!ecc)
		return NAND_ECC_NONE;

	return ecc->type;
}

const char *UFPROG_API ufprog_ecc_chip_type_name(struct ufprog_nand_ecc_chip *ecc)
{
	switch (ecc->type) {
	case NAND_ECC_NONE:
		return "None";
	case NAND_ECC_ON_DIE:
		return "On-Die";
	case NAND_ECC_EXTERNAL:
		return "External";
	default:
		return "Unknown";
	}
}

ufprog_status UFPROG_API ufprog_ecc_get_config(struct ufprog_nand_ecc_chip *ecc, struct nand_ecc_config *ret_ecccfg)
{
	if (!ecc || !ret_ecccfg)
		return UFP_INVALID_PARAMETER;

	if (ecc->driver && ecc->instance)
		return ecc->driver->get_config(ecc->instance, ret_ecccfg);

	memcpy(ret_ecccfg, &ecc->config, sizeof(ecc->config));

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_ecc_get_bbm_config(struct ufprog_nand_ecc_chip *ecc,
						   struct nand_bbm_config *ret_bbmcfg)
{
	ufprog_status ret;

	if (!ecc || !ret_bbmcfg)
		return UFP_INVALID_PARAMETER;

	if (ecc->driver && ecc->instance) {
		if (ecc->driver->get_bbm_config) {
			ret = ecc->driver->get_bbm_config(ecc->instance, ret_bbmcfg);
			if (ret)
				return ret;
		} else {
			memset(ret_bbmcfg, 0, sizeof(ecc->bbm_config));
		}
	} else {
		memcpy(ret_bbmcfg, &ecc->bbm_config, sizeof(ecc->bbm_config));
	}

	return UFP_OK;
}

ufprog_bool UFPROG_API ufprog_ecc_support_convert_page_layout(struct ufprog_nand_ecc_chip *ecc)
{
	if (!ecc)
		return UFP_INVALID_PARAMETER;

	if (ecc->driver && ecc->instance)
		return !!ecc->driver->convert_page_layout;

	return !!ecc->convert_page_layout;
}

const struct nand_page_layout *UFPROG_API ufprog_ecc_get_page_layout(struct ufprog_nand_ecc_chip *ecc,
								     ufprog_bool canonical)
{
	if (!ecc)
		return NULL;

	if (ecc->driver && ecc->instance)
		return ecc->driver->get_page_layout(ecc->instance, canonical);

	return canonical ? ecc->page_layout_canonical : ecc->page_layout;
}

ufprog_status UFPROG_API ufprog_ecc_convert_page_layout(struct ufprog_nand_ecc_chip *ecc, const void *src, void *out,
							ufprog_bool from_canonical)
{
	if (!ecc || !src || !out)
		return UFP_INVALID_PARAMETER;

	if (ecc->driver && ecc->instance) {
		if (!ecc->driver->convert_page_layout)
			return UFP_UNSUPPORTED;

		return ecc->driver->convert_page_layout(ecc->instance, src, out, from_canonical);
	}

	if (!ecc->convert_page_layout)
		return UFP_UNSUPPORTED;

	return ecc->convert_page_layout(ecc, src, out, from_canonical);
}

ufprog_status UFPROG_API ufprog_ecc_encode_page(struct ufprog_nand_ecc_chip *ecc, void *page)
{
	if (!ecc || !page)
		return UFP_INVALID_PARAMETER;

	if (ecc->driver && ecc->instance)
		return ecc->driver->encode_page(ecc->instance, page);

	if (ecc->encode_page)
		return ecc->encode_page(ecc, page);

	return UFP_OK;
}

ufprog_status UFPROG_API ufprog_ecc_decode_page(struct ufprog_nand_ecc_chip *ecc, void *page)
{
	if (!ecc || !page)
		return UFP_INVALID_PARAMETER;

	if (ecc->driver && ecc->instance)
		return ecc->driver->decode_page(ecc->instance, page);

	if (ecc->decode_page)
		return ecc->decode_page(ecc, page);

	return UFP_OK;
}

const struct nand_ecc_status *UFPROG_API ufprog_ecc_get_status(struct ufprog_nand_ecc_chip *ecc)
{
	if (!ecc)
		return NULL;

	if (ecc->driver && ecc->instance)
		return ecc->driver->get_status(ecc->instance);

	if (ecc->get_status)
		return ecc->get_status(ecc);

	return NULL;
}

ufprog_status ufprog_ecc_set_enable(struct ufprog_nand_ecc_chip *ecc, bool enable)
{
	if (!ecc)
		return UFP_INVALID_PARAMETER;

	if (ecc->set_enable)
		return ecc->set_enable(ecc, enable);

	return UFP_OK;
}

ufprog_bool UFPROG_API ufprog_ecc_bbm_add_page(struct nand_bbm_page_cfg *cfg, uint32_t page)
{
	uint32_t i;

	if (!cfg)
		return UFP_INVALID_PARAMETER;

	for (i = 0; i < cfg->num; i++) {
		if (cfg->idx[i] == page)
			return UFP_OK;
	}

	if (cfg->num >= NAND_BBM_MAX_PAGES)
		return UFP_FAIL;

	cfg->idx[cfg->num++] = page;

	return UFP_OK;
}

ufprog_bool UFPROG_API ufprog_ecc_bbm_add_check_pos(struct nand_bbm_check_cfg *cfg, uint32_t pos)
{
	uint32_t i;

	if (!cfg)
		return UFP_INVALID_PARAMETER;

	for (i = 0; i < cfg->num; i++) {
		if (cfg->pos[i] == pos)
			return UFP_OK;
	}

	if (cfg->num >= NAND_BBM_MAX_NUM)
		return UFP_FAIL;

	cfg->pos[cfg->num++] = pos;

	return UFP_OK;
}

ufprog_bool UFPROG_API ufprog_ecc_bbm_add_mark_pos(struct nand_bbm_mark_cfg *cfg, uint32_t pos)
{
	uint32_t i;

	if (!cfg)
		return UFP_INVALID_PARAMETER;

	for (i = 0; i < cfg->num; i++) {
		if (cfg->pos[i] == pos)
			return UFP_OK;
	}

	if (cfg->num >= NAND_BBM_MAX_NUM)
		return UFP_FAIL;

	cfg->pos[cfg->num++] = pos;

	return UFP_OK;
}
