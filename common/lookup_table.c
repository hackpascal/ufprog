/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Lookup table implementation
 */

#include <stdio.h>
#include <stdbool.h>
#include <ufprog/lookup_table.h>
#include <json-c/linkhash.h>

#define LOOKTABLE_DEFAULT_INIT_SIZE		10

ufprog_status UFPROG_API lookup_table_create(struct ufprog_lookup_table **outtbl, uint32_t init_size)
{
	struct lh_table *lht;

	if (!outtbl)
		return UFP_INVALID_PARAMETER;

	*outtbl = NULL;

	if (!init_size)
		init_size = LOOKTABLE_DEFAULT_INIT_SIZE;

	lht = lh_kchar_table_new(init_size, NULL);
	if (!lht)
		return UFP_NOMEM;

	*outtbl = (void *)lht;

	return UFP_OK;
}

ufprog_status UFPROG_API lookup_table_destroy(struct ufprog_lookup_table *tbl)
{
	if (!tbl)
		return UFP_INVALID_PARAMETER;

	lh_table_free((void *)tbl);

	return UFP_OK;
}

ufprog_status UFPROG_API lookup_table_insert(struct ufprog_lookup_table *tbl, const char *key, const void *ptr)
{
	int ret;

	if (!tbl || !key)
		return UFP_INVALID_PARAMETER;

	ret = lh_table_insert((void *)tbl, key, ptr);
	if (!ret)
		return UFP_OK;

	return UFP_NOMEM;
}

ufprog_status UFPROG_API lookup_table_insert_ptr(struct ufprog_lookup_table *tbl, const void *ptr)
{
	char key[20];

	if (!tbl || !ptr)
		return UFP_INVALID_PARAMETER;

	snprintf(key, sizeof(key), "%p", ptr);

	return lookup_table_insert(tbl, key, ptr);
}

ufprog_status UFPROG_API lookup_table_delete(struct ufprog_lookup_table *tbl, const char *key)
{
	int ret;

	if (!tbl || !key)
		return UFP_INVALID_PARAMETER;

	ret = lh_table_delete((void *)tbl, key);
	if (!ret)
		return UFP_OK;

	return UFP_FAIL;
}

ufprog_status UFPROG_API lookup_table_delete_ptr(struct ufprog_lookup_table *tbl, const void *ptr)
{
	char key[20];

	if (!tbl || !ptr)
		return UFP_INVALID_PARAMETER;

	snprintf(key, sizeof(key), "%p", ptr);

	return lookup_table_delete(tbl, key);
}

ufprog_bool UFPROG_API lookup_table_find(struct ufprog_lookup_table *tbl, const char *key, void **retptr)
{
	struct lh_entry *e;

	if (!tbl || !key)
		return false;

	if (retptr)
		*retptr = NULL;

	e = lh_table_lookup_entry((void *)tbl, key);
	if (!e)
		return false;

	if (retptr)
		*retptr = lh_entry_v(e);

	return true;
}

uint32_t UFPROG_API lookup_table_length(struct ufprog_lookup_table *tbl)
{
	int ret;

	if (!tbl)
		return UFP_INVALID_PARAMETER;

	ret = lh_table_length((void *)tbl);
	if (ret < 0)
		return 0;

	return ret;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4706)
#endif /* _MSC_VER */

ufprog_status UFPROG_API lookup_table_enum(struct ufprog_lookup_table *tbl, ufprog_lookup_table_entry_cb cb, void *priv)
{
	struct lh_entry *e, *tmp;
	struct lh_table *lht = (struct lh_table *)tbl;
	int ret;

	if (!tbl || !cb)
		return UFP_INVALID_PARAMETER;

	lh_foreach_safe(lht, e, tmp) {
		ret = cb(priv, tbl, lh_entry_k(e), lh_entry_v(e));
		if (ret)
			break;
	}

	return UFP_OK;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif /* _MSC_VER */
