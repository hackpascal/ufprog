/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Configuration public API header
 */
#pragma once

#ifndef _UFPROG_CONFIG_H_
#define _UFPROG_CONFIG_H_

#include <ufprog/osdef.h>

EXTERN_C_BEGIN

struct json_object;

ufprog_status UFPROG_API json_from_str(const char *str, struct json_object **outjroot);
ufprog_status UFPROG_API json_from_file(const char *file, struct json_object **outjroot);
ufprog_status UFPROG_API json_to_str(struct json_object *jroot, char **outstr);
ufprog_status UFPROG_API json_to_file(struct json_object *jroot, const char *file, ufprog_bool create);
ufprog_status UFPROG_API json_free(struct json_object *jroot);

ufprog_status UFPROG_API json_read_str(struct json_object *jparent, const char *name, const char **retstr,
				       const char *dflval);
ufprog_status UFPROG_API json_read_bool(struct json_object *jparent, const char *name, ufprog_bool *retval);
ufprog_status UFPROG_API json_read_int64(struct json_object *jparent, const char *name, int64_t *retval,
					 int64_t dflval);
ufprog_status UFPROG_API json_read_uint64(struct json_object *jparent, const char *name, uint64_t *retval,
					  uint64_t dflval);
ufprog_status UFPROG_API json_read_int32(struct json_object *jparent, const char *name, int32_t *retval,
					 int32_t dflval);
ufprog_status UFPROG_API json_read_hex64(struct json_object *jparent, const char *name, uint64_t *retval,
					 uint64_t dflval);

ufprog_status UFPROG_API json_read_obj(struct json_object *jparent, const char *name, struct json_object **retjobj);
ufprog_status UFPROG_API json_read_array(struct json_object *jparent, const char *name, struct json_object **retjarr);

static inline ufprog_status json_read_uint32(struct json_object *jparent, const char *name, uint32_t *retval,
					     uint32_t dflval)
{
	return json_read_int32(jparent, name, (int32_t *)retval, dflval);
}

static inline ufprog_status json_read_hex32(struct json_object *jparent, const char *name, uint32_t *retval,
					    uint32_t dflval)
{
	ufprog_status ret;
	uint64_t val;

	ret = json_read_hex64(jparent, name, &val, dflval);
	if (ret)
		val = 0;

	*retval = (uint32_t)val;
	return ret;
}

size_t UFPROG_API json_obj_len(struct json_object *jobj);
size_t UFPROG_API json_array_len(struct json_object *jarr);
ufprog_status UFPROG_API json_array_read_str(struct json_object *jarr, size_t idx, const char **retstr,
					     const char *dflval);
ufprog_status UFPROG_API json_array_read_bool(struct json_object *jarr, size_t idx, ufprog_bool *retval);
ufprog_status UFPROG_API json_array_read_int64(struct json_object *jarr, size_t idx, int64_t *retval, int64_t dflval);
ufprog_status UFPROG_API json_array_read_uint64(struct json_object *jarr, size_t idx, uint64_t *retval,
						uint64_t dflval);
ufprog_status UFPROG_API json_array_read_int32(struct json_object *jarr, size_t idx, int32_t *retval, int32_t dflval);
ufprog_status UFPROG_API json_array_read_hex64(struct json_object *jarr, size_t idx, uint64_t *retval, uint64_t dflval);
ufprog_status UFPROG_API json_array_read_obj(struct json_object *jarr, size_t idx, struct json_object **retjobj);
ufprog_status UFPROG_API json_array_read_array(struct json_object *jarr, size_t idx, struct json_object **retjarr);

static inline ufprog_status json_array_read_uint32(struct json_object *jarr, size_t idx, uint32_t *retval,
						   uint32_t dflval)
{
	return json_array_read_int32(jarr, idx, (int32_t *)retval, dflval);
}

static inline ufprog_status json_array_read_hex32(struct json_object *jarr, size_t idx, uint32_t *retval,
						  uint32_t dflval)
{
	ufprog_status ret;
	uint64_t val;

	ret = json_array_read_hex64(jarr, idx, &val, dflval);
	if (ret)
		val = 0;

	*retval = (uint32_t)val;
	return ret;
}

ufprog_bool UFPROG_API json_node_exists(struct json_object *jparent, const char *name);
ufprog_bool UFPROG_API json_is_str(struct json_object *jobj, const char *child);
ufprog_bool UFPROG_API json_is_bool(struct json_object *jobj, const char *child);
ufprog_bool UFPROG_API json_is_int(struct json_object *jobj, const char *child);
ufprog_bool UFPROG_API json_is_array(struct json_object *jobj, const char *child);
ufprog_bool UFPROG_API json_is_obj(struct json_object *jobj, const char *child);

ufprog_status UFPROG_API json_node_del(struct json_object *jparent, const char *name);

ufprog_status UFPROG_API json_create_obj(struct json_object **jobj);
ufprog_status UFPROG_API json_create_array(struct json_object **jarr);
ufprog_status UFPROG_API json_put_obj(struct json_object *jobj);

ufprog_status UFPROG_API json_add_str(struct json_object *jparent, const char *name, const char *value, int len);
ufprog_status UFPROG_API json_add_bool(struct json_object *jparent, const char *name, ufprog_bool value);
ufprog_status UFPROG_API json_add_int(struct json_object *jparent, const char *name, int64_t value);
ufprog_status UFPROG_API json_add_uint(struct json_object *jparent, const char *name, uint64_t value);
ufprog_status UFPROG_API json_add_hex(struct json_object *jparent, const char *name, uint64_t value);
ufprog_status UFPROG_API json_add_obj(struct json_object *jparent, const char *name, struct json_object *jobj);

ufprog_status UFPROG_API json_set_str(struct json_object *jparent, const char *name, const char *value, int len);
ufprog_status UFPROG_API json_set_bool(struct json_object *jparent, const char *name, ufprog_bool value);
ufprog_status UFPROG_API json_set_int(struct json_object *jparent, const char *name, int64_t value);
ufprog_status UFPROG_API json_set_uint(struct json_object *jparent, const char *name, uint64_t value);
ufprog_status UFPROG_API json_set_hex(struct json_object *jparent, const char *name, uint64_t value);

ufprog_status UFPROG_API json_array_add_str(struct json_object *jarr, int idx, const char *value, int len);
ufprog_status UFPROG_API json_array_add_bool(struct json_object *jarr, int idx, ufprog_bool value);
ufprog_status UFPROG_API json_array_add_int(struct json_object *jarr, int idx, int64_t value);
ufprog_status UFPROG_API json_array_add_uint(struct json_object *jarr, int idx, uint64_t value);
ufprog_status UFPROG_API json_array_add_hex(struct json_object *jarr, int idx, uint64_t value);
ufprog_status UFPROG_API json_array_add_obj(struct json_object *jarr, int idx, struct json_object *jobj);

ufprog_status UFPROG_API json_array_set_str(struct json_object *jarr, uint32_t idx, const char *value, int len);
ufprog_status UFPROG_API json_array_set_bool(struct json_object *jarr, uint32_t idx, ufprog_bool value);
ufprog_status UFPROG_API json_array_set_int(struct json_object *jarr, uint32_t idx, int64_t value);
ufprog_status UFPROG_API json_array_set_uint(struct json_object *jarr, uint32_t idx, uint64_t value);
ufprog_status UFPROG_API json_array_set_hex(struct json_object *jarr, uint32_t idx, uint64_t value);

typedef int (UFPROG_API *json_obj_item_cb)(void *priv, const char *key, struct json_object *jobj);
ufprog_status UFPROG_API json_obj_foreach(struct json_object *config, const char *subnode, json_obj_item_cb cb,
					  void *priv, size_t *retcount);

typedef int (UFPROG_API *json_array_item_cb)(void *priv, struct json_object *jobj, int index);
ufprog_status UFPROG_API json_array_foreach(struct json_object *config, const char *subnode, json_array_item_cb cb,
					    void *priv, size_t *retcount);

ufprog_status UFPROG_API json_open_config(const char *name, struct json_object **outjroot);
ufprog_status UFPROG_API json_save_config(const char *name, struct json_object *jroot);

EXTERN_C_END

#endif /* _UFPROG_CONFIG_H_ */
