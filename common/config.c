/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Configuration file processing
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <ufprog/config.h>
#include <ufprog/osdef.h>
#include <ufprog/misc.h>
#include <ufprog/log.h>
#include <ufprog/dirs.h>
#include <json-c/json.h>

struct json_open_config_data {
	const char *name;
	struct json_object *jroot;
	ufprog_status ret;
	bool save_new;
};

ufprog_status UFPROG_API json_from_str(const char *str, struct json_object **outjroot)
{
	enum json_tokener_error jerr;
	struct json_object *jobj;

	if (outjroot)
		*outjroot = NULL;

	if (!str || !outjroot) {
		return UFP_INVALID_PARAMETER;
	}

	jobj = json_tokener_parse_verbose(str, &jerr);
	if (!jobj) {
		log_err("Failed to parse JSON data: %s\n", json_tokener_error_desc(jerr));
		return UFP_JSON_DATA_INVALID;
	}

	*outjroot = jobj;
	return UFP_OK;
}

ufprog_status UFPROG_API json_from_file(const char *file, struct json_object **outjroot)
{
	ufprog_status ret;
	char *str;

	if (outjroot)
		*outjroot = NULL;

	if (!file || !outjroot)
		return UFP_INVALID_PARAMETER;

	ret = os_read_text_file(file, &str, NULL);
	if (ret) {
		if (ret == UFP_FILE_NOT_EXIST) {
			log_dbg("File '%s' not exist for JSON loading\n", file);
			return UFP_FILE_NOT_EXIST;
		}

		return UFP_FILE_READ_FAILURE;
	}

	ret = json_from_str(str, outjroot);
	free(str);

	return ret;
}

ufprog_status UFPROG_API json_to_str(struct json_object *jroot, char **outstr)
{
	const char *jstr;

	if (outstr)
		*outstr = NULL;

	if (!outstr || !jroot)
		return UFP_INVALID_PARAMETER;

	jstr = json_object_to_json_string_ext(jroot, JSON_C_TO_STRING_PRETTY |
						     JSON_C_TO_STRING_SPACED);
	if (!jstr) {
		log_err("Failed to format JSON string\n");
		return UFP_JSON_FORMAT_FAILED;
	}

	*outstr = os_strdup(jstr);
	if (!*outstr) {
		log_err("No memory for JSON string\n");
		return UFP_NOMEM;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API json_to_file(struct json_object *jroot, const char *file, ufprog_bool create)
{
	ufprog_status ret;
	const char *jstr;

	if (!file || !jroot)
		return UFP_INVALID_PARAMETER;

	jstr = json_object_to_json_string_ext(jroot, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
	if (!jstr) {
		log_err("Failed to format JSON string\n");
		return UFP_JSON_FORMAT_FAILED;
	}

	ret = write_file_contents(file, jstr, strlen(jstr), create);
	if (ret) {
		if (ret == UFP_FILE_NOT_EXIST) {
			log_dbg("File '%s' not exist for writing JSON\n", file);
			return UFP_FILE_NOT_EXIST;
		}

		return UFP_FILE_WRITE_FAILURE;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API json_free(struct json_object *jroot)
{
	if (!jroot)
		return UFP_INVALID_PARAMETER;

	json_object_put(jroot);
	return UFP_OK;
}

ufprog_status UFPROG_API json_read_str(struct json_object *jparent, const char *name, const char **retstr,
				       const char *dflval)
{
	struct json_object *jobj;

	if (!retstr)
		return UFP_INVALID_PARAMETER;

	*retstr = NULL;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	if (!json_object_object_get_ex(jparent, name, &jobj)) {
		if (dflval) {
			*retstr = dflval;
			return UFP_OK;
		}

		return UFP_NOT_EXIST;
	}

	if (!json_object_is_type(jobj, json_type_string)) {
		log_errdbg("JSON: invalid type of '%s', expect '%s', got '%s'\n", name,
			   json_type_to_name(json_type_string), json_type_to_name(json_object_get_type(jobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retstr = json_object_get_string(jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_read_bool(struct json_object *jparent, const char *name, ufprog_bool *retval)
{
	struct json_object *jobj;

	if (!retval)
		return UFP_INVALID_PARAMETER;

	*retval = false;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	if (!json_object_object_get_ex(jparent, name, &jobj))
		return UFP_OK;

	if (!json_object_is_type(jobj, json_type_boolean)) {
		log_errdbg("JSON: invalid type of '%s', expect '%s', got '%s'\n", name,
			   json_type_to_name(json_type_boolean), json_type_to_name(json_object_get_type(jobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retval = json_object_get_boolean(jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_read_int64(struct json_object *jparent, const char *name, int64_t *retval, int64_t dflval)
{
	struct json_object *jobj;

	if (!retval)
		return UFP_INVALID_PARAMETER;

	*retval = 0;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	if (!json_object_object_get_ex(jparent, name, &jobj)) {
		*retval = dflval;
		return UFP_OK;
	}

	if (!json_object_is_type(jobj, json_type_int)) {
		log_errdbg("JSON: invalid type of '%s', expect '%s', got '%s'\n", name,
			   json_type_to_name(json_type_int), json_type_to_name(json_object_get_type(jobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retval = json_object_get_int64(jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_read_uint64(struct json_object *jparent, const char *name, uint64_t *retval,
					  uint64_t dflval)
{
	struct json_object *jobj;

	if (!retval)
		return UFP_INVALID_PARAMETER;

	*retval = 0;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	if (!json_object_object_get_ex(jparent, name, &jobj)) {
		*retval = dflval;
		return UFP_OK;
	}

	if (!json_object_is_type(jobj, json_type_int)) {
		log_errdbg("JSON: invalid type of '%s', expect '%s', got '%s'\n", name,
			   json_type_to_name(json_type_int), json_type_to_name(json_object_get_type(jobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retval = json_object_get_int64(jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_read_int32(struct json_object *jparent, const char *name, int32_t *retval, int32_t dflval)
{
	struct json_object *jobj;

	if (!retval)
		return UFP_INVALID_PARAMETER;

	*retval = 0;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	if (!json_object_object_get_ex(jparent, name, &jobj)) {
		*retval = dflval;
		return UFP_OK;
	}

	if (!json_object_is_type(jobj, json_type_int)) {
		log_errdbg("JSON: invalid type of '%s', expect '%s', got '%s'\n", name,
			   json_type_to_name(json_type_int), json_type_to_name(json_object_get_type(jobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retval = json_object_get_int(jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_read_hex64(struct json_object *jparent, const char *name, uint64_t *retval,
					 uint64_t dflval)
{
	struct json_object *jobj;
	const char *hexstr;
	char *end;

	if (!retval)
		return UFP_INVALID_PARAMETER;

	*retval = 0;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	if (!json_object_object_get_ex(jparent, name, &jobj)) {
		*retval = dflval;
		return UFP_OK;
	}

	if (json_object_is_type(jobj, json_type_int)) {
		*retval = json_object_get_int64(jobj);
		return UFP_OK;
	} else if (json_object_is_type(jobj, json_type_string)) {
		hexstr = json_object_get_string(jobj);
		*retval = strtoull(hexstr, &end, 16);

		if (*retval == ULLONG_MAX || *end) {
			log_errdbg("JSON: '%s' is not an invalid Hex value of '%s'\n", hexstr,
				   name);
			return UFP_JSON_DATA_INVALID;
		}

		return UFP_OK;
	}

	log_errdbg("JSON: invalid type of '%s', expect '%s' or '%s', got '%s'\n", name,
		   json_type_to_name(json_type_int), json_type_to_name(json_type_string),
		   json_type_to_name(json_object_get_type(jobj)));

	return UFP_JSON_TYPE_INVALID;
}

ufprog_status UFPROG_API json_read_obj(struct json_object *jparent, const char *name, struct json_object **retjobj)
{
	struct json_object *jtmpobj;

	if (!retjobj)
		return UFP_INVALID_PARAMETER;

	*retjobj = NULL;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	if (!json_object_object_get_ex(jparent, name, &jtmpobj))
		return UFP_NOT_EXIST;

	if (!json_object_is_type(jtmpobj, json_type_object)) {
		log_errdbg("JSON: invalid type of '%s', expect '%s', got '%s'\n", name,
			   json_type_to_name(json_type_object), json_type_to_name(json_object_get_type(jtmpobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retjobj = jtmpobj;
	return UFP_OK;
}

ufprog_status UFPROG_API json_read_array(struct json_object *jparent, const char *name, struct json_object **retjarr)
{
	struct json_object *jtmpobj;

	if (!retjarr)
		return UFP_INVALID_PARAMETER;

	*retjarr = NULL;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	if (!json_object_object_get_ex(jparent, name, &jtmpobj))
		return UFP_NOT_EXIST;

	if (!json_object_is_type(jtmpobj, json_type_array)) {
		log_errdbg("JSON: invalid type of '%s', expect '%s', got '%s'\n", name,
			   json_type_to_name(json_type_array), json_type_to_name(json_object_get_type(jtmpobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retjarr = jtmpobj;
	return UFP_OK;
}

size_t UFPROG_API json_obj_len(struct json_object *jobj)
{
	if (!jobj)
		return UFP_INVALID_PARAMETER;

	if (!json_object_is_type(jobj, json_type_object)) {
		log_errdbg("JSON: object is not '%s'\n", json_type_to_name(json_type_object));
		return 0;
	}

	return json_object_object_length(jobj);
}

size_t UFPROG_API json_array_len(struct json_object *jarr)
{
	if (!jarr)
		return UFP_INVALID_PARAMETER;

	if (!json_object_is_type(jarr, json_type_array)) {
		log_errdbg("JSON: object is not '%s'\n", json_type_to_name(json_type_array));
		return 0;
	}

	return json_object_array_length(jarr);
}

ufprog_status UFPROG_API json_array_read_str(struct json_object *jarr, size_t idx, const char **retstr,
					     const char *dflval)
{
	struct json_object *jobj;

	if (!retstr)
		return UFP_INVALID_PARAMETER;

	*retstr = NULL;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_array_get_idx(jarr, idx);
	if (!jobj) {
		if (dflval) {
			*retstr = dflval;
			return UFP_OK;
		}

		return UFP_NOT_EXIST;
	}

	if (!json_object_is_type(jobj, json_type_string)) {
		log_errdbg("JSON: invalid type of array index %u, expect '%s', got '%s'\n", idx,
			   json_type_to_name(json_type_string), json_type_to_name(json_object_get_type(jobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retstr = json_object_get_string(jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_array_read_bool(struct json_object *jarr, size_t idx, ufprog_bool *retval)
{
	struct json_object *jobj;

	if (!retval)
		return UFP_INVALID_PARAMETER;

	*retval = false;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_array_get_idx(jarr, idx);
	if (!jobj)
		return UFP_OK;

	if (!json_object_is_type(jobj, json_type_boolean)) {
		log_errdbg("JSON: invalid type of array index %u, expect '%s', got '%s'\n", idx,
			   json_type_to_name(json_type_boolean), json_type_to_name(json_object_get_type(jobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retval = json_object_get_boolean(jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_array_read_int64(struct json_object *jarr, size_t idx, int64_t *retval, int64_t dflval)
{
	struct json_object *jobj;

	if (!retval)
		return UFP_INVALID_PARAMETER;

	*retval = 0;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_array_get_idx(jarr, idx);
	if (!jobj) {
		*retval = dflval;
		return UFP_OK;
	}

	if (!json_object_is_type(jobj, json_type_int)) {
		log_errdbg("JSON: invalid type of array index %u, expect '%s', got '%s'\n", idx,
			   json_type_to_name(json_type_int), json_type_to_name(json_object_get_type(jobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retval = json_object_get_int64(jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_array_read_uint64(struct json_object *jarr, size_t idx, uint64_t *retval, uint64_t dflval)
{
	struct json_object *jobj;

	if (!retval)
		return UFP_INVALID_PARAMETER;

	*retval = 0;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_array_get_idx(jarr, idx);
	if (!jobj) {
		*retval = dflval;
		return UFP_OK;
	}

	if (!json_object_is_type(jobj, json_type_int)) {
		log_errdbg("JSON: invalid type of array index %u, expect '%s', got '%s'\n", idx,
			   json_type_to_name(json_type_int), json_type_to_name(json_object_get_type(jobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retval = json_object_get_int64(jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_array_read_int32(struct json_object *jarr, size_t idx, int32_t *retval, int32_t dflval)
{
	struct json_object *jobj;

	if (!retval)
		return UFP_INVALID_PARAMETER;

	*retval = 0;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_array_get_idx(jarr, idx);
	if (!jobj) {
		*retval = dflval;
		return UFP_OK;
	}

	if (!json_object_is_type(jobj, json_type_int)) {
		log_errdbg("JSON: invalid type of array index %u, expect '%s', got '%s'\n", idx,
			   json_type_to_name(json_type_int), json_type_to_name(json_object_get_type(jobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retval = json_object_get_int(jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_array_read_hex64(struct json_object *jarr, size_t idx, uint64_t *retval, uint64_t dflval)
{
	struct json_object *jobj;
	const char *hexstr;
	char *end;

	if (!retval)
		return UFP_INVALID_PARAMETER;

	*retval = 0;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_array_get_idx(jarr, idx);
	if (!jobj) {
		*retval = dflval;
		return UFP_OK;
	}

	if (json_object_is_type(jobj, json_type_int)) {
		*retval = json_object_get_int64(jobj);
		return UFP_OK;
	} else if (json_object_is_type(jobj, json_type_string)) {
		hexstr = json_object_get_string(jobj);
		*retval = strtoull(hexstr, &end, 16);

		if (*retval == ULLONG_MAX || *end) {
			log_errdbg("JSON: '%s' is not an invalid Hex value of array index %u'\n", hexstr, idx);
			return UFP_JSON_DATA_INVALID;
		}

		return UFP_OK;
	}

	log_errdbg("JSON: invalid type of array index %u, expect '%s' or '%s', got '%s'\n", idx,
		   json_type_to_name(json_type_int), json_type_to_name(json_type_string),
		   json_type_to_name(json_object_get_type(jobj)));

	return UFP_JSON_TYPE_INVALID;
}

ufprog_status UFPROG_API json_array_read_obj(struct json_object *jarr, size_t idx, struct json_object **retjobj)
{
	struct json_object *jtmpobj;

	if (!retjobj)
		return UFP_INVALID_PARAMETER;

	*retjobj = NULL;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jtmpobj = json_object_array_get_idx(jarr, idx);
	if (!jtmpobj)
		return UFP_NOT_EXIST;

	if (!json_object_is_type(jtmpobj, json_type_object)) {
		log_errdbg("JSON: invalid type of array index %u, expect '%s', got '%s'\n", idx,
			   json_type_to_name(json_type_object), json_type_to_name(json_object_get_type(jtmpobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retjobj = jtmpobj;
	return UFP_OK;
}

ufprog_status UFPROG_API json_array_read_array(struct json_object *jarr, size_t idx, struct json_object **retjarr)
{
	struct json_object *jtmpobj;

	if (!retjarr)
		return UFP_INVALID_PARAMETER;

	*retjarr = NULL;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jtmpobj = json_object_array_get_idx(jarr, idx);
	if (!jtmpobj)
		return UFP_NOT_EXIST;

	if (!json_object_is_type(jtmpobj, json_type_array)) {
		log_errdbg("JSON: invalid type of array index %u, expect '%s', got '%s'\n", idx,
			   json_type_to_name(json_type_array), json_type_to_name(json_object_get_type(jtmpobj)));
		return UFP_JSON_TYPE_INVALID;
	}

	*retjarr = jtmpobj;
	return UFP_OK;
}

ufprog_bool UFPROG_API json_node_exists(struct json_object *jparent, const char *name)
{
	struct json_object *jobj;

	if (!jparent || !name)
		return false;

	if (!json_object_object_get_ex(jparent, name, &jobj))
		return false;

	return true;
}

ufprog_bool UFPROG_API json_is_str(struct json_object *jobj, const char *child)
{
	struct json_object *jchild = jobj;

	if (jobj && child) {
		if (!json_object_object_get_ex(jobj, child, &jchild))
			jchild = NULL;
	}

	if (!jchild)
		return false;

	return json_object_is_type(jchild, json_type_string);
}

ufprog_bool UFPROG_API json_is_bool(struct json_object *jobj, const char *child)
{
	struct json_object *jchild = jobj;

	if (jobj && child) {
		if (!json_object_object_get_ex(jobj, child, &jchild))
			jchild = NULL;
	}

	if (!jchild)
		return false;

	return json_object_is_type(jchild, json_type_boolean);
}

ufprog_bool UFPROG_API json_is_int(struct json_object *jobj, const char *child)
{
	struct json_object *jchild = jobj;

	if (jobj && child) {
		if (!json_object_object_get_ex(jobj, child, &jchild))
			jchild = NULL;
	}

	if (!jchild)
		return false;

	return json_object_is_type(jchild, json_type_int);
}

ufprog_bool UFPROG_API json_is_array(struct json_object *jobj, const char *child)
{
	struct json_object *jchild = jobj;

	if (jobj && child) {
		if (!json_object_object_get_ex(jobj, child, &jchild))
			jchild = NULL;
	}

	if (!jchild)
		return false;

	return json_object_is_type(jchild, json_type_array);
}

ufprog_bool UFPROG_API json_is_obj(struct json_object *jobj, const char *child)
{
	struct json_object *jchild = jobj;

	if (jobj && child) {
		if (!json_object_object_get_ex(jobj, child, &jchild))
			jchild = NULL;
	}

	if (!jchild)
		return false;

	return json_object_is_type(jchild, json_type_object);
}

ufprog_status UFPROG_API json_node_del(struct json_object *jparent, const char *name)
{
	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	json_object_object_del(jparent, name);
	return UFP_OK;
}

ufprog_status UFPROG_API json_create_obj(struct json_object **jobj)
{
	if (!jobj)
		return UFP_INVALID_PARAMETER;

	*jobj = json_object_new_object();
	if (!*jobj)
		return UFP_NOMEM;

	return UFP_OK;
}

ufprog_status UFPROG_API json_create_array(struct json_object **jarr)
{
	if (!jarr)
		return UFP_INVALID_PARAMETER;

	*jarr = json_object_new_array();
	if (!*jarr)
		return UFP_NOMEM;

	return UFP_OK;
}

ufprog_status UFPROG_API json_put_obj(struct json_object *jobj)
{
	if (!jobj)
		return UFP_INVALID_PARAMETER;

	json_object_put(jobj);

	return UFP_OK;
}

ufprog_status UFPROG_API json_add_str(struct json_object *jparent, const char *name, const char *value, int len)
{
	struct json_object *jobj;

	if (!jparent || !name || !value)
		return UFP_INVALID_PARAMETER;

	if (len < 0)
		jobj = json_object_new_string(value);
	else
		jobj = json_object_new_string_len(value, len);

	if (!jobj)
		return UFP_NOMEM;

	json_object_object_add(jparent, name, jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_add_bool(struct json_object *jparent, const char *name, ufprog_bool value)
{
	struct json_object *jobj;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_new_boolean(value);
	if (!jobj)
		return UFP_NOMEM;

	json_object_object_add(jparent, name, jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_add_int(struct json_object *jparent, const char *name, int64_t value)
{
	struct json_object *jobj;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_new_int64(value);
	if (!jobj)
		return UFP_NOMEM;

	json_object_object_add(jparent, name, jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_add_uint(struct json_object *jparent, const char *name, uint64_t value)
{
	struct json_object *jobj;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_new_int64(value);
	if (!jobj)
		return UFP_NOMEM;

	json_object_object_add(jparent, name, jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_add_hex(struct json_object *jparent, const char *name, uint64_t value)
{
	struct json_object *jobj;
	char hexstr[20];

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	snprintf(hexstr, sizeof(hexstr), "%" PRIX64, value);

	jobj = json_object_new_string(hexstr);
	if (!jobj)
		return UFP_NOMEM;

	json_object_object_add(jparent, name, jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_add_obj(struct json_object *jparent, const char *name, struct json_object *jobj)
{
	if (!jparent || !name || !jobj)
		return UFP_INVALID_PARAMETER;

	json_object_object_add(jparent, name, jobj);
	return UFP_OK;
}

ufprog_status UFPROG_API json_set_str(struct json_object *jparent, const char *name, const char *value, int len)
{
	struct json_object *jobj;
	int jret;

	if (!jparent || !name || !value)
		return UFP_INVALID_PARAMETER;

	if (json_object_object_get_ex(jparent, name, &jobj)) {
		if (json_object_is_type(jobj, json_type_string)) {
			if (len < 0)
				jret = json_object_set_string(jobj, value);
			else
				jret = json_object_set_string_len(jobj, value, len);

			if (jret)
				return UFP_OK;

			log_errdbg("JSON: failed to set string value to '%s'\n", name);
			return UFP_FAIL;
		}

		json_object_object_del(jparent, name);
	}

	return json_add_str(jparent, name, value, len);
}

ufprog_status UFPROG_API json_set_bool(struct json_object *jparent, const char *name, ufprog_bool value)
{
	struct json_object *jobj;
	int jret;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	if (json_object_object_get_ex(jparent, name, &jobj)) {
		if (json_object_is_type(jobj, json_type_boolean)) {
			jret = json_object_set_boolean(jobj, value);
			if (jret)
				return UFP_OK;

			log_errdbg("JSON: failed to set boolean value to '%s'\n", name);
			return UFP_FAIL;
		}

		json_object_object_del(jparent, name);
	}

	return json_add_bool(jparent, name, value);
}

ufprog_status UFPROG_API json_set_int(struct json_object *jparent, const char *name, int64_t value)
{
	struct json_object *jobj;
	int jret;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	if (json_object_object_get_ex(jparent, name, &jobj)) {
		if (json_object_is_type(jobj, json_type_int)) {
			jret = json_object_set_int64(jobj, value);
			if (jret)
				return UFP_OK;

			log_errdbg("JSON: failed to set integer value to '%s'\n", name);
			return UFP_FAIL;
		}

		json_object_object_del(jparent, name);
	}

	return json_add_int(jparent, name, value);
}

ufprog_status UFPROG_API json_set_uint(struct json_object *jparent, const char *name, uint64_t value)
{
	struct json_object *jobj;
	int jret;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	if (json_object_object_get_ex(jparent, name, &jobj)) {
		if (json_object_is_type(jobj, json_type_int)) {
			jret = json_object_set_int64(jobj, value);
			if (jret)
				return UFP_OK;

			log_errdbg("JSON: failed to set integer value to '%s'\n", name);
			return UFP_FAIL;
		}

		json_object_object_del(jparent, name);
	}

	return json_add_uint(jparent, name, value);
}

ufprog_status UFPROG_API json_set_hex(struct json_object *jparent, const char *name, uint64_t value)
{
	struct json_object *jobj;
	char hexstr[20];
	int jret;

	if (!jparent || !name)
		return UFP_INVALID_PARAMETER;

	snprintf(hexstr, sizeof(hexstr), "%" PRIX64, value);

	if (json_object_object_get_ex(jparent, name, &jobj)) {
		if (json_object_is_type(jobj, json_type_string)) {
			jret = json_object_set_string(jobj, hexstr);
			if (jret)
				return UFP_OK;

			log_errdbg("JSON: failed to set hex string value to '%s'\n", name);
			return UFP_FAIL;
		}

		json_object_object_del(jparent, name);
	}

	return json_add_str(jparent, name, hexstr, -1);
}


ufprog_status UFPROG_API json_array_add_str(struct json_object *jarr, int idx, const char *value, int len)
{
	struct json_object *jobj;
	int ret;

	if (!jarr || !value)
		return UFP_INVALID_PARAMETER;

	if (len < 0)
		jobj = json_object_new_string(value);
	else
		jobj = json_object_new_string_len(value, len);

	if (!jobj)
		return UFP_NOMEM;

	if (idx < 0)
		ret = json_object_array_add(jarr, jobj);
	else
		ret = json_object_array_put_idx(jarr, idx, jobj);

	if (ret)
		return UFP_NOMEM;

	return UFP_OK;
}

ufprog_status UFPROG_API json_array_add_bool(struct json_object *jarr, int idx, ufprog_bool value)
{
	struct json_object *jobj;
	int ret;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_new_boolean(value);
	if (!jobj)
		return UFP_NOMEM;

	if (idx < 0)
		ret = json_object_array_add(jarr, jobj);
	else
		ret = json_object_array_put_idx(jarr, idx, jobj);

	if (ret)
		return UFP_NOMEM;

	return UFP_OK;
}

ufprog_status UFPROG_API json_array_add_int(struct json_object *jarr, int idx, int64_t value)
{
	struct json_object *jobj;
	int ret;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_new_int64(value);
	if (!jobj)
		return UFP_NOMEM;

	if (idx < 0)
		ret = json_object_array_add(jarr, jobj);
	else
		ret = json_object_array_put_idx(jarr, idx, jobj);

	if (ret)
		return UFP_NOMEM;

	return UFP_OK;
}

ufprog_status UFPROG_API json_array_add_uint(struct json_object *jarr, int idx, uint64_t value)
{
	struct json_object *jobj;
	int ret;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_new_int64(value);
	if (!jobj)
		return UFP_NOMEM;

	if (idx < 0)
		ret = json_object_array_add(jarr, jobj);
	else
		ret = json_object_array_put_idx(jarr, idx, jobj);

	if (ret)
		return UFP_NOMEM;

	return UFP_OK;
}

ufprog_status UFPROG_API json_array_add_hex(struct json_object *jarr, int idx, uint64_t value)
{
	struct json_object *jobj;
	char hexstr[20];
	int ret;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	snprintf(hexstr, sizeof(hexstr), "%" PRIX64, value);

	jobj = json_object_new_string(hexstr);
	if (!jobj)
		return UFP_NOMEM;

	if (idx < 0)
		ret = json_object_array_add(jarr, jobj);
	else
		ret = json_object_array_put_idx(jarr, idx, jobj);

	if (ret)
		return UFP_NOMEM;

	return UFP_OK;
}

ufprog_status UFPROG_API json_array_add_obj(struct json_object *jarr, int idx, struct json_object *jobj)
{
	int ret;

	if (!jarr || !jobj)
		return UFP_INVALID_PARAMETER;

	if (idx < 0)
		ret = json_object_array_add(jarr, jobj);
	else
		ret = json_object_array_put_idx(jarr, idx, jobj);

	if (ret)
		return UFP_NOMEM;

	return UFP_OK;
}

ufprog_status UFPROG_API json_array_set_str(struct json_object *jarr, uint32_t idx, const char *value, int len)
{
	struct json_object *jobj;
	int jret;

	if (!jarr || value)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_array_get_idx(jarr, idx);
	if (!jobj) {
		log_errdbg("JSON: array index %u does not exist\n", idx);
		return UFP_NOT_EXIST;
	}

	if (!json_object_is_type(jobj, json_type_string)) {
		log_errdbg("JSON: array type is not string\n");
		return UFP_JSON_TYPE_INVALID;
	}

	if (len < 0)
		jret = json_object_set_string(jobj, value);
	else
		jret = json_object_set_string_len(jobj, value, len);

	if (jret)
		return UFP_OK;

	return UFP_FAIL;
}

ufprog_status UFPROG_API json_array_set_bool(struct json_object *jarr, uint32_t idx, ufprog_bool value)
{
	struct json_object *jobj;
	int jret;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_array_get_idx(jarr, idx);
	if (!jobj) {
		log_errdbg("JSON: array index %u does not exist\n", idx);
		return UFP_NOT_EXIST;
	}

	if (!json_object_is_type(jobj, json_type_boolean)) {
		log_errdbg("JSON: array type is not boolean\n");
		return UFP_JSON_TYPE_INVALID;
	}

	jret = json_object_set_boolean(jobj, value);
	if (jret)
		return UFP_OK;

	return UFP_FAIL;
}

ufprog_status UFPROG_API json_array_set_int(struct json_object *jarr, uint32_t idx, int64_t value)
{
	struct json_object *jobj;
	int jret;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_array_get_idx(jarr, idx);
	if (!jobj) {
		log_errdbg("JSON: array index %u does not exist\n", idx);
		return UFP_NOT_EXIST;
	}

	if (!json_object_is_type(jobj, json_type_int)) {
		log_errdbg("JSON: array type is not integer\n");
		return UFP_JSON_TYPE_INVALID;
	}

	jret = json_object_set_int64(jobj, value);
	if (jret)
		return UFP_OK;

	return UFP_FAIL;
}

ufprog_status UFPROG_API json_array_set_uint(struct json_object *jarr, uint32_t idx, uint64_t value)
{
	struct json_object *jobj;
	int jret;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_array_get_idx(jarr, idx);
	if (!jobj) {
		log_errdbg("JSON: array index %u does not exist\n", idx);
		return UFP_NOT_EXIST;
	}

	if (!json_object_is_type(jobj, json_type_int)) {
		log_errdbg("JSON: array type is not integer\n");
		return UFP_JSON_TYPE_INVALID;
	}

	jret = json_object_set_int64(jobj, value);
	if (jret)
		return UFP_OK;

	return UFP_FAIL;
}

ufprog_status UFPROG_API json_array_set_hex(struct json_object *jarr, uint32_t idx, uint64_t value)
{
	struct json_object *jobj;
	char hexstr[20];
	int jret;

	if (!jarr)
		return UFP_INVALID_PARAMETER;

	jobj = json_object_array_get_idx(jarr, idx);
	if (!jobj) {
		log_errdbg("JSON: array index %u does not exist\n", idx);
		return UFP_NOT_EXIST;
	}

	if (!json_object_is_type(jobj, json_type_string)) {
		log_errdbg("JSON: array type is not string\n");
		return UFP_JSON_TYPE_INVALID;
	}

	snprintf(hexstr, sizeof(hexstr), "%" PRIX64, value);

	jret = json_object_set_string(jobj, hexstr);
	if (jret)
		return UFP_OK;

	return UFP_FAIL;
}

ufprog_status UFPROG_API json_obj_foreach(struct json_object *config, const char *subnode, json_obj_item_cb cb,
					  void *priv, size_t *retcount)
{
	struct json_object *jobj;
	size_t n = 0;
	int cbret;

	if (!config || !cb)
		return UFP_INVALID_PARAMETER;

	if (!json_object_is_type(config, json_type_object)) {
		log_err("JSON: not an object node\n");
		return UFP_JSON_TYPE_INVALID;
	}

	if (!subnode) {
		jobj = config;
	} else {
		if (!json_object_object_get_ex(config, subnode, &jobj)) {
			log_dbg("JSON: no node named '%' could be found\n", subnode);
			return UFP_NOT_EXIST;
		}

		if (!json_object_is_type(jobj, json_type_object)) {
			log_err("JSON: node '%' is not an object node\n", subnode);
			return UFP_JSON_TYPE_INVALID;
		}
	}

	json_object_object_foreach(jobj, key, jsubnode) {
		n++;

		cbret = cb(priv, key, jsubnode);
		if (cbret)
			break;
	}

	if (retcount)
		*retcount = n;

	return UFP_OK;
}

ufprog_status UFPROG_API json_array_foreach(struct json_object *config, const char *subnode, json_array_item_cb cb,
					    void *priv, size_t *retcount)
{
	struct json_object *arr, *jobj;
	ufprog_status ret;
	uint32_t i;
	int cbret;
	size_t n;

	if (!config || !cb)
		return UFP_INVALID_PARAMETER;

	if (!subnode) {
		if (json_object_is_type(config, json_type_object)) {
			cb(priv, config, -1);

			if (retcount)
				*retcount = 1;

			return UFP_OK;
		}

		if (!json_object_is_type(config, json_type_array)) {
			log_err("JSON: not an object node\n");
			return UFP_JSON_TYPE_INVALID;
		}

		arr = config;
	} else {
		if (!json_object_object_get_ex(config, subnode, &arr)) {
			log_dbg("JSON: no node named '%' could be found\n", subnode);
			return UFP_NOT_EXIST;
		}

		if (json_object_is_type(arr, json_type_object)) {
			cb(priv, arr, -1);

			if (retcount)
				*retcount = 1;

			return UFP_OK;
		}

		if (!json_object_is_type(arr, json_type_array)) {
			log_err("JSON: '%s' is not an object node\n", subnode);
			return UFP_JSON_TYPE_INVALID;
		}
	}

	n = json_array_len(arr);
	if (n > INT32_MAX)
		n = INT32_MAX;

	for (i = 0; i < n; i++) {
		ret = json_array_read_obj(arr, i, &jobj);
		if (ret || !jobj) {
			log_warn("Invalid device connection match#%u\n", i);
			continue;
		}

		cbret = cb(priv, jobj, i);
		if (cbret)
			break;
	}

	if (retcount)
		*retcount = i;

	return UFP_OK;
}

static int UFPROG_API dir_enum_open_config(void *priv, uint32_t index, const char *dir)
{
	struct json_open_config_data *data = priv;
	char *config_path;
	size_t len;

	config_path = path_concat(false, strlen(UFPROG_CONFIG_SUFFIX), dir, data->name, NULL);
	if (!config_path)
		return 0;

	len = strlen(config_path);
	memcpy(config_path + len, UFPROG_CONFIG_SUFFIX, strlen(UFPROG_CONFIG_SUFFIX) + 1);

	log_dbg("Trying to load config '%s'\n", config_path);

	data->ret = json_from_file(config_path, &data->jroot);
	if (data->ret) {
		if (data->ret == UFP_FILE_NOT_EXIST) {
			free(config_path);
			return 0;
		}

		log_errdbg("Failed to load '%s'\n", config_path);
	}

	free(config_path);
	return 1;
}

ufprog_status UFPROG_API json_open_config(const char *name, struct json_object **outjroot)
{
	struct json_open_config_data data;

	if (!name || !outjroot)
		return UFP_INVALID_PARAMETER;

	*outjroot = NULL;

	data.name = name;
	data.jroot = NULL;
	data.ret = UFP_FILE_NOT_EXIST;

	dir_enum(DIR_CONFIG, dir_enum_open_config, &data);

	if (!data.jroot || data.ret) {
		if (data.ret == UFP_FILE_NOT_EXIST)
			log_dbg("No config named '%s' could be opened\n", name);
		else if (!data.ret)
			data.ret = UFP_FAIL;

		return data.ret;
	}

	log_dbg("Opened config '%s'\n", name);

	*outjroot = data.jroot;

	return UFP_OK;
}

static int UFPROG_API dir_enum_save_config(void *priv, uint32_t index, const char *dir)
{
	struct json_open_config_data *data = priv;
	char *config_path;
	size_t len;

	config_path = path_concat(false, strlen(UFPROG_CONFIG_SUFFIX), dir, data->name, NULL);
	if (!config_path)
		return 0;

	len = strlen(config_path);
	memcpy(config_path + len, UFPROG_CONFIG_SUFFIX, strlen(UFPROG_CONFIG_SUFFIX) + 1);

	if (data->save_new)
		log_dbg("Trying to save config to '%s'\n", config_path);

	data->ret = json_to_file(data->jroot, config_path, data->save_new);
	if (data->ret) {
		if (data->ret == UFP_FILE_NOT_EXIST) {
			free(config_path);
			return 0;
		}

		log_errdbg("Failed to save config to '%s'\n", config_path);
	}

	free(config_path);
	return 1;
}

ufprog_status UFPROG_API json_save_config(const char *name, struct json_object *jroot)
{
	struct json_open_config_data data;

	if (!name || !jroot)
		return UFP_INVALID_PARAMETER;

	data.name = name;
	data.jroot = jroot;
	data.ret = UFP_FILE_NOT_EXIST;
	data.save_new = false;

	dir_enum(DIR_CONFIG, dir_enum_save_config, &data);

	if (data.ret) {
		if (data.ret != UFP_FILE_NOT_EXIST) {
			log_dbg("Failed to update config named '%s'\n", name);
			return data.ret;
		}

		data.ret = UFP_FAIL;
		data.save_new = true;

		dir_enum(DIR_CONFIG, dir_enum_save_config, &data);

		if (data.ret) {
			log_dbg("Failed to save config named '%s'\n", name);
			return data.ret;
		}
	}

	return UFP_OK;
}
