/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Path processing
 */

#include <malloc.h>
#include <stdarg.h>
#include <string.h>
#include <ufprog/log.h>
#include <ufprog/dirs.h>

#define DEFAULT_DIR_LIST_CAPACITY		5

struct dir_list {
	uint32_t capacity;
	uint32_t used;
	char **list;
};

static const char *dir_cat_names[] = {
	[DIR_DATA_ROOT] = "Data",
	[DIR_CONFIG] = "Config",
	[DIR_DEVICE] = "Device",
	[DIR_PLUGIN] = "Plugin",
};

static struct dir_list dirs[__MAX_DIR_CATEGORY];
static char *root_dir;

ufprog_bool UFPROG_API uses_portable_dirs(void)
{
#ifdef BUILD_PORTABLE
	return true;
#else
	return false;
#endif
}

ufprog_status UFPROG_API set_root_dir(const char *dir)
{
	if (!dir)
		return UFP_INVALID_PARAMETER;

	if (root_dir)
		free(root_dir);

	root_dir = os_strdup(dir);
	if (!root_dir) {
		log_err("No memory for root dir\n");
		return UFP_NOMEM;
	}

	log_dbg("Setting root directory to '%s'\n", dir);

	return UFP_OK;
}

const char *UFPROG_API get_root_dir(void)
{
	return root_dir;
}

ufprog_status UFPROG_API add_dir(enum dir_category cat, const char *dir)
{
	char **old_list;

	if (cat < 0 || cat >= __MAX_DIR_CATEGORY || !dir)
		return UFP_INVALID_PARAMETER;

	if (!dirs[cat].capacity && dirs[cat].used == dirs[cat].capacity) {
		old_list = dirs[cat].list;

		if (!dirs[cat].list)
			dirs[cat].list = malloc(DEFAULT_DIR_LIST_CAPACITY * sizeof(char *));
		else
			dirs[cat].list = realloc(old_list, DEFAULT_DIR_LIST_CAPACITY * sizeof(char *));

		if (!dirs[cat].list) {
			dirs[cat].list = old_list;
			log_err("No memory for %s dir list\n", dir_cat_names[cat]);
			return UFP_NOMEM;
		}

		memset(&dirs[cat].list[dirs[cat].capacity], 0, DEFAULT_DIR_LIST_CAPACITY * sizeof(char *));
		dirs[cat].capacity += DEFAULT_DIR_LIST_CAPACITY;
	}

	dirs[cat].list[dirs[cat].used] = os_strdup(dir);
	if (!dirs[cat].list[dirs[cat].used]) {
		log_err("No memory for %s dir\n", dir_cat_names[cat]);
		return UFP_NOMEM;
	}

	dirs[cat].used++;

	log_dbg("Adding %s directory: '%s'\n", dir_cat_names[cat], dir);

	return UFP_OK;
}

const char *UFPROG_API get_dir(enum dir_category cat, uint32_t index)
{
	if (cat < 0 || cat >= __MAX_DIR_CATEGORY)
		return NULL;

	if (index >= dirs[cat].used)
		return NULL;

	return dirs[cat].list[index];
}

void UFPROG_API dir_enum(enum dir_category cat, dir_enum_cb cb, void *priv)
{
	uint32_t i;
	int ret;

	if (cat < 0 || cat >= __MAX_DIR_CATEGORY)
		return;

	for (i = 0; i < dirs[cat].used; i++) {
		ret = cb(priv, i, dirs[cat].list[i]);
		if (ret)
			break;
	}
}

char *UFPROG_API path_concat(ufprog_bool end_sep, size_t extra_len, const char *base, ...)
{
	size_t len, new_len = 0, n = 0;
	char *new_path, *p;
	const char *str;
	va_list args;

	if (!base)
		return NULL;

	new_len = strlen(base);

	va_start(args, base);

	do {
		str = va_arg(args, const char *);
		if (str) {
			new_len += strlen(str) + 1;
			n++;
		}
	} while (str);

	va_end(args);

	new_path = calloc(1, new_len + extra_len + 1);
	if (!new_path) {
		log_err("No memory for path concatenating\n");
		return NULL;
	}

	p = new_path;
	len = strlen(base);
	memcpy(p, base, len);
	p += len;

	va_start(args, base);

	do {
		str = va_arg(args, const char *);
		if (str) {
			len = strlen(str);
			memcpy(p, str, len);
			p += len;
			*p++ = PATH_SEP;
		}
	} while (str);

	va_end(args);

	if (!end_sep && n)
		p[-1] = 0;

	return new_path;
}
