// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * SPI-NOR flash vendor
 */

#define _GNU_SOURCE
#include <malloc.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ufprog/log.h>
#include "vendor.h"

#define SNOR_EXT_VENDOR_INCREMENT			10

static const struct spi_nor_vendor *vendors[] = {
	&vendor_atmel,
	&vendor_eon,
	&vendor_esmt,
	&vendor_gigadevice,
	&vendor_intel,
	&vendor_issi,
	&vendor_issi_pmc, /* PMC comes after ISSI */
	&vendor_macronix,
	&vendor_micron,
	&vendor_spansion,
	&vendor_sst,
	&vendor_winbond,
	&vendor_xmc,
	&vendor_xtx,
};

static struct spi_nor_vendor *ext_vendors;
static uint32_t ext_vendor_capacity;
static uint32_t num_ext_vendors;

ufprog_status spi_nor_vendors_init(void)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(vendors); i++) {
		if (vendors[i]->ops && vendors[i]->ops->init)
			STATUS_CHECK_RET(vendors[i]->ops->init());
	}

	return UFP_OK;
}

const struct spi_nor_vendor *spi_nor_find_vendor(uint8_t mfr_id)
{
	uint32_t i;

	for (i = 0; i < num_ext_vendors; i++) {
		if (ext_vendors[i].mfr_id == mfr_id)
			return &ext_vendors[i];
	}

	for (i = 0; i < ARRAY_SIZE(vendors); i++) {
		if (vendors[i]->mfr_id == mfr_id)
			return vendors[i];
	}

	return NULL;
}

static const struct spi_nor_vendor *spi_nor_find_builtin_vendor_by_id(const char *id)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(vendors); i++) {
		if (!strcasecmp(vendors[i]->id, id))
			return vendors[i];
	}

	return NULL;
}

const struct spi_nor_vendor *spi_nor_find_vendor_by_id(const char *id)
{
	uint32_t i;

	for (i = 0; i < num_ext_vendors; i++) {
		if (ext_vendors[i].id && !strcasecmp(ext_vendors[i].id, id))
			return &ext_vendors[i];
	}

	return spi_nor_find_builtin_vendor_by_id(id);
}

bool spi_nor_find_vendor_part(const uint8_t *id, struct spi_nor_vendor_part *retvp)
{
	const struct spi_nor_flash_part *part;
	uint32_t i;

	retvp->vendor = NULL;
	retvp->vendor_init = NULL;
	retvp->part = NULL;

	for (i = 0; i < ARRAY_SIZE(vendors); i++) {
		part = spi_nor_find_part(vendors[i]->parts, vendors[i]->nparts, id);
		if (part) {
			if (part->display_vendor) {
				retvp->vendor = part->display_vendor;
				retvp->vendor_init = vendors[i];
			} else {
				retvp->vendor = vendors[i];
			}
			retvp->part = part;
			return true;
		}
	}

	for (i = 0; i < num_ext_vendors; i++) {
		part = spi_nor_find_part(ext_vendors[i].parts, ext_vendors[i].nparts, id);
		if (part) {
			if (part->display_vendor) {
				retvp->vendor = part->display_vendor;
				retvp->vendor_init = &ext_vendors[i];
			} else {
				retvp->vendor = &ext_vendors[i];
			}
			retvp->part = part;
			return true;
		}
	}

	return false;
}

bool spi_nor_find_vendor_part_by_name(const char *model, struct spi_nor_vendor_part *retvp)
{
	const struct spi_nor_flash_part *part;
	const struct spi_nor_vendor *vendor;
	uint32_t i;

	retvp->vendor = NULL;
	retvp->vendor_init = NULL;
	retvp->part = NULL;

	for (i = 0; i < ARRAY_SIZE(vendors); i++) {
		part = spi_nor_find_part_by_name(vendors[i]->parts, vendors[i]->nparts, model, &vendor);
		if (part) {
			if (vendor) {
				retvp->vendor = vendor;
				retvp->vendor_init = vendors[i];
			} else  if (part->display_vendor) {
				retvp->vendor = part->display_vendor;
				retvp->vendor_init = vendors[i];
			} else {
				retvp->vendor = vendors[i];
			}
			retvp->part = part;
			return true;
		}
	}

	for (i = 0; i < num_ext_vendors; i++) {
		part = spi_nor_find_part_by_name(ext_vendors[i].parts, ext_vendors[i].nparts, model, &vendor);
		if (part) {
			if (vendor) {
				retvp->vendor = vendor;
				retvp->vendor_init = &ext_vendors[i];
			} else if (part->display_vendor) {
				retvp->vendor = part->display_vendor;
				retvp->vendor_init = &ext_vendors[i];
			} else {
				retvp->vendor = &ext_vendors[i];
			}
			retvp->part = part;
			return true;
		}
	}

	return false;
}

bool spi_nor_vendor_find_part_by_name(const char *model, const struct spi_nor_vendor *vendor,
				      struct spi_nor_vendor_part *retvp)
{
	const struct spi_nor_vendor *alias_vendor, *builtin_vendor;
	const struct spi_nor_flash_part *part;
	uint32_t i;

	retvp->vendor = NULL;
	retvp->vendor_init = NULL;
	retvp->part = NULL;

	if (vendor) {
		part = spi_nor_find_part_by_name(vendor->parts, vendor->nparts, model, &alias_vendor);
		if (part && (!alias_vendor || vendor == alias_vendor)) {
			if (alias_vendor) {
				retvp->vendor = alias_vendor;
				retvp->vendor_init = vendor;
			} else if (part->display_vendor) {
				retvp->vendor = part->display_vendor;
				retvp->vendor_init = vendor;
			} else {
				retvp->vendor = vendor;
			}
			retvp->part = part;
			return true;
		}
	}

	for (i = 0; i < ARRAY_SIZE(vendors); i++) {
		part = spi_nor_find_part_by_name(vendors[i]->parts, vendors[i]->nparts, model, &alias_vendor);
		if (part) {
			if (!vendor) {
				if (alias_vendor) {
					retvp->vendor = alias_vendor;
					retvp->vendor_init = vendors[i];
				} else {
					retvp->vendor = vendors[i];
				}
				retvp->part = part;
				return true;
			}

			if (alias_vendor) {
				builtin_vendor = spi_nor_find_builtin_vendor_by_id(alias_vendor->id);
				if (!builtin_vendor)
					builtin_vendor = spi_nor_find_vendor_by_id(alias_vendor->id);

				if (!strcasecmp(vendor->id, alias_vendor->id)) {
					retvp->vendor = builtin_vendor;
					retvp->vendor_init = vendors[i];
					retvp->part = part;
					return true;
				}
			} else {
				if (part->display_vendor) {
					retvp->vendor = part->display_vendor;
					retvp->vendor_init = vendors[i];
					retvp->part = part;
					return true;
				} else {
					if (!strcasecmp(vendor->id, vendors[i]->id)) {
						retvp->vendor = vendors[i];
						retvp->part = part;
						return true;
					}
				}
			}
		}
	}

	for (i = 0; i < num_ext_vendors; i++) {
		part = spi_nor_find_part_by_name(ext_vendors[i].parts, ext_vendors[i].nparts, model, &alias_vendor);
		if (part) {
			if (!vendor) {
				if (alias_vendor) {
					retvp->vendor = alias_vendor;
					retvp->vendor_init = &ext_vendors[i];
				} else {
					retvp->vendor = &ext_vendors[i];
				}
				retvp->part = part;
				return true;
			}

			if (alias_vendor) {
				builtin_vendor = spi_nor_find_builtin_vendor_by_id(alias_vendor->id);
				if (!builtin_vendor)
					builtin_vendor = spi_nor_find_vendor_by_id(alias_vendor->id);

				if (!strcasecmp(ext_vendors[i].id, alias_vendor->id)) {
					retvp->vendor = builtin_vendor;
					retvp->vendor_init = &ext_vendors[i];
					retvp->part = part;
					return true;
				}
			} else {
				if (part->display_vendor) {
					retvp->vendor = part->display_vendor;
					retvp->vendor_init = &ext_vendors[i];
					retvp->part = part;
					return true;
				} else {
					if (!strcasecmp(vendor->id, ext_vendors[i].id)) {
						retvp->vendor = &ext_vendors[i];
						retvp->part = part;
						return true;
					}
				}
			}
		}
	}

	return false;
}

static int spi_nor_part_item_cmp(void const *a, void const *b)
{
	const struct spi_nor_probe_part *pa = a, *pb = b;

	return strcasecmp(pa->name, pb->name);
}

uint32_t spi_nor_vendor_list_parts(const struct spi_nor_vendor *vendor, const char *match_part,
				   const struct spi_nor_id *match_id, struct spi_nor_probe_part *list, bool no_meta)
{
	const struct spi_nor_flash_part *part;
	uint32_t i, j, count = 0;

	for (i = 0; i < vendor->nparts; i++) {
		part = &vendor->parts[i];

		if (match_id) {
			if (match_id->len != part->id.len ||
			    !spi_nor_id_match(match_id->id, part->id.id, part->id_mask, part->id.len))
				continue;
		}

		if (match_part && *match_part) {
			if (!strcasestr(part->model, match_part))
				continue;
		}

		if (part->flags & SNOR_F_NO_OP)
			continue;

		if (!no_meta || (no_meta && !(part->flags & SNOR_F_META))) {
			if (list) {
				if (part->display_vendor)
					list[count].vendor = part->display_vendor->name;
				else
					list[count].vendor = vendor->name;
				list[count].name = part->model;
			}

			count++;
		}

		if (part->alias && part->alias->num) {
			for (j = 0; j < part->alias->num; j++) {
				if (list) {
					if (part->alias->items[j].vendor)
						list[count].vendor = part->alias->items[j].vendor->name;
					else
						list[count].vendor = vendor->name;
					list[count].name = part->alias->items[j].model;
				}

				count++;
			}
		}
	}

	if (list)
		qsort(list, count, sizeof(*list), spi_nor_part_item_cmp);

	return count;
}

uint32_t spi_nor_list_parts(const char *vendorid, const char *match_part, const struct spi_nor_id *match_id,
			    struct spi_nor_probe_part *list)
{
	const struct spi_nor_vendor *vendor, *builtin_vendor;
	uint32_t i, n, count = 0;

	if (vendorid && *vendorid) {
		vendor = spi_nor_find_vendor_by_id(vendorid);
		if (!vendor)
			return 0;

		n = spi_nor_vendor_list_parts(vendor, match_part, match_id, list, true);

		if (list)
			list += n;

		count += n;

		builtin_vendor = spi_nor_find_builtin_vendor_by_id(vendorid);
		if (builtin_vendor && builtin_vendor != vendor) {
			n = spi_nor_vendor_list_parts(builtin_vendor, match_part, match_id, list, true);

			if (list)
				list += n;

			count += n;
		}

		return count;
	}

	for (i = 0; i < ARRAY_SIZE(vendors); i++) {
		n = spi_nor_vendor_list_parts(vendors[i], match_part, match_id, list, true);

		if (list)
			list += n;

		count += n;
	}

	for (i = 0; i < num_ext_vendors; i++) {
		n = spi_nor_vendor_list_parts(&ext_vendors[i], match_part, match_id, list, true);

		if (list)
			list += n;

		count += n;
	}

	return count;
}

bool spi_nor_set_ext_vendor_capacity(uint32_t n)
{
	struct spi_nor_vendor *newptr;

	if (n <= ext_vendor_capacity)
		return true;

	if (!ext_vendor_capacity)
		newptr = malloc(n * sizeof(struct spi_nor_vendor));
	else
		newptr = realloc(ext_vendors, n * sizeof(struct spi_nor_vendor));

	if (!newptr) {
		logm_err("No memory for external vendor list\n");
		return false;
	}

	ext_vendors = newptr;
	ext_vendor_capacity = n;

	return true;
}

struct spi_nor_vendor *spi_nor_alloc_ext_vendor(void)
{
	struct spi_nor_vendor *vendor;

	if (num_ext_vendors == ext_vendor_capacity) {
		if (!spi_nor_set_ext_vendor_capacity(ext_vendor_capacity + SNOR_EXT_VENDOR_INCREMENT))
			return NULL;
	}

	if (num_ext_vendors < ext_vendor_capacity) {
		vendor = &ext_vendors[num_ext_vendors];
		num_ext_vendors++;

		memset(vendor, 0, sizeof(struct spi_nor_vendor));

		return vendor;
	}

	return NULL;
}

bool spi_nor_is_ext_vendor(const struct spi_nor_vendor *vendor)
{
	return vendor >= ext_vendors && vendor < ext_vendors + num_ext_vendors;
}

void spi_nor_reset_ext_vendors(spi_nor_reset_ext_vendor_cb cb)
{
	uint32_t i;

	if (ext_vendors) {
		for (i = 0; i < num_ext_vendors; i++) {
			if (cb)
				cb(&ext_vendors[i]);

			if (ext_vendors[i].id)
				free((void *)ext_vendors[i].id);

			if (ext_vendors[i].name)
				free((void *)ext_vendors[i].name);
		}

		memset(ext_vendors, 0, ext_vendor_capacity * sizeof(struct spi_nor_vendor));
	}

	num_ext_vendors = 0;
}

static int spi_nor_vendor_item_cmp(void const *a, void const *b)
{
	const struct spi_nor_vendor_item *va = a, *vb = b;

	return strcasecmp(va->name, vb->name);
}

ufprog_status UFPROG_API ufprog_spi_nor_list_vendors(struct spi_nor_vendor_item **outlist, uint32_t *retcount)
{
	struct spi_nor_vendor_item *list;
	uint32_t i, n = 0;

	if (!outlist)
		return UFP_INVALID_PARAMETER;

	list = malloc((num_ext_vendors + ARRAY_SIZE(vendors)) * sizeof(*list));
	if (!list) {
		logm_err("No memory for flash vendor list\n");
		return UFP_NOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(vendors); i++) {
		list[n].id = vendors[i]->id;
		list[n].name = vendors[i]->name;
		n++;
	}

	for (i = 0; i < num_ext_vendors; i++) {
		if (spi_nor_find_builtin_vendor_by_id(ext_vendors[i].id))
			continue;

		list[n].id = ext_vendors[i].id;
		list[n].name = ext_vendors[i].name;
		n++;
	}

	qsort(list, n, sizeof(*list), spi_nor_vendor_item_cmp);

	*outlist = list;
	*retcount = n;

	return UFP_OK;
}
